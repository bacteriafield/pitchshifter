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

#include "vocal_chain.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <string_view>

namespace PCore {

    static const char* const kFxNames[VocalChain::FxTypeCount] = {
        "Chorus", "Flanger", "Phaser", "Tremolo", "Vibrato", "Strobe",
        "Doubler", "Envelope", "Pixelator", "Detune", "Pitch Shifter", "Whammy",
    };

    static const char* const kDelayNames[VocalChain::DelayTypeCount] = {
        "Digital", "Analog",
    };

    const char* VocalChain::fxName(int type) {
        return kFxNames[std::clamp(type, 0, FxTypeCount - 1)];
    }

    const char* VocalChain::delayName(int type) {
        return kDelayNames[std::clamp(type, 0, DelayTypeCount - 1)];
    }

    VocalChain::VocalChain(int sampleRate)
        : sampleRate_(sampleRate),
          comp_(sampleRate), voice_(sampleRate), eq_(sampleRate), gate_(sampleRate),
          chorus_(sampleRate), flanger_(sampleRate), phaser_(sampleRate), tremolo_(sampleRate),
          envelope_(sampleRate), pixelator_(sampleRate), pitch_(sampleRate),
          delay_(sampleRate), reverb_(sampleRate),
          queue_(1024) {}

    void VocalChain::prepare(int sampleRate, int maxBlock, int /*inChans*/, int outChans) {
        sampleRate_ = sampleRate;
        maxBlock_   = std::max(1, maxBlock);
        outChans_   = std::max(1, outChans);

        AudioNode* const nodes[] = {
            &comp_, &voice_, &eq_, &gate_,
            &chorus_, &flanger_, &phaser_, &tremolo_, &envelope_, &pixelator_, &pitch_,
            &delay_, &reverb_,
        };
        for (AudioNode* n : nodes)
            n->prepare(sampleRate, maxBlock_, 1, 1);

        bufA_.assign(maxBlock_, 0.0f);
        bufB_.assign(maxBlock_, 0.0f);
        setFxType(fxType_);
    }

    void VocalChain::process(const float* const* in, float* const* out, unsigned long frames) {
        ParamChange change;
        while (queue_.pop(change))
            apply(change.path, change.value);

        if (!out) return;
        const float* src = (in && in[0]) ? in[0] : nullptr;

        for (unsigned long done = 0; done < frames;) {
            const unsigned long n = std::min<unsigned long>(frames - done, static_cast<unsigned long>(maxBlock_));
            float* a = bufA_.data();
            float* b = bufB_.data();

            for (unsigned long i = 0; i < n; ++i)
                a[i] = src ? src[done + i] * inLevel_ : 0.0f;

            // Each block reads `a` and writes `b`, then they swap -- so no
            // effect ever has to be safe to run in place.
            auto run = [&](AudioNode& node) {
                const float* ip[1] = { a };
                float* op[1] = { b };
                node.process(ip, op, n);
                std::swap(a, b);
            };

            // ponytail: blocks switch on and off with no crossfade, so toggling
            // one mid-phrase can click. Add a short gain ramp per block (like
            // AudioNoise's do_effect_step) if that turns out to be audible.
            if (on_[BComp])   run(comp_);
            if (on_[BVoice])  run(voice_);
            if (on_[BEq])     run(eq_);
            if (on_[BGate])   run(gate_);
            if (on_[BFx])     run(fxNode());
            if (on_[BDelay])  run(delay_);
            if (on_[BReverb]) run(reverb_);

            for (int c = 0; c < outChans_; ++c) {
                if (!out[c]) continue;
                for (unsigned long i = 0; i < n; ++i)
                    out[c][done + i] = a[i] * outLevel_;
            }
            done += n;
        }
    }

    bool VocalChain::known(const std::string& path) {
        static const char* const kBlocks[] = {
            "in.", "out.", "comp.", "voice.", "eq.", "gate.", "fx.", "delay.", "reverb.",
        };
        if (path.size() >= sizeof(ParamChange::path)) return false;
        for (const char* b : kBlocks) {
            const size_t len = std::strlen(b);
            if (path.size() > len && path.compare(0, len, b) == 0)
                return true;
        }
        return false;
    }

