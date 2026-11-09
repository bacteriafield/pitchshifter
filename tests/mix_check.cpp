//
// Do several effects really run together in one preset? Renders offline through
// the same VocalChain the program uses: all seven blocks at once must stay
// finite and audible, each block must change the sound, every one of the twelve
// effects must work in the mix, and a saved preset must render identically.
//

#include "core/audio/vocal_chain.h"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

using namespace PCore;

static std::vector<float> render(const Preset& p) {
    VocalChain chain(48000);
    chain.prepare(48000, 512, 1, 1);
    chain.loadPreset(p);

    std::vector<float> out, in(512), o(512);
    size_t n = 0;
    for (int b = 0; b < 400; ++b) {
        for (int i = 0; i < 512; ++i, ++n) {
            // Bursts over a low floor, so the gate and compressor have work to do.
            const float level = (n / 24000) % 2 == 0 ? 0.5f : 0.0003f;
            in[i] = level * static_cast<float>(std::sin(2.0 * 3.14159265358979 * 220.0 * n / 48000.0));
        }
        const float* ip[1] = { in.data() };
        float* op[1] = { o.data() };
        chain.process(ip, op, 512);
        out.insert(out.end(), o.begin(), o.end());
    }
    return out;
}

static double rms(const std::vector<float>& a) {
    double s = 0;
    for (float v : a) s += v * v;
    return std::sqrt(s / a.size());
}

static double difference(const std::vector<float>& a, const std::vector<float>& b) {
    double s = 0;
    for (size_t i = 0; i < a.size(); ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
    return std::sqrt(s / a.size()) / rms(a);
}

static bool sane(const std::vector<float>& a) {
    for (float v : a)
        if (!std::isfinite(v)) return false;
    return rms(a) > 1e-4;
}

static int failures = 0;

static void check(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

int main() {
    const Preset mix{ "All Seven", {
        { "comp.on", 1 }, { "comp.threshold", -24 }, { "comp.ratio", 6 },
        { "voice.on", 1 }, { "voice.type", Voice::Grunge },
        { "eq.on", 1 }, { "eq.band5", 9 },
        { "gate.on", 1 }, { "gate.threshold", -40 },
        { "fx.on", 1 }, { "fx.type", VocalChain::FxTremolo }, { "fx.rate", 7 }, { "fx.depth", 0.9f },
        { "delay.on", 1 }, { "delay.time_ms", 250 }, { "delay.feedback", 0.4f }, { "delay.mix", 0.35f },
        { "reverb.on", 1 }, { "reverb.mix", 0.3f },
    } };

    const auto all = render(mix);
    check(sane(all), "all seven blocks: finite and audible");
    check(difference(all, render(mix)) == 0.0, "rendering is repeatable");

    for (const char* block : { "comp", "voice", "eq", "gate", "fx", "delay", "reverb" }) {
        Preset without = mix;
        without.values[std::string(block) + ".on"] = 0;
        check(difference(all, render(without)) > 0.005, std::string(block) + " changes the sound of the mix");
    }

    for (int t = 0; t < VocalChain::FxTypeCount; ++t) {
        Preset p = mix;
        p.values.erase("fx.rate");
        p.values.erase("fx.depth");
        p.values["fx.type"] = static_cast<float>(t);
        check(sane(render(p)), std::string("the mix works with ") + VocalChain::fxName(t));
    }

    std::stringstream file;
    PresetBank::write(file, { mix });
    std::vector<Preset> parsed;
    check(PresetBank::parse(file, parsed) && parsed.size() == 1, "the preset's text form parses");
    check(!parsed.empty() && difference(all, render(parsed[0])) < 0.001, "a saved preset renders the same");

    std::printf(failures ? "mix check FAILED (%d)\n" : "mix check OK\n", failures);
    return failures != 0;
}
