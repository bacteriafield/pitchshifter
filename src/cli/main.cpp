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

static void printPresets(const PCore::PresetLibrary& lib) {
    std::cout << "\nPresets:\n";
    const size_t factoryCount = PCore::PresetLibrary::factory().size();
    for (size_t i = 0; i < lib.count(); ++i) {
        std::cout << "  " << (i < 10 ? " " : "") << i << "  " << lib.at(i).name
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

    PCore::PresetLibrary lib;
    std::string loadErr;
    if (lib.load(kUserPresets, &loadErr))
        std::cout << "loaded user presets from " << kUserPresets << "\n";
    else
        logger_log(logger, Info, "Main", "Presets", loadErr.c_str());

    PCore::applyPreset(engine.chain(), lib.at(0));

    char buf[160];
    snprintf(buf, sizeof(buf), "in=%.1fms out=%.1fms", drv->inputLatencySec() * 1000.0,
             drv->outputLatencySec() * 1000.0);
    std::cout << "running, round trip " << buf << "\n";
    std::cout << "preset 0: " << lib.at(0).name << "\n";
    printPresets(lib);
    printHelp();

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd.empty()) continue;
        if (cmd == "quit" || cmd == "q") break;

        if (cmd == "help") { printHelp(); continue; }
        if (cmd == "list") { printPresets(lib); continue; }

        if (cmd == "voices") {
            for (int i = 0; i < PCore::Voice::CharacterCount; ++i)
                std::cout << "  " << i << "  " << PCore::Voice::characterName(i) << "\n";
            continue;
        }

        if (cmd == "fx") {
            for (int i = 0; i < PCore::VocalChain::FxCount; ++i)
                std::cout << "  " << i << "  " << PCore::VocalChain::fxName(i) << "\n";
            continue;
        }

        if (cmd == "set") {
            std::string path;
            float value = 0.0f;
            if (!(in >> path >> value)) { std::cout << "usage: set <path> <value>\n"; continue; }
            if (!PCore::VocalChain::isKnown(path)) {
                std::cout << "unknown parameter '" << path << "'\n";
                continue;
            }
            engine.chain().set(path, value);
            std::cout << path << " = " << value << "\n";
            continue;
        }

        if (cmd == "save") {
            std::string name;
            std::getline(in >> std::ws, name);
            if (name.empty()) { std::cout << "usage: save <name>\n"; continue; }

            lib.addOrReplace(PCore::capturePreset(engine.chain(), name));
            std::string saveErr;
            if (lib.save(kUserPresets, &saveErr)) std::cout << "saved '" << name << "'\n";
            else                                  std::cout << saveErr << "\n";
            continue;
        }

        // Otherwise: a preset number
        try {
            const size_t n = static_cast<size_t>(std::stoul(cmd));
            if (n >= lib.count()) { std::cout << "no preset " << n << "\n"; continue; }
            PCore::applyPreset(engine.chain(), lib.at(n));
            std::cout << "preset " << n << ": " << lib.at(n).name << "\n";
        } catch (const std::exception&) {
            std::cout << "unknown command '" << cmd << "' -- try 'help'\n";
        }
    }

    drv->stop();
    drv->shutdown();
    logger_destroy(logger);
    return 0;
}