    bool VocalChain::set(const std::string& path, float value) {
        if (!known(path)) return false;

        ParamChange change {};
        std::memcpy(change.path, path.c_str(), path.size() + 1);
        change.value = value;
        if (!queue_.push(change)) return false;

        // A new type loads its own settings, so the old type's knob tweaks no
        // longer describe what is running. Forget them.
        if (path == "fx.type" || path == "voice.type") {
            const std::string prefix = path.substr(0, path.find('.') + 1);
            for (auto it = values_.begin(); it != values_.end();) {
                const bool inBlock = it->first.compare(0, prefix.size(), prefix) == 0;
                const std::string key = inBlock ? it->first.substr(prefix.size()) : std::string();
                const bool keep = !inBlock || key == "on" || key == "type" || key == "pedal" ||
                                  (prefix == "voice." && key == "mix");
                it = keep ? std::next(it) : values_.erase(it);
            }
        }

        values_[path] = value;
        return true;
    }

    float VocalChain::get(const std::string& path, float fallback) const {
        const auto it = values_.find(path);
        return it == values_.end() ? fallback : it->second;
    }

    static bool isTypeKey(const std::string& key) {
        return key.size() > 5 && key.compare(key.size() - 5, 5, ".type") == 0;
    }

    bool VocalChain::loadPreset(const Preset& preset) {
        std::map<std::string, float> merged = PresetBank::defaults();
        for (const auto& kv : preset.values)
            merged[kv.first] = kv.second;

        values_.clear();

        // Types first: selecting a type loads its recipe, which the preset's
        // own knob values must then be allowed to override.
        bool ok = true;
        for (const auto& kv : merged)
            if (isTypeKey(kv.first)) ok = set(kv.first, kv.second) && ok;
        for (const auto& kv : merged)
            if (!isTypeKey(kv.first)) ok = set(kv.first, kv.second) && ok;
        return ok;
    }

    Preset VocalChain::snapshot(const std::string& name) const {
        return Preset{ name, values_ };
    }

    void VocalChain::apply(const char* path, float v) {
        const std::string_view p(path);
        const size_t dot = p.find('.');
        if (dot == std::string_view::npos) return;

        const std::string_view block = p.substr(0, dot);

        // ponytail: every parameter name is 15 chars or fewer, so this string
        // lives in the small-string buffer and never allocates on the audio
        // thread. A longer name would -- keep them short.
        const std::string key(p.substr(dot + 1));

        if (key == "on") {
            const bool on = v >= 0.5f;
            if (block == "comp")        on_[BComp]   = on;
            else if (block == "voice")  on_[BVoice]  = on;
            else if (block == "eq")     on_[BEq]     = on;
            else if (block == "gate")   on_[BGate]   = on;
            else if (block == "fx")     on_[BFx]     = on;
            else if (block == "delay")  on_[BDelay]  = on;
            else if (block == "reverb") on_[BReverb] = on;
            return;
        }

        if (block == "in") {
            if (key == "level") inLevel_ = dsp::dbToLevel(std::clamp(v, -24.0f, 24.0f));
        }
        else if (block == "out") {
            if (key == "level") outLevel_ = dsp::dbToLevel(std::clamp(v, -24.0f, 24.0f));
        }
        else if (block == "comp") {
            comp_.setParameters(key, v);
        }
        else if (block == "voice") {
            if (key == "type") voice_.setType(static_cast<int>(std::lround(v)));
            else               voice_.setParameters(key, v);
        }
        else if (block == "eq") {
            eq_.setParameters(key, v);
        }
        else if (block == "gate") {
            gate_.setParameters(key, v);
        }
        else if (block == "fx") {
            if (key == "type") {
                setFxType(static_cast<int>(std::lround(v)));
            } else if (key == "pedal") {
                pedal_ = std::clamp(v, 0.0f, 1.0f);
                if (fxType_ == FxWhammy) pitch_.setParameters("octave", pedal_);
            } else {
                fxSet(key, v);
            }
        }
        else if (block == "delay") {
            if (key == "type")
                delay_.setParameters("tone", std::lround(v) == DelayAnalog ? 0.3f : 1.0f);
            else
                delay_.setParameters(key, v);
        }
        else if (block == "reverb") {
            reverb_.setParameters(key, v);
        }
    }

