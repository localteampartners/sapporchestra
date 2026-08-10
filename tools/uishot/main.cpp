// SappOrchestraUiShot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappOrchestraUiShot [output.png]

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iterator>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappOrchestraUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

    // --sfztest <fixture-root>: headless proof of the `instrument` choice
    // parameter (sapptune issue #20). Copies the fixture library to a temp
    // dir, points SAPP_SFZ_ROOT at it, and asserts: the choice list is the
    // case-insensitively sorted library; selecting choice N loads entry N-1;
    // MIDI bank-select + program change loads by entry index; the chosen SFZ
    // round-trips through host state BY PATH; and no pre-existing parameter
    // moved (the new parameter is appended last).
    int sfzFails = 0;

    void sfzCheck(bool ok, const juce::String& what)
    {
        std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (!ok) ++sfzFails;
    }

    bool pumpUntil(const std::function<bool()>& done, int timeoutMs)
    {
        const auto deadline = juce::Time::getMillisecondCounter() + uint32_t(timeoutMs);
        while (juce::Time::getMillisecondCounter() < deadline) {
            if (done()) return true;
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        }
        return done();
    }

    void runSfzTest(const juce::String& fixtureRoot)
    {
        // Work on a copy so the index-file cache never pollutes tests/data.
        const juce::File src(fixtureRoot);
        const juce::File root =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("sapporch-sfztest");
        root.deleteRecursively();
        if (!src.isDirectory() || !src.copyDirectoryTo(root)) {
            std::printf("FAIL: cannot copy fixture %s\n", fixtureRoot.toRawUTF8());
            setApplicationReturnValue(1);
            quit();
            return;
        }
        ::setenv(sapp::sfzlib::kRootEnvVar, root.getFullPathName().toRawUTF8(), 1);

        processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        processor->prepareToPlay(48000.0, 512);
        pumpUntil([this] { return !processor->isLoading(); }, 8000);  // diagnostic load

        // --- A. parameter table: existing order intact, `instrument` last --
        // `clean` (sapptune #30) and `libraryReady` (sapporchestra #2) are
        // appended AFTER `instrument`, so no pre-existing automation index
        // moves. libraryReady lives outside the APVTS and therefore last.
        const char* expectedIds[] = {"dynamics", "expression", "stageX", "stageDepth",
                                     "width", "earlyLevel", "tailLevel", "hallSize",
                                     "hallDecay", "hallDamping", "legato", "dnaMode",
                                     "dnaAmount", "masterGain", "limiter", "quality",
                                     "articulation", "instrument", "clean", "libraryReady"};
        const auto& params = processor->getParameters();
        bool tableOk = params.size() == int(std::size(expectedIds));
        for (int i = 0; tableOk && i < params.size(); ++i) {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(params[i]);
            tableOk = withId != nullptr && withId->paramID == expectedIds[i];
            if (!tableOk)
                std::printf("  param %d is %s, expected %s\n", i,
                            withId ? withId->paramID.toRawUTF8() : "?", expectedIds[i]);
        }
        sfzCheck(tableOk, "17 pre-existing params unchanged; instrument/clean/libraryReady appended");

        // --- B. choice list mirrors the sorted library ---------------------
        auto* choice = dynamic_cast<juce::AudioParameterChoice*>(
            processor->valueTree().getParameter("instrument"));
        sfzCheck(choice != nullptr, "`instrument` is an AudioParameterChoice");
        if (choice == nullptr) { finishSfzTest(); return; }
        const auto& library = processor->sfzLibrary();
        sfzCheck(library.size() == 3 && choice->choices.size() == 4,
                 "fixture scan: 3 instruments + \"(keep current)\" = 4 choices");
        sfzCheck(choice->choices[0] == "(keep current)", "choice 0 keeps the current sound");
        sfzCheck(choice->choices[1] == "acme/Beta Flute"
                     && choice->choices[2] == "Gamma"
                     && choice->choices[3] == "Zeta Lib/alpha trumpet",
                 "choices sorted case-insensitively (Beta < Gamma < alpha-in-Zeta)");

        // --- C. selecting choice 2 loads entry 1 ("Gamma") -----------------
        choice->setValueNotifyingHost(choice->convertTo0to1(2.0f));
        const juce::String gammaPath = root.getChildFile("Gamma.sfz").getFullPathName();
        sfzCheck(pumpUntil([&] { return processor->currentInstrumentPath() == gammaPath; }, 8000),
                 "choice 2 loaded " + gammaPath);

        // --- D. MIDI bank select + program change --------------------------
        {
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 0), 0);    // bank MSB
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), 1);   // bank LSB
            midi.addEvent(juce::MidiMessage::programChange(1, 0), 2);         // entry 0
            processor->processBlock(buffer, midi);
        }
        const juce::String betaPath =
            root.getChildFile("acme").getChildFile("Beta Flute.sfz").getFullPathName();
        sfzCheck(pumpUntil([&] { return processor->currentInstrumentPath() == betaPath; }, 8000),
                 "bank 0 / program 0 loaded entry 0: " + betaPath);
        sfzCheck(int(choice->getIndex()) == 1,
                 "`instrument` parameter re-synced to choice 1 after program change");

        // --- E. state round-trip BY PATH -----------------------------------
        auto* hallSize = processor->valueTree().getParameter("hallSize");
        hallSize->setValueNotifyingHost(hallSize->convertTo0to1(1.25f));
        juce::MemoryBlock state;
        processor->getStateInformation(state);

        auto second = std::make_unique<sapporch::SappOrchestraProcessor>();
        second->prepareToPlay(48000.0, 512);
        second->setStateInformation(state.getData(), int(state.getSize()));
        sfzCheck(pumpUntil([&] { return second->currentInstrumentPath() == betaPath; }, 8000),
                 "state restore reloaded the chosen SFZ by path");
        auto* hallSize2 = second->valueTree().getParameter("hallSize");
        sfzCheck(std::abs(hallSize2->convertFrom0to1(hallSize2->getValue()) - 1.25f) < 0.01f,
                 "pre-existing parameter (hallSize) recalled identically");
        auto* choice2 = dynamic_cast<juce::AudioParameterChoice*>(
            second->valueTree().getParameter("instrument"));
        sfzCheck(pumpUntil([&] { return choice2->getIndex() == 1; }, 2000),
                 "restored instance re-synced `instrument` to the loaded path");
        second.reset();

        finishSfzTest();
    }

    void finishSfzTest()
    {
        std::printf("sfztest: %s\n", sfzFails == 0 ? "ALL PASS" : "FAILURES");
        processor.reset();
        setApplicationReturnValue(sfzFails == 0 ? 0 : 1);
        quit();
    }

    // --cctest: end-to-end SappLink proof through the PLUGIN path — CC 16
    // (stageX) arrives via processBlock, slews the APVTS parameter exactly
    // like host automation, and must move the source across the stereo stage.
    void runCcTest()
    {
        processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        processor->prepareToPlay(48000.0, 512);
        // The diagnostic instrument loads asynchronously on the message
        // thread; give it time on the normal run loop, then measure.
        juce::Timer::callAfterDelay(2500, [this] { finishCcTest(); });
    }

    void finishCcTest()
    {
        // Kill the room so the direct path dominates the measurement.
        processor->valueTree().getParameter("tailLevel")->setValueNotifyingHost(0.0f);
        processor->valueTree().getParameter("earlyLevel")->setValueNotifyingHost(0.0f);

        juce::AudioBuffer<float> buffer(2, 512);
        auto measure = [&](int ccValue) {
            double energyL = 0.0, energyR = 0.0;
            for (int b = 0; b < 120; ++b) {   // ~1.3 s per side
                juce::MidiBuffer midi;
                if (b == 0) {
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, 16, ccValue), 0);
                    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 1);
                }
                buffer.clear();
                processor->processBlock(buffer, midi);
                if (b > 40) {  // measure after the slew settles
                    for (int i = 0; i < 512; ++i) {
                        energyL += double(buffer.getSample(0, i)) * buffer.getSample(0, i);
                        energyR += double(buffer.getSample(1, i)) * buffer.getSample(1, i);
                    }
                }
            }
            juce::MidiBuffer off;
            off.addEvent(juce::MidiMessage::allNotesOff(1), 0);
            buffer.clear();
            processor->processBlock(buffer, off);
            for (int b = 0; b < 60; ++b) { juce::MidiBuffer none; buffer.clear(); processor->processBlock(buffer, none); }
            return std::pair<double, double>(energyL, energyR);
        };

        const auto [leftL, leftR] = measure(0);      // stageX = -1
        const auto [rightL, rightR] = measure(127);  // stageX = +1
        const bool pass = leftL > leftR * 1.5 && rightR > rightL * 1.5;
        std::printf("SappLink CC16 sweep: cc=0 L/R %.3g/%.3g  cc=127 L/R %.3g/%.3g  [%s]\n",
                    leftL, leftR, rightL, rightR, pass ? "PASS" : "FAIL");
        editor.reset();
        processor.reset();
        setApplicationReturnValue(pass ? 0 : 1);
        quit();
    }

    void initialise(const juce::String& commandLine) override
    {
        if (commandLine.contains("--cctest")) {
            runCcTest();
            return;
        }
        if (commandLine.contains("--sfztest")) {
            const auto fixture =
                commandLine.fromFirstOccurrenceOf("--sfztest", false, false).trim().unquoted();
            runSfzTest(fixture);
            return;
        }

        const bool showSounds = commandLine.contains("--sounds");
        const bool orchestra = commandLine.contains("--orchestra");
        int presetIndex = 0;
        for (int p = 6; p >= 2; --p)
            if (commandLine.contains("--orchestra" + juce::String(p))) presetIndex = p - 1;
        juce::String pathArg = commandLine.replace("--sounds", "")
                                   .replace("--orchestra6", "").replace("--orchestra5", "").replace("--orchestra4", "").replace("--orchestra3", "").replace("--orchestra2", "")
                                   .replace("--orchestra", "")
                                   .trim().unquoted();
        const juce::String outPath = pathArg.isNotEmpty()
            ? pathArg : juce::String("/tmp/sapporchestra-ui.png");

        processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor.reset(processor->createEditor());

        if (orchestra) {
            juce::Timer::callAfterDelay(2500, [this, outPath, presetIndex] {
                processor->loadOrchestraPreset(presetIndex);
                waitForOrchestra(outPath, 0);
            });
            return;
        }

        // Give the async diagnostic-instrument load and fonts time to settle,
        // then play a chord so the meter/voices are alive in the shot.
        juce::Timer::callAfterDelay(2500, [this, outPath, showSounds]
        {
            if (showSounds)
                for (auto* child : editor->getChildren())
                    if (auto* b = dynamic_cast<juce::TextButton*>(child))
                        if (b->getButtonText() == "GET SOUNDS")
                            b->triggerClick();
            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 0);
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.75f), 0);
            for (int i = 0; i < 20; ++i) {
                buffer.clear();
                processor->processBlock(buffer, midi);
                midi.clear();
            }

            juce::Timer::callAfterDelay(showSounds ? 2500 : 300, [this, outPath]
            {
                auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
                juce::File file(outPath);
                file.deleteFile();
                juce::FileOutputStream stream(file);
                juce::PNGImageFormat png;
                if (stream.openedOk() && png.writeImageToStream(snapshot, stream))
                    std::printf("wrote %s (%dx%d)\n", outPath.toRawUTF8(),
                                snapshot.getWidth(), snapshot.getHeight());
                else
                    std::printf("FAILED to write %s\n", outPath.toRawUTF8());
                editor.reset();
                processor.reset();
                quit();
            });
        });
    }

    void waitForOrchestra(juce::String outPath, int tries)
    {
        int occupied = 0;
        for (int i = 0; i < 16; ++i)
            if (processor->slotOccupied(i)) ++occupied;
        if (processor->isLoading() && tries < 240) {
            juce::Timer::callAfterDelay(1000, [this, outPath, tries] {
                waitForOrchestra(outPath, tries + 1);
            });
            return;
        }
        std::printf("orchestra slots occupied: %d\n", occupied);
        auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
        juce::File file(outPath);
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat png;
        if (stream.openedOk() && png.writeImageToStream(snapshot, stream))
            std::printf("wrote %s (%dx%d)\n", outPath.toRawUTF8(),
                        snapshot.getWidth(), snapshot.getHeight());
        editor.reset();
        processor.reset();
        quit();
    }

    void shutdown() override {}

private:
    std::unique_ptr<sapporch::SappOrchestraProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

START_JUCE_APPLICATION(UiShotApp)
