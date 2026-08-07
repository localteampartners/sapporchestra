// SappOrchestraUiShot — renders the plugin editor offscreen and writes a PNG.
// Used to verify UI changes without a screen-recording session.
//   SappOrchestraUiShot [output.png]

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class UiShotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SappOrchestraUiShot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }

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

        const bool showSounds = commandLine.contains("--sounds");
        const bool orchestra = commandLine.contains("--orchestra");
        juce::String pathArg =
            commandLine.replace("--sounds", "").replace("--orchestra", "").trim().unquoted();
        const juce::String outPath = pathArg.isNotEmpty()
            ? pathArg : juce::String("/tmp/sapporchestra-ui.png");

        processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor.reset(processor->createEditor());

        if (orchestra) {
            juce::Timer::callAfterDelay(2500, [this, outPath] {
                processor->loadOrchestraPreset();
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
        if ((processor->isLoading() || occupied < 16) && tries < 240) {
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
