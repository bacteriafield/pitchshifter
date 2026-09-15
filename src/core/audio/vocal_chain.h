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

#ifndef PCORE_AUDIO_VOCAL_CHAIN_H
#define PCORE_AUDIO_VOCAL_CHAIN_H

#include "audio_node.h"
#include "preset.h"
#include "../rt/ring_buffer.h"

#include "effects/chorus.h"
#include "effects/compressor.h"
#include "effects/delay.h"
#include "effects/envelope_filter.h"
#include "effects/eq.h"
#include "effects/flanger.h"
#include "effects/noisegate.h"
#include "effects/phaser.h"
#include "effects/pitchshifter.h"
#include "effects/pixelator.h"
#include "effects/reverb.h"
#include "effects/tremolo.h"
#include "effects/voice.h"

#include <map>
#include <string>
#include <vector>

namespace PCore {
    //
    // The DigiTech Vocal 300 signal path, as drawn in its manual:
    //
    //   Input Level -> Compressor -> Mic Pre/Voice -> EQ -> Noise Gate
    //               -> Effects (one of twelve) -> Delay -> Reverb -> Output Level
    //
    // Seven switchable blocks, which is also where the original's "up to 7
    // effects at once" comes from. Mono inside -- it is a mic box -- and
    // copied to every output.
    //
    // Threading: set() and loadPreset() belong to ONE control thread. They
    // queue changes on a lock-free ring that process() drains at the top of
    // each block, so the audio thread never waits on a lock and never sees a
    // preset applied halfway through a block.
    //
    class VocalChain : public AudioNode {
    public:
        enum FxType {
            FxChorus, FxFlanger, FxPhaser, FxTremolo, FxVibrato, FxStrobe,
            FxDoubler, FxEnvelope, FxPixelator, FxDetune, FxPitch, FxWhammy,
            FxTypeCount
        };

        enum DelayType { DelayDigital, DelayAnalog, DelayTypeCount };

        static const char* fxName(int type);
        static const char* delayName(int type);

        explicit VocalChain(int sampleRate);
        virtual ~VocalChain() = default;

        void prepare(int sampleRate, int maxBlock, int inChans, int outChans) override;
        void process(const float* const* in, float* const* out, unsigned long frames) override;

        // --- control thread only -------------------------------------------
        //
        // "block.param", e.g. "comp.threshold", "fx.type", "delay.on".
        // Blocks: in, comp, voice, eq, gate, fx, delay, reverb, out.
        // Returns false for an unknown block or when the queue is full.
        //
        bool set(const std::string& path, float value);
        float get(const std::string& path, float fallback = 0.0f) const;

        // Reset every block to PresetBank::defaults(), then apply the preset.
        bool loadPreset(const Preset& preset);

        // Everything set since the last load, ready to store as a preset.
        Preset snapshot(const std::string& name) const;

    private:
        struct ParamChange {
            char path[32];
            float value;
        };

        enum Block { BComp, BVoice, BEq, BGate, BFx, BDelay, BReverb, BlockCount };

        int sampleRate_;
        int maxBlock_ = 512;
        int outChans_ = 1;

        float inLevel_  = 1.0f;
        float outLevel_ = 1.0f;
        bool on_[BlockCount] = {};
        int fxType_ = FxChorus;
        float pedal_ = 0.0f;

        Compressor comp_;
        Voice voice_;
        Eq eq_;
        NoiseGate gate_;

        // The Effects block's engines. Several of the twelve types share one.
        Chorus chorus_;
        Flanger flanger_;
        Phaser phaser_;
        Tremolo tremolo_;
        EnvelopeFilter envelope_;
        Pixelator pixelator_;
        PitchShifter pitch_;

        Delay delay_;
        Reverb reverb_;

        SpscRing<ParamChange> queue_;
        std::map<std::string, float> values_;   // control-thread mirror

        std::vector<float> bufA_, bufB_;

        static bool known(const std::string& path);
        void apply(const char* path, float value);   // audio thread
        void setFxType(int type);
        void fxSet(const std::string& key, float value);
        AudioNode& fxNode();
    };
}

#endif // PCORE_AUDIO_VOCAL_CHAIN_H