    void VocalChain::setFxType(int type) {
        fxType_ = std::clamp(type, 0, FxTypeCount - 1);

        // Selecting a type loads that type's settings onto its engine; any
        // fx.* values the preset carries are applied on top afterwards.
        switch (fxType_) {
        case FxChorus:
            chorus_.setParameters("lfo_rate_hz", 0.8f);
            chorus_.setParameters("depth_ms", 6.0f);
            chorus_.setParameters("base_delay_ms", 15.0f);
            chorus_.setParameters("mix", 0.4f);
            break;
        case FxVibrato:     // all wet, so only the pitch wobble is heard
            chorus_.setParameters("lfo_rate_hz", 5.0f);
            chorus_.setParameters("depth_ms", 2.0f);
            chorus_.setParameters("base_delay_ms", 2.0f);
            chorus_.setParameters("mix", 1.0f);
            break;
        case FxDoubler:     // a slow, shallow second voice ~25ms behind
            chorus_.setParameters("lfo_rate_hz", 0.15f);
            chorus_.setParameters("depth_ms", 1.0f);
            chorus_.setParameters("base_delay_ms", 25.0f);
            chorus_.setParameters("mix", 0.5f);
            break;
        case FxFlanger:
            flanger_.setParameters("rate", 0.5f);
            flanger_.setParameters("delay", 2.0f);
            flanger_.setParameters("depth", 0.7f);
            flanger_.setParameters("feedback", 0.5f);
            flanger_.setParameters("mix", 0.5f);
            break;
        case FxPhaser:
            phaser_.setParameters("rate", 0.5f);
            phaser_.setParameters("freq", 1000.0f);
            phaser_.setParameters("octaves", 1.0f);
            phaser_.setParameters("q", 0.7f);
            phaser_.setParameters("feedback", 0.5f);
            phaser_.setParameters("mix", 1.0f);
            break;
        case FxTremolo:
            tremolo_.setParameters("rate", 5.0f);
            tremolo_.setParameters("depth", 0.7f);
            tremolo_.setParameters("waveform", 0.0f);   // sine
            break;
        case FxStrobe:      // square tremolo at full depth: the voice chops
            tremolo_.setParameters("rate", 8.0f);
            tremolo_.setParameters("depth", 1.0f);
            tremolo_.setParameters("waveform", 2.0f);   // square
            break;
        case FxEnvelope:
            envelope_.setParameters("min_hz", 250.0f);
            envelope_.setParameters("max_hz", 3000.0f);
            envelope_.setParameters("sensitivity", 6.0f);
            envelope_.setParameters("q", 2.0f);
            envelope_.setParameters("attack", 8.0f);
            envelope_.setParameters("release", 120.0f);
            envelope_.setParameters("mix", 1.0f);
            break;
        case FxPixelator:
            pixelator_.setParameters("bits", 6.0f);
            pixelator_.setParameters("downsample", 8.0f);
            pixelator_.setParameters("mix", 1.0f);
            break;
        case FxDetune:      // ~15 cents, blended, to thicken without a pitch jump
            pitch_.setParameters("semitones", 0.15f);
            pitch_.setParameters("mix", 0.5f);
            break;
        case FxPitch:
            pitch_.setParameters("semitones", -12.0f);
            pitch_.setParameters("mix", 0.5f);
            break;
        case FxWhammy:      // pitch follows fx.pedal, 0 .. +1 octave
            pitch_.setParameters("octave", pedal_);
            pitch_.setParameters("mix", 1.0f);
            break;
        default:
            break;
        }
    }

    void VocalChain::fxSet(const std::string& key, float value) {
        switch (fxType_) {
        case FxChorus: case FxVibrato: case FxDoubler: chorus_.setParameters(key, value);    break;
        case FxFlanger:                                flanger_.setParameters(key, value);   break;
        case FxPhaser:                                 phaser_.setParameters(key, value);    break;
        case FxTremolo: case FxStrobe:                 tremolo_.setParameters(key, value);   break;
        case FxEnvelope:                               envelope_.setParameters(key, value);  break;
        case FxPixelator:                              pixelator_.setParameters(key, value); break;
        default:                                       pitch_.setParameters(key, value);     break;
        }
    }

    AudioNode& VocalChain::fxNode() {
        switch (fxType_) {
        case FxChorus: case FxVibrato: case FxDoubler: return chorus_;
        case FxFlanger:                                return flanger_;
        case FxPhaser:                                 return phaser_;
        case FxTremolo: case FxStrobe:                 return tremolo_;
        case FxEnvelope:                               return envelope_;
        case FxPixelator:                              return pixelator_;
        default:                                       return pitch_;
        }
    }
}
