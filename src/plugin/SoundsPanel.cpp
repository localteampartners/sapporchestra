#include "SoundsPanel.h"

#include "PluginEditor.h"  // palette
#include "SappSettings.h"

namespace sapporch {

namespace {
juce::Font panelFont(float h, bool bold = false)
{
    return juce::Font(juce::FontOptions{h, bold ? juce::Font::bold : juce::Font::plain});
}

} // namespace

// ---------------------------------------------------------------- registry --

const std::vector<LibraryDef>& soundsRegistry()
{
    static const std::vector<LibraryDef> registry{
        {"vpo", "Virtual Playing Orchestra 3", "0.7 GB", "Free", "zip", false,
         {"https://virtualplaying.com/go/virtual-playing-orchestra-v3-2-wave-files-archive/",
          "https://virtualplaying.com/go/virtual-playing-orchestra-v3-3-standard-scripts/",
          "https://virtualplaying.com/go/virtual-playing-orchestra-v3-3-performance-scripts/"}},
        {"sonatina", "Sonatina Symphonic Orchestra", "2.6 GB", "CC Sampling Plus", "zip", false,
         {"https://codeload.github.com/peastman/sso/zip/refs/heads/master"}},
        {"vsco2-ce", "VSCO 2 Community Edition", "3.3 GB", "CC0 public domain", "zip", true,
         {"https://codeload.github.com/sgossner/VSCO-2-CE/zip/refs/heads/master",
          "https://codeload.github.com/sgossner/VSCO-2-CE/zip/refs/heads/SFZ"}},
    };
    return registry;
}

// ------------------------------------------------------------ download job --

class SoundsPanel::DownloadJob : public juce::Thread
{
public:
    DownloadJob(LibraryDef def, juce::File destDir, std::function<void(bool)> onDone)
        : juce::Thread("SoundsDownload"),
          def_(std::move(def)), destDir_(destDir), onDone_(std::move(onDone))
    {
    }

    ~DownloadJob() override { stopThread(10000); }

    juce::String status() const
    {
        const juce::ScopedLock sl(lock_);
        return status_;
    }
    float progress() const { return progress_.load(); }

    void run() override
    {
        destDir_.createDirectory();
        bool ok = true;
        for (size_t i = 0; i < def_.urls.size() && ok && !threadShouldExit(); ++i) {
            const auto archive =
                destDir_.getChildFile("download-part" + juce::String(int(i)) +
                                      (juce::String(def_.kind) == "targz" ? ".tar.gz" : ".zip"));
            setStatus("Downloading " + juce::String(int(i + 1)) + "/" +
                      juce::String(int(def_.urls.size())) + "...");
            ok = fetch(juce::URL(def_.urls[i]), archive);
            if (ok && !threadShouldExit()) {
                setStatus("Extracting...");
                ok = extract(archive);
                archive.deleteFile();
            }
        }
        // Some libraries ship samples and mappings as separate archives whose
        // zip roots differ (VSCO2-CE master + SFZ branches): merge every
        // extracted top-level folder into the destination root.
        if (ok && def_.flatten && !threadShouldExit()) {
            setStatus("Merging...");
            for (const auto& child :
                 destDir_.findChildFiles(juce::File::findDirectories, false)) {
                if (!child.getFileName().startsWith("VSCO")) continue;
                mergeInto(child, destDir_);
                child.deleteRecursively();
            }
        }
        const bool success = ok && !threadShouldExit();
        auto callback = onDone_;
        juce::MessageManager::callAsync([callback, success] { callback(success); });
    }

    // Recursive move-merge: files overwrite, directories merge.
    static void mergeInto(const juce::File& src, const juce::File& dst)
    {
        for (const auto& entry : src.findChildFiles(
                 juce::File::findFilesAndDirectories, false)) {
            const auto target = dst.getChildFile(entry.getFileName());
            if (entry.isDirectory()) {
                target.createDirectory();
                mergeInto(entry, target);
            } else {
                target.deleteFile();
                entry.moveFileTo(target);
            }
        }
    }

private:
    bool fetch(juce::URL url, const juce::File& out)
    {
        auto stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withNumRedirectsToFollow(8)
                .withConnectionTimeoutMs(20000));
        if (stream == nullptr) {
            setStatus("Connection failed");
            return false;
        }
        const auto total = stream->getTotalLength();
        out.deleteFile();
        juce::FileOutputStream file(out);
        if (!file.openedOk()) {
            setStatus("Cannot write file");
            return false;
        }
        juce::HeapBlock<char> buffer(1 << 16);
        juce::int64 done = 0;
        while (!stream->isExhausted() && !threadShouldExit()) {
            const int n = stream->read(buffer, 1 << 16);
            if (n <= 0) break;
            file.write(buffer, size_t(n));
            done += n;
            if (total > 0) progress_.store(float(double(done) / double(total)));
        }
        file.flush();
        return done > 0;
    }

