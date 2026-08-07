#pragma once
// "GET SOUNDS" overlay: one-click download + install of curated free sample
// libraries, and a browser of every installed SFZ instrument.
//
// Downloads land in ~/Samples/<library>/ (same layout the CLI tools and
// sappsounds/scripts/fetch-library.sh use). Zips are extracted with
// juce::ZipFile; .tar.gz archives use the system tar (ships with macOS and
// Windows 10+). Everything runs on a background thread with progress; the
// audio thread is never involved.

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace sapporch {

// One downloadable library (possibly several archive parts, like VPO).
struct LibraryDef {
    const char* key;          // install folder name under ~/Samples
    const char* displayName;
    const char* sizeText;
    const char* license;
    const char* kind;         // "zip" or "targz"
    std::vector<const char*> urls;
};

const std::vector<LibraryDef>& soundsRegistry();

class SoundsPanel : public juce::Component, private juce::Timer
{
public:
    explicit SoundsPanel(SappOrchestraProcessor& processor,
                         std::function<void()> onClose);
    ~SoundsPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // Step to the previous/next installed instrument relative to the one the
    // processor currently has loaded (header ◀ ▶ arrows). Scans on demand.
    void stepInstrument(int direction);

    // The samples root folder every Sapp instrument shares. Persisted in
    // ~/Library/Application Support/Sapp/ (or the OS equivalent).
    static juce::File samplesRoot();
    static void setSamplesRoot(const juce::File& root);

private:
    class DownloadJob;
    struct InstalledInstrument {
        juce::File file;
        juce::String label;
    };

    void timerCallback() override;
    void rescanInstruments();          // async, message thread completion
    void applyFilters();
    void chooseFolder();
    void startDownload(int registryIndex);
    bool isInstalled(const LibraryDef& def) const;

    SappOrchestraProcessor& processor_;
    std::function<void()> onClose_;

    juce::Label title_, subtitle_, installedHeader_, statusLabel_, rootLabel_;
    juce::TextButton closeButton_{"CLOSE"};
    juce::TextButton folderButton_{"FOLDER..."};
    juce::OwnedArray<juce::TextButton> downloadButtons_;
    juce::OwnedArray<juce::Label> libraryLabels_;

    class InstrumentListModel;
    std::unique_ptr<InstrumentListModel> listModel_;
    std::unique_ptr<juce::ListBox> instrumentList_;
    juce::TextEditor filterBox_;
    juce::ComboBox categoryBox_;

    std::vector<InstalledInstrument> instruments_, filtered_;
    std::unique_ptr<DownloadJob> job_;
    std::atomic<bool> scanning_{false};
    std::unique_ptr<juce::FileChooser> folderChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsPanel)
};

} // namespace sapporch
