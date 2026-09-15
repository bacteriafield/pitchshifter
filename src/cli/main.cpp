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

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "core/audio/audio_engine.h"
#include "core/audio/preset.h"
#include "core/audio/effects/voice.h"
#include "io/device_manager.h"
#include "io/port_audio_driver.h"

extern "C" {
    #include "core/logger.h"
}

static const char* kUserPresets = "presets.txt";

// Factory presets first, then user presets: the numbering the commands use.
static std::vector<const PCore::Preset*> allPresets(const PCore::PresetBank& bank) {
    std::vector<const PCore::Preset*> list;
    for (const PCore::Preset& p : PCore::PresetBank::factory()) list.push_back(&p);
    for (const PCore::Preset& p : bank.user()) list.push_back(&p);
    return list;
}

static void printPresets(const PCore::PresetBank& bank) {
    std::cout << "\nPresets:\n";
    const size_t factoryCount = PCore::PresetBank::factory().size();
    const auto list = allPresets(bank);
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << "  " << (i < 10 ? " " : "") << i << "  " << list[i]->name
                  << (i < factoryCount ? "" : "   (user)") << "\n";
    }
}

static void printHelp() {
    std::cout <<
        "\nCommands:\n"
        "  <n>                 load preset n\n"
        "  list                list presets\n"
        "  voices              list voice characters\n"
        "  fx                  list effect types\n"
        "  set <path> <value>  e.g. set voice.type 4, set delay.time_ms 250\n"
        "  save <name>         save the current settings as a user preset\n"
        "  help                this text\n"
        "  quit                stop\n\n";
}

int main() {
    Logger* logger = logger_create("pitchshifter.log", true, true, true);
    logger_set_bitmask(Error | Warning | Info | Debug);

    IO::StreamConfig cfg;
    cfg.sampleRate = 48000;
    cfg.framesPerBlock = 512;
    cfg.inputChannels = 1;
    cfg.outputChannels = 2;

    std::unique_ptr<IO::AudioDriver> drv = std::make_unique<IO::PortAudioDriver>();
    PCore::AudioEngine engine(cfg.sampleRate, cfg.framesPerBlock, cfg.inputChannels, cfg.outputChannels);

    std::string err;
    if (!drv->initialize(cfg, &PCore::AudioEngine::driverCallback, &engine, &err)) {
        logger_log(logger, Error, "Main", "Init", err.c_str());
        std::cerr << "audio init failed: " << err << "\n";
        return -1;
    }
    if (!drv->start(&err)) {
        logger_log(logger, Error, "Main", "Start", err.c_str());
        std::cerr << "audio start failed: " << err << "\n";
        return -1;
    }

    PCore::PresetBank bank;
    std::string loadErr;
    if (bank.load(kUserPresets, &loadErr))
        std::cout << "loaded user presets from " << kUserPresets << "\n";
    else
        logger_log(logger, Info, "Main", "Presets", loadErr.c_str());

    engine.chain().loadPreset(PCore::PresetBank::factory().front());

    char buf[160];
    snprintf(buf, sizeof(buf), "in=%.1fms out=%.1fms", drv->inputLatencySec() * 1000.0,
             drv->outputLatencySec() * 1000.0);
    std::cout << "running, round trip " << buf << "\n";
    std::cout << "preset 0: " << PCore::PresetBank::factory().front().name << "\n";
    printPresets(bank);
    printHelp();

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd.empty()) continue;
        if (cmd == "quit" || cmd == "q") break;

        if (cmd == "help") { printHelp(); continue; }
        if (cmd == "list") { printPresets(bank); continue; }

        if (cmd == "voices") {
            for (int i = 0; i < PCore::Voice::TypeCount; ++i)
                std::cout << "  " << i << "  " << PCore::Voice::typeName(i) << "\n";
            continue;
        }

        if (cmd == "fx") {
            for (int i = 0; i < PCore::VocalChain::FxTypeCount; ++i)
                std::cout << "  " << i << "  " << PCore::VocalChain::fxName(i) << "\n";
            continue;
        }

        if (cmd == "set") {
            std::string path;
            float value = 0.0f;
            if (!(in >> path >> value)) { std::cout << "usage: set <path> <value>\n"; continue; }
            if (!engine.chain().set(path, value)) {
                std::cout << "unknown parameter '" << path << "'\n";
                continue;
            }
            std::cout << path << " = " << value << "\n";
            continue;
        }

        if (cmd == "save") {
            std::string name;
            std::getline(in >> std::ws, name);
            if (name.empty()) { std::cout << "usage: save <name>\n"; continue; }

            std::string saveErr;
            if (bank.store(engine.chain().snapshot(name), &saveErr) && bank.save(kUserPresets, &saveErr))
                std::cout << "saved '" << name << "'\n";
            else
                std::cout << saveErr << "\n";
            continue;
        }

        // Otherwise: a preset number
        try {
            const size_t n = static_cast<size_t>(std::stoul(cmd));
            const auto list = allPresets(bank);
            if (n >= list.size()) { std::cout << "no preset " << n << "\n"; continue; }
            engine.chain().loadPreset(*list[n]);
            std::cout << "preset " << n << ": " << list[n]->name << "\n";
        } catch (const std::exception&) {
            std::cout << "unknown command '" << cmd << "' -- try 'help'\n";
        }
    }

    drv->stop();
    drv->shutdown();
    logger_destroy(logger);
    return 0;
}