    bool extract(const juce::File& archive)
    {
        progress_.store(-1.0f);  // indeterminate
        if (juce::String(def_.kind) == "targz") {
            juce::ChildProcess tar;
            const juce::StringArray args{"tar", "-xzf", archive.getFullPathName(),
                                         "-C", destDir_.getFullPathName()};
            if (!tar.start(args)) return false;
            return tar.waitForProcessToFinish(30 * 60 * 1000) && tar.getExitCode() == 0;
        }
        juce::ZipFile zip(archive);
        return zip.uncompressTo(destDir_, true).wasOk();
    }

    void setStatus(const juce::String& s)
    {
        const juce::ScopedLock sl(lock_);
        status_ = s;
    }

    LibraryDef def_;
    juce::File destDir_;
    std::function<void(bool)> onDone_;
    mutable juce::CriticalSection lock_;
    juce::String status_{"Starting..."};
    std::atomic<float> progress_{0.0f};
};

// ------------------------------------------------------------- list model ---

class SoundsPanel::InstrumentListModel : public juce::ListBoxModel
{
public:
    explicit InstrumentListModel(SoundsPanel& owner) : owner_(owner) {}

    int getNumRows() override { return int(owner_.filtered_.size()); }

    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || size_t(row) >= owner_.filtered_.size()) return;
        if (selected) {
            g.setColour(palette::burgundy.withAlpha(0.55f));
            g.fillRect(0, 0, w, h);
        }
        g.setColour(palette::ivory);
        g.setFont(panelFont(13.0f));
        g.drawText(owner_.filtered_[size_t(row)].label, 8, 0, w - 12, h,
                   juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        if (row < 0 || size_t(row) >= owner_.filtered_.size()) return;
        owner_.processor_.loadSfzInstrument(owner_.filtered_[size_t(row)].file);
        if (owner_.onClose_) owner_.onClose_();
    }

private:
    SoundsPanel& owner_;
};

// ------------------------------------------------------------------ panel ---

SoundsPanel::SoundsPanel(SappOrchestraProcessor& processor, std::function<void()> onClose)
    : processor_(processor), onClose_(std::move(onClose))
{
    title_.setText("Instruments", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions{"Georgia", 26.0f, juce::Font::plain}));
    title_.setColour(juce::Label::textColourId, palette::goldBright);
    addAndMakeVisible(title_);

    subtitle_.setText("Everything in your samples folder, ready to play - plus free "
                      "libraries to download. The folder is shared by all Sapp instruments.",
                      juce::dontSendNotification);
    subtitle_.setFont(panelFont(12.0f));
    subtitle_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(subtitle_);

    for (size_t i = 0; i < soundsRegistry().size(); ++i) {
        const auto& def = soundsRegistry()[i];
        auto* label = libraryLabels_.add(new juce::Label());
        label->setText(juce::String(def.displayName) + "\n" + def.sizeText + " · " + def.license,
                       juce::dontSendNotification);
        label->setFont(panelFont(13.0f));
        label->setColour(juce::Label::textColourId, palette::ivory);
        addAndMakeVisible(label);

        auto* button = downloadButtons_.add(new juce::TextButton("DOWNLOAD"));
        button->onClick = [this, i] { startDownload(int(i)); };
        addAndMakeVisible(button);
    }

    for (int p = 0; p < processor_.orchestraPresetCount(); ++p) {
        auto* b = orchestraButtons_.add(new juce::TextButton(
            "FULL ORCHESTRA - " + processor_.orchestraPresetName(p)));
        b->onClick = [this, p] {
            if (processor_.loadOrchestraPreset(p)) {
                if (onClose_) onClose_();
            } else {
                statusLabel_.setText(processor_.orchestraPresetName(p) +
                                         " not found - download it above first.",
                                     juce::dontSendNotification);
            }
        };
        addAndMakeVisible(b);
    }

    installedHeader_.setText("YOUR INSTRUMENTS - double-click to load",
                             juce::dontSendNotification);
    installedHeader_.setFont(panelFont(10.5f, true));
    installedHeader_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(installedHeader_);

    rootLabel_.setText(samplesRoot().getFullPathName(), juce::dontSendNotification);
    rootLabel_.setFont(panelFont(11.0f));
    rootLabel_.setColour(juce::Label::textColourId, palette::dim);
    rootLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(rootLabel_);

    folderButton_.onClick = [this] { chooseFolder(); };
    addAndMakeVisible(folderButton_);

    categoryBox_.setTextWhenNothingSelected("All categories");
    categoryBox_.onChange = [this] { applyFilters(); };
    addAndMakeVisible(categoryBox_);

    filterBox_.setTextToShowWhenEmpty("filter... (e.g. violin, horn, timpani)", palette::dim);
    filterBox_.setFont(panelFont(13.0f));
    filterBox_.onTextChange = [this] { applyFilters(); };
    addAndMakeVisible(filterBox_);

    listModel_ = std::make_unique<InstrumentListModel>(*this);
    instrumentList_ = std::make_unique<juce::ListBox>("instruments", listModel_.get());
    instrumentList_->setRowHeight(24);
    instrumentList_->setColour(juce::ListBox::backgroundColourId, palette::shadow);
    addAndMakeVisible(*instrumentList_);

    statusLabel_.setFont(panelFont(12.0f));
    statusLabel_.setColour(juce::Label::textColourId, palette::gold);
    addAndMakeVisible(statusLabel_);

    closeButton_.onClick = [this] { if (onClose_) onClose_(); };
    addAndMakeVisible(closeButton_);

    startTimerHz(6);
}

