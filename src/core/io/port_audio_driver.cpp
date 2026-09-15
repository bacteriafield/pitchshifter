/*
    MIT License

    Copyright (c) 2025 Evandro

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/

#include "port_audio_driver.h"
#include "audio_driver.h"
#include <cstring>

#if defined(__linux__)
#include <pa_linux_alsa.h>
#include <sys/resource.h>
#endif
//#include <portaudio.h>

namespace IO {
	static IO::HostApi mapHostApiInternal(PaHostApiTypeId id) {
		switch (id) {
			case paALSA:      return IO::HostApi::ALSA;
			case paCoreAudio: return IO::HostApi::CoreAudio;
			case paJACK:      return IO::HostApi::JACK;
			case paWASAPI:    return IO::HostApi::WASAPI;
			case paASIO:      return IO::HostApi::ASIO;
			case paOSS:       return IO::HostApi::OSS;
			default:          return IO::HostApi::PortAudioMux;
		}
	}

	IO::HostApi PortAudioDriver::mapHostApi(PaHostApiTypeId id) {
		return mapHostApiInternal(id);
	}

    PortAudioDriver::PortAudioDriver() {}
    PortAudioDriver::~PortAudioDriver() { shutdown(); }

    int PortAudioDriver::paCallback(const void *input, void *output, unsigned long frames,
                                    const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *userData) {
        auto *self = static_cast<PortAudioDriver*>(userData);


        const float* inArr[32] = { nullptr };
        float* outArr[32] = { nullptr };
        
        // PortAudio NonInterleaved passes void* that is actually void** (array of pointers)
        // input points to array of const float*
        // output points to array of float*
        
        if (input) {
            const float* const* src = static_cast<const float* const*>(input);
            for (int i = 0; i < self->cfg_.inputChannels && i < 32; ++i) {
                inArr[i] = src[i];
            }
        }
        
        if (output) {
            float* const* dst = static_cast<float* const*>(output);
            for (int i = 0; i < self->cfg_.outputChannels && i < 32; ++i) {
                outArr[i] = dst[i];
            }
        }

        if (self->cb_) {
            CallbackResult result = self->cb_(inArr, outArr, frames, self->userData_);

			switch (result) {
				case CallbackResult::Continue: return paContinue;
				case CallbackResult::Complete: return paComplete;
				case CallbackResult::Abort:    return paAbort;
			}
        }

        // If callback didn't run or fill, we might want to silence. 
        // But with NonInterleaved, silencing means memset each channel.
        // Assuming callback handles it or we rely on PA to not play garbage if we return continue.
        // Actually, safe bet is to zero if callback fails? 
        // AudioGraph::process zeros outputs if inputs are null etc.
        
        return paContinue;
    }

    bool PortAudioDriver::initialize(const StreamConfig &info, RtCallback callback, void *userData, std::string *err) {
        cfg_ = info;
        cb_ = callback;
		userData_ = userData;
        PaError e = Pa_Initialize();

        if (e != paNoError) {
            if (err) *err = Pa_GetErrorText(e);
            return false;
        }

        // Select default devices;
        PaStreamParameters in{}, out{};
        const PaDeviceInfo* inInfo = nullptr; 
        const PaDeviceInfo* outInfo = nullptr;

        if (cfg_.inputChannels > 0) {
            in.device = Pa_GetDefaultInputDevice();
            if (in.device == paNoDevice) { if (err) *err = "No default input device"; return false; }
            inInfo = Pa_GetDeviceInfo(in.device);
            in.channelCount = cfg_.inputChannels;
            in.sampleFormat = paFloat32 | paNonInterleaved;
            in.suggestedLatency = inInfo->defaultHighInputLatency;
        }

        if (cfg_.outputChannels > 0) {
            out.device = Pa_GetDefaultOutputDevice();
            if (out.device == paNoDevice) { if (err) *err = "No default output device"; return false; }
            outInfo = Pa_GetDeviceInfo(out.device);
            out.channelCount = cfg_.outputChannels;
            out.sampleFormat = paFloat32 | paNonInterleaved;
            out.suggestedLatency = outInfo->defaultHighOutputLatency;
        }

        e = Pa_OpenStream(&stream_,
                        cfg_.inputChannels ? &in : nullptr,
                        cfg_.outputChannels ? &out : nullptr,
                        cfg_.sampleRate, static_cast<unsigned long>(cfg_.framesPerBlock), paClipOff,
                        &PortAudioDriver::paCallback, this);
        if (e != paNoError) {
            if (err) *err = Pa_GetErrorText(e);
            return false;
        }

#if defined(__linux__)
        //
        // WSL2, containers and ordinary desktop logins usually have
        // RLIMIT_RTPRIO == 0, so PortAudio's ALSA backend cannot put its
        // callback thread on SCHED_FIFO. Its 1 second startup handshake then
        // trips and Pa_StartStream fails with paTimedOut ("Wait timed out").
        // Ask for normal scheduling rather than failing to start at all.
        //
        {
            const PaDeviceIndex probeDev = cfg_.outputChannels ? out.device : in.device;
            const PaDeviceInfo*  pdi = Pa_GetDeviceInfo(probeDev);
            const PaHostApiInfo* pha = pdi ? Pa_GetHostApiInfo(pdi->hostApi) : nullptr;

            // PaAlsa_EnableRealtimeScheduling blind-casts the stream, so it is
            // only safe on a stream that really is on the ALSA host API.
            if (pha && pha->type == paALSA) {
                struct rlimit rl {};
                if (getrlimit(RLIMIT_RTPRIO, &rl) == 0 && rl.rlim_cur == 0)
                    PaAlsa_EnableRealtimeScheduling(stream_, 0);
            }
        }
#endif

        if (const PaStreamInfo* si = Pa_GetStreamInfo(stream_)) {
            inLatency_  = si->inputLatency;
            outLatency_ = si->outputLatency;
        }

        initialized_ = true;
        return true;
    }

    bool PortAudioDriver::start(std::string* err) {
        if (!initialized_) { if (err) *err = "Not initialized"; return false; }
        PaError e = Pa_StartStream(stream_);
        if (e != paNoError) { if (err) *err = Pa_GetErrorText(e); return false; }
        return true;
    }

    void PortAudioDriver::stop() {
        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }
    }

    void PortAudioDriver::shutdown() {
        stop();
        Pa_Terminate();
        initialized_ = false;
    }

    std::vector<DeviceInfo> PortAudioDriver::listDevices() const {
        std::vector<DeviceInfo> devices;

        PaError pe = Pa_Initialize();
        bool didInit = false;
        if (pe == paNoError) didInit = true;

        int count = Pa_GetDeviceCount();
        if (count < 0) {
            if (didInit) Pa_Terminate();
            return devices;
        }

        devices.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
            if (!di) continue;

            DeviceInfo d;
            d.id = i;
            d.name = di->name ? di->name : "Unknown";
            d.maxInputChannels = di->maxInputChannels;
            d.maxOutputChannels = di->maxOutputChannels;
            d.defaultSampleRate = static_cast<int>(di->defaultSampleRate);

            const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
            d.api = hai ? mapHostApi(hai->type) : HostApi::Unknown;

            devices.push_back(std::move(d));
        }

        if (didInit) Pa_Terminate();
        return devices;
    }

    int PortAudioDriver::defaultInputDeviceId() const {
        PaError pe = Pa_Initialize();
        bool didInit = false;
        if (pe == paNoError) didInit = true;

        PaDeviceIndex idx = Pa_GetDefaultInputDevice();
        int out = (idx == paNoDevice) ? -1 : static_cast<int>(idx);

        if (didInit) Pa_Terminate();
        return out;
    }

    int PortAudioDriver::defaultOutputDeviceId() const {
        PaError pe = Pa_Initialize();
        bool didInit = false;
        if (pe == paNoError) didInit = true;

        PaDeviceIndex idx = Pa_GetDefaultOutputDevice();
        int out = (idx == paNoDevice) ? -1 : static_cast<int>(idx);

        if (didInit) Pa_Terminate();
        return out;
    }
}