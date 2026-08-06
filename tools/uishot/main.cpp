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

    void initialise(const juce::String& commandLine) override
    {
        const juce::String outPath = commandLine.trim().isNotEmpty()
            ? commandLine.trim().unquoted() : juce::String("/tmp/sapporchestra-ui.png");

        processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor.reset(processor->createEditor());

        // Give the async diagnostic-instrument load and fonts time to settle,
        // then play a chord so the meter/voices are alive in the shot.
        juce::Timer::callAfterDelay(2500, [this, outPath]
        {
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

            juce::Timer::callAfterDelay(300, [this, outPath]
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

    void shutdown() override {}

private:
    std::unique_ptr<sapporch::SappOrchestraProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
};

START_JUCE_APPLICATION(UiShotApp)