SoundsPanel::~SoundsPanel() { job_.reset(); }

juce::File SoundsPanel::samplesRoot() { return settings::samplesRoot(); }

void SoundsPanel::setSamplesRoot(const juce::File& root) { settings::setSamplesRoot(root); }

void SoundsPanel::chooseFolder()
{
    folderChooser_ = std::make_unique<juce::FileChooser>(
        "Choose your samples folder (scanned for .sfz instruments)", samplesRoot());
    folderChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                    juce::FileBrowserComponent::canSelectDirectories,
                                [this](const juce::FileChooser& chooser) {
                                    const auto dir = chooser.getResult();
                                    if (dir.isDirectory()) {
                                        setSamplesRoot(dir);
                                        rootLabel_.setText(dir.getFullPathName(),
                                                           juce::dontSendNotification);
                                        rescanInstruments();
                                    }
                                });
}

void SoundsPanel::stepInstrument(int direction)
{
    if (instruments_.empty()) {
        rescanInstruments();
        return;
    }
    const juce::String current = processor_.currentInstrumentPath();
    int index = -1;
    for (size_t i = 0; i < instruments_.size(); ++i)
        if (instruments_[i].file.getFullPathName() == current) {
            index = int(i);
            break;
        }
    const int count = int(instruments_.size());
    const int next = index < 0 ? (direction >= 0 ? 0 : count - 1)
                               : ((index + direction) % count + count) % count;
    processor_.loadSfzInstrument(instruments_[size_t(next)].file);
}

bool SoundsPanel::isInstalled(const LibraryDef& def) const
{
    const auto dir = samplesRoot().getChildFile(def.key);
    if (!dir.isDirectory()) return false;
    return !dir.findChildFiles(juce::File::findFiles, true, "*.sfz").isEmpty();
}

void SoundsPanel::visibilityChanged()
{
    if (isVisible()) rescanInstruments();
}

void SoundsPanel::rescanInstruments()
{
    if (scanning_.exchange(true)) return;
    juce::Thread::launch([this] {
        std::vector<InstalledInstrument> found;
        const auto root = samplesRoot();
        for (const auto& file :
             root.findChildFiles(juce::File::findFiles, true, "*.sfz")) {
            const auto path = file.getFullPathName().replaceCharacter('\\', '/');
            if (path.contains("/includes/") || path.contains("/modules/") ||
                path.contains("/Data/"))
                continue;
            InstalledInstrument inst;
            inst.file = file;
            inst.label = file.getRelativePathFrom(root)
                             .replaceCharacter('\\', '/')
                             .dropLastCharacters(4);  // strip ".sfz"
            found.push_back(std::move(inst));
            if (found.size() >= 5000) break;
        }
        std::sort(found.begin(), found.end(),
                  [](const auto& a, const auto& b) { return a.label < b.label; });
        juce::MessageManager::callAsync([this, found = std::move(found)]() mutable {
            instruments_ = std::move(found);

            // Categories = first path component (library), or library/section
            // for two-level layouts. Rebuild the combo, keep "All" first.
            juce::StringArray categories;
            for (const auto& inst : instruments_) {
                auto parts = juce::StringArray::fromTokens(inst.label, "/", "");
                if (parts.size() >= 2)
                    categories.addIfNotAlreadyThere(parts[0]);
                if (parts.size() >= 3)
                    categories.addIfNotAlreadyThere(parts[0] + "/" + parts[1]);
            }
            categories.sort(true);
            const auto previous = categoryBox_.getText();
            categoryBox_.clear(juce::dontSendNotification);
            categoryBox_.addItem("All categories", 1);
            for (int i = 0; i < categories.size(); ++i)
                categoryBox_.addItem(categories[i], i + 2);
            for (int i = 0; i < categoryBox_.getNumItems(); ++i)
                if (categoryBox_.getItemText(i) == previous)
                    categoryBox_.setSelectedItemIndex(i, juce::dontSendNotification);

            applyFilters();
            scanning_.store(false);
        });
    });
}

