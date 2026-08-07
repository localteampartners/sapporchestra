#pragma once
// In-plugin updater: checks the GitHub latest release, downloads the right
// platform zip, installs it over the current plugin/app, and (standalone
// only) relaunches. Inside a DAW the host owns the loaded binary, so there
// the update is installed on disk and the user reopens the plugin.
//
// macOS: bundles are copied into ~/Library/Audio/Plug-Ins and quarantine is
// cleared (xattr -rc). Windows: a loaded DLL cannot be overwritten but CAN
// be renamed — the old .vst3 is moved aside and the new one takes its place.

#include <juce_audio_utils/juce_audio_utils.h>

#include "../core/VersionCompare.h"

// UiShot and other non-plugin targets compile this header too.
#ifndef JucePlugin_VersionString
#define JucePlugin_VersionString "0.0.0"
#endif

namespace sapporch {

class UpdateManager : private juce::Thread
{
public:
    enum class State { Idle, Checking, UpdateAvailable, Downloading, Installing, Installed, UpToDate, Failed };

    UpdateManager() : juce::Thread("SappUpdate") {}
    ~UpdateManager() override { stopThread(15000); }

    std::function<void()> onStateChanged;  // message thread

    State state() const { return state_.load(); }
    juce::String latestTag() const { const juce::ScopedLock sl(lock_); return latestTag_; }
    juce::String statusText() const { const juce::ScopedLock sl(lock_); return status_; }
    float progress() const { return progress_.load(); }

    // Kick an async check (no-op while busy).
    void checkForUpdate()
    {
        if (isThreadRunning()) return;
        mode_ = Mode::Check;
        setState(State::Checking, "Checking for updates...");
        startThread();
    }

    // Download + install the release found by checkForUpdate().
    void installUpdate()
    {
        if (isThreadRunning() || state() != State::UpdateAvailable) return;
        mode_ = Mode::Install;
        startThread();
    }

private:
    enum class Mode { Check, Install };

    static constexpr const char* kRepo = "localteampartners/sapporchestra";

    void run() override
    {
        if (mode_ == Mode::Check) runCheck();
        else runInstall();
    }

    void runCheck()
    {
        const juce::URL api("https://api.github.com/repos/" + juce::String(kRepo) +
                            "/releases/latest");
        auto stream = api.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders("User-Agent: SappOrchestra-Updater")
                .withConnectionTimeoutMs(15000)
                .withNumRedirectsToFollow(5));
        if (stream == nullptr) { setState(State::Failed, "Update check failed (offline?)"); return; }

        const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
        const auto tag = parsed.getProperty("tag_name", "").toString();
        if (tag.isEmpty()) { setState(State::Failed, "Update check failed"); return; }

        // Pick this platform's asset.
        juce::String wantedSuffix;
#if JUCE_WINDOWS
        wantedSuffix = "-Windows-x64.zip";
#else
        wantedSuffix = "-macOS-universal.zip";
#endif
        juce::String assetUrl, fallbackUrl;
        if (auto* assets = parsed.getProperty("assets", juce::var()).getArray()) {
            for (const auto& asset : *assets) {
                const auto name = asset.getProperty("name", "").toString();
                const auto url = asset.getProperty("browser_download_url", "").toString();
                if (name.endsWith(wantedSuffix)) assetUrl = url;
#if !JUCE_WINDOWS
                if (name.endsWith("-macOS-arm64.zip")) fallbackUrl = url;
#endif
            }
        }
        if (assetUrl.isEmpty()) assetUrl = fallbackUrl;

        {
            const juce::ScopedLock sl(lock_);
            latestTag_ = tag;
            assetUrl_ = assetUrl;
        }
        if (!sapp::orchestra::isNewerVersion(tag.toRawUTF8(), JucePlugin_VersionString))
            setState(State::UpToDate, "Up to date (v" JucePlugin_VersionString ")");
        else if (assetUrl.isEmpty())
            setState(State::Failed, tag + " found but no build for this platform yet");
        else
            setState(State::UpdateAvailable, "Update available: " + tag);
    }

    void runInstall()
    {
        juce::String url, tag;
        {
            const juce::ScopedLock sl(lock_);
            url = assetUrl_;
            tag = latestTag_;
        }
        setState(State::Downloading, "Downloading " + tag + "...");

        const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("sapporchestra-update");
        tempDir.deleteRecursively();
        tempDir.createDirectory();
        const auto zipFile = tempDir.getChildFile("update.zip");

        if (!download(juce::URL(url), zipFile)) { setState(State::Failed, "Download failed"); return; }
        if (threadShouldExit()) return;

        setState(State::Installing, "Installing " + tag + "...");
        juce::ZipFile zip(zipFile);
        if (!zip.uncompressTo(tempDir, true).wasOk()) { setState(State::Failed, "Unpack failed"); return; }

        // The zip contains a single versioned folder with the bundles inside.
        juce::File stage = tempDir;
        for (const auto& dir : tempDir.findChildFiles(juce::File::findDirectories, false))
            if (dir.getFileName().startsWith("SappOrchestra")) { stage = dir; break; }

        bool installedSomething = false;
        juce::String note;
#if JUCE_WINDOWS
        installedSomething = installWindows(stage, tag, note);
#else
        installedSomething = installMac(stage, tag, note);
#endif
        if (!installedSomething) { setState(State::Failed, note.isNotEmpty() ? note : "Install failed"); return; }

        if (juce::JUCEApplicationBase::isStandaloneApp())
            relaunchStandalone(stage);

        setState(State::Installed,
                 tag + " installed" + (note.isNotEmpty() ? " - " + note
                                                          : " - reopen the plugin to load it"));
    }

    bool download(juce::URL url, const juce::File& out)
    {
        auto stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders("User-Agent: SappOrchestra-Updater")
                .withNumRedirectsToFollow(8)
                .withConnectionTimeoutMs(20000));
        if (stream == nullptr) return false;
        juce::FileOutputStream file(out);
        if (!file.openedOk()) return false;
        const auto total = stream->getTotalLength();
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

#if JUCE_WINDOWS
    // Replace the .vst3 the host currently has loaded via the rename trick,
    // and refresh the standalone exe next to it if present.
    bool installWindows(const juce::File& stage, const juce::String& tag, juce::String& note)
    {
        const auto newVst = stage.getChildFile("SappOrchestra.vst3");
        if (!newVst.exists()) { note = "package incomplete"; return false; }

        // Find the currently loaded .vst3 bundle (we live inside it), else
        // fall back to the user-writable standard location.
        auto module = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File target;
        for (auto dir = module; dir.getFullPathName().isNotEmpty();
             dir = dir.getParentDirectory()) {
            if (dir.getFileName().endsWithIgnoreCase(".vst3")) { target = dir; break; }
            if (dir.getParentDirectory() == dir) break;
        }
        if (target == juce::File())
            target = juce::File::getSpecialLocation(juce::File::windowsLocalAppData)
                         .getChildFile("Programs/Common/VST3/SappOrchestra.vst3");

        const auto parked = target.getSiblingFile("SappOrchestra.vst3.old-" + tag);
        parked.deleteRecursively();
        if (target.exists() && !target.moveFileTo(parked)) {
            note = "cannot replace " + target.getFullPathName() + " (permissions)";
            return false;
        }
        target.getParentDirectory().createDirectory();
        if (!newVst.copyDirectoryTo(target)) {
            parked.moveFileTo(target);  // roll back
            note = "copy failed";
            return false;
        }
        note = "restart your DAW to load " + tag;
        return true;
    }
#else
    bool installMac(const juce::File& stage, const juce::String& tag, juce::String& note)
    {
        juce::ignoreUnused(tag);
        const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        bool any = false;
        juce::StringArray installedPaths;

        auto install = [&](const char* bundleName, const juce::File& destDir) {
            const auto source = stage.getChildFile(bundleName);
            if (!source.exists()) return;
            destDir.createDirectory();
            const auto target = destDir.getChildFile(bundleName);
            target.deleteRecursively();
            if (source.copyDirectoryTo(target)) {
                any = true;
                installedPaths.add(target.getFullPathName());
            }
        };
        install("SappOrchestra.vst3", home.getChildFile("Library/Audio/Plug-Ins/VST3"));
        install("SappOrchestra.component", home.getChildFile("Library/Audio/Plug-Ins/Components"));

        // Standalone: stage the new app next to the current one.
        if (juce::JUCEApplicationBase::isStandaloneApp()) {
            const auto newApp = stage.getChildFile("SappOrchestra.app");
            const auto currentApp =
                juce::File::getSpecialLocation(juce::File::currentApplicationFile);
            if (newApp.exists() && currentApp.getFileName().endsWith(".app")) {
                const auto parked = currentApp.getSiblingFile("SappOrchestra.app.old");
                parked.deleteRecursively();
                if (currentApp.moveFileTo(parked) && newApp.copyDirectoryTo(currentApp)) {
                    any = true;
                    installedPaths.add(currentApp.getFullPathName());
                } else {
                    parked.moveFileTo(currentApp);
                }
            }
        }
        if (any) {
            // Downloaded bundles carry the quarantine flag; clear it so the
            // DAW will load them.
            for (const auto& path : installedPaths) {
                juce::ChildProcess xattr;
                xattr.start(juce::StringArray{"/usr/bin/xattr", "-rc", path});
                xattr.waitForProcessToFinish(15000);
            }
        } else {
            note = "package incomplete";
        }
        return any;
    }
#endif

    void relaunchStandalone(const juce::File&)
    {
#if JUCE_MAC
        const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
        juce::MessageManager::callAsync([app] {
            juce::ChildProcess opener;
            opener.start(juce::StringArray{"/usr/bin/open", "-n", app.getFullPathName()});
            juce::JUCEApplicationBase::quit();
        });
#endif
    }

    void setState(State s, const juce::String& text)
    {
        {
            const juce::ScopedLock sl(lock_);
            status_ = text;
        }
        state_.store(s);
        juce::MessageManager::callAsync([this] {
            if (onStateChanged) onStateChanged();
        });
    }

    Mode mode_ = Mode::Check;
    std::atomic<State> state_{State::Idle};
    std::atomic<float> progress_{0.0f};
    mutable juce::CriticalSection lock_;
    juce::String latestTag_, assetUrl_, status_;
};

} // namespace sapporch