void SoundsPanel::applyFilters()
{
    const auto needle = filterBox_.getText().toLowerCase();
    const auto category = categoryBox_.getSelectedId() > 1 ? categoryBox_.getText() : juce::String();
    filtered_.clear();
    for (const auto& inst : instruments_) {
        if (category.isNotEmpty() && !inst.label.startsWith(category + "/")) continue;
        if (needle.isNotEmpty() && !inst.label.toLowerCase().contains(needle)) continue;
        filtered_.push_back(inst);
    }
    if (instrumentList_ != nullptr) instrumentList_->updateContent();
}

void SoundsPanel::startDownload(int index)
{
    if (job_ != nullptr) return;  // one at a time
    const auto& def = soundsRegistry()[size_t(index)];
    const auto dest = samplesRoot().getChildFile(def.key);
    statusLabel_.setText("Downloading " + juce::String(def.displayName) + "...",
                         juce::dontSendNotification);
    job_ = std::make_unique<DownloadJob>(def, dest, [this](bool ok) {
        statusLabel_.setText(ok ? "Done - pick an instrument on the right."
                                : "Download failed - check your connection and retry.",
                             juce::dontSendNotification);
        job_.reset();
        rescanInstruments();
        repaint();
    });
    job_->startThread();
}

void SoundsPanel::timerCallback()
{
    installedHeader_.setText("YOUR INSTRUMENTS - double-click to load into channel " +
                                 juce::String(processor_.selectedSlot() + 1),
                             juce::dontSendNotification);
    for (int p = 0; p < orchestraButtons_.size(); ++p)
        orchestraButtons_[p]->setEnabled(processor_.orchestraPresetAvailable(p) &&
                                         job_ == nullptr);

    for (int i = 0; i < downloadButtons_.size(); ++i) {
        const auto& def = soundsRegistry()[size_t(i)];
        if (isInstalled(def)) {
            downloadButtons_[i]->setButtonText("INSTALLED");
            downloadButtons_[i]->setEnabled(false);
        } else {
            downloadButtons_[i]->setEnabled(job_ == nullptr);
        }
    }
    if (job_ != nullptr) {
        const float p = job_->progress();
        statusLabel_.setText(job_->status() +
                                 (p >= 0.0f ? "  " + juce::String(int(p * 100)) + "%" : ""),
                             juce::dontSendNotification);
        repaint();
    }
}

void SoundsPanel::paint(juce::Graphics& g)
{
    g.fillAll(palette::background.withAlpha(0.97f));
    g.setColour(palette::panelEdge);
    g.drawRect(getLocalBounds(), 1);

    // Progress bar under the status line.
    if (job_ != nullptr) {
        const float p = job_->progress();
        auto bar = juce::Rectangle<int>(24, getHeight() - 34, getWidth() / 2 - 48, 8);
        g.setColour(palette::shadow);
        g.fillRoundedRectangle(bar.toFloat(), 3.0f);
        if (p >= 0.0f) {
            g.setColour(palette::gold);
            g.fillRoundedRectangle(bar.toFloat().withWidth(float(bar.getWidth()) * p), 3.0f);
        }
    }
}

void SoundsPanel::resized()
{
    const int w = getWidth();
    title_.setBounds(24, 14, 300, 32);
    subtitle_.setBounds(26, 46, w - 180, 18);
    closeButton_.setBounds(w - 110, 18, 86, 28);

    int y = 84;
    for (int i = 0; i < libraryLabels_.size(); ++i) {
        libraryLabels_[i]->setBounds(24, y, w / 2 - 160, 40);
        downloadButtons_[i]->setBounds(w / 2 - 128, y + 5, 104, 30);
        y += 52;
    }
    for (int p = 0; p < orchestraButtons_.size(); ++p)
        orchestraButtons_[p]->setBounds(24 + p * ((w / 2 - 140) / 2 + 6),
                                        getHeight() - 104,
                                        (w / 2 - 140) / 2, 32);
    statusLabel_.setBounds(24, getHeight() - 62, w / 2 - 48, 22);

    installedHeader_.setBounds(w / 2 + 12, 84, w / 2 - 200, 16);
    folderButton_.setBounds(w - 112, 78, 88, 26);
    rootLabel_.setBounds(w / 2 + 12, getHeight() - 34, w / 2 - 36, 18);
    categoryBox_.setBounds(w / 2 + 12, 104, (w / 2 - 44) / 2, 26);
    filterBox_.setBounds(w / 2 + 20 + (w / 2 - 44) / 2, 104, (w / 2 - 44) / 2, 26);
    instrumentList_->setBounds(w / 2 + 12, 138, w / 2 - 36, getHeight() - 178);
}

} // namespace sapporch
