// sapporchestra-headless — the station harness.
//
// Drives SappOrchestraProcessor exactly the way the sappradio station host
// does: no editor is ever created, and — this is the part that mattered —
// the JUCE dispatch loop is NEVER run. A plugin embedded in a non-JUCE
// headless host has a MessageManager but nothing pumps it, so juce::Timer
// callbacks and MessageManager::callAsync() never fire. Anything the plugin
// needs the message loop for simply does not happen, silently.
//
//   sapporchestra-headless selftest [--root DIR]
//       Regression suite for sapporchestra #1 and #2. Exit 0 = all pass.
//
//   sapporchestra-headless render --instrument LABEL --out FILE
//                                [--root DIR] [--settle MS] [--pump]
//       One station-style render. --pump runs the dispatch loop during the
//       settle window (i.e. pretends to be a JUCE host); the default does
//       not, which is the real station condition.
//
//   sapporchestra-headless index [--root DIR] [--rescan]
//       Build/refresh <root>/.sapp-sfz-index.json with the plugin's own
//       rules, with no editor and no user. See _project/RUNBOOK.md.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace {

void setEnv(const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s(name, value.toRawUTF8());
#else
    if (value.isEmpty()) ::unsetenv(name);
    else ::setenv(name, value.toRawUTF8(), 1);
#endif
}

struct RenderResult {
    std::vector<float> left, right;
    juce::String instrumentPath, instrumentName, status;
    bool libraryReady = false;
    double rms = 0.0, peak = 0.0;
    uint64_t hash = 0;
};

uint64_t audioHash(const std::vector<float>& l, const std::vector<float>& r)
{
    // FNV-1a over the raw sample bits — the "are the two renders byte
    // identical?" check the station reported with md5.
    uint64_t h = 1469598103934665603ull;
    auto feed = [&h](float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        for (int b = 0; b < 4; ++b) {
            h ^= uint64_t((bits >> (b * 8)) & 0xff);
            h *= 1099511628211ull;
        }
    };
    for (float v : l) feed(v);
    for (float v : r) feed(v);
    return h;
}

int choiceForLabel(const sapporch::SappOrchestraProcessor& processor,
                   const juce::String& label)
{
    const auto& library = processor.sfzLibrary();
    for (size_t i = 0; i < library.size(); ++i)
        if (juce::String::fromUTF8(library[i].label.c_str()) == label)
            return int(i) + 1;   // choice 0 = "(keep current)"
    return -1;
}

// One station render. `label` empty = select nothing (the control case).
RenderResult stationRender(const juce::String& label, int settleMs, bool pump,
                           bool* labelResolved = nullptr)
{
    RenderResult out;
    auto processor = std::make_unique<sapporch::SappOrchestraProcessor>();
    processor->prepareToPlay(48000.0, 512);

    if (label.isNotEmpty()) {
        const int choice = choiceForLabel(*processor, label);
        if (labelResolved != nullptr) *labelResolved = choice > 0;
        if (choice > 0) {
            auto* parameter = processor->valueTree().getParameter("instrument");
            // Exactly what a host does with a display name: normalized write.
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(choice)));
        }
    } else if (labelResolved != nullptr) {
        *labelResolved = true;
    }

    // Settle window. The station passes --settle 20000 and does NOT pump a
    // JUCE dispatch loop; --pump models a JUCE-based host instead.
    const auto deadline = juce::Time::getMillisecondCounter() + uint32_t(settleMs);
    while (juce::Time::getMillisecondCounter() < deadline) {
        if (processor->libraryReady()) break;
        if (pump) juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        else juce::Thread::sleep(10);
    }

    out.instrumentPath = processor->currentInstrumentPath();
    out.instrumentName = processor->currentInstrumentName();
    out.status = processor->loadStatus();
    out.libraryReady = processor->libraryReady();

    // ~2.5 s of audio: one held note plus a tail.
    constexpr int kBlock = 512;
    constexpr int kBlocks = 235;                 // ~2.5 s at 48 kHz
    juce::AudioBuffer<float> buffer(2, kBlock);
    for (int b = 0; b < kBlocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        if (b == 140) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        buffer.clear();
        processor->processBlock(buffer, midi);
        out.left.insert(out.left.end(), buffer.getReadPointer(0),
                        buffer.getReadPointer(0) + kBlock);
        out.right.insert(out.right.end(), buffer.getReadPointer(1),
                         buffer.getReadPointer(1) + kBlock);
    }

    double sum = 0.0;
    for (size_t i = 0; i < out.left.size(); ++i) {
        sum += double(out.left[i]) * out.left[i] + double(out.right[i]) * out.right[i];
        out.peak = std::max(out.peak, double(std::abs(out.left[i])));
        out.peak = std::max(out.peak, double(std::abs(out.right[i])));
    }
    out.rms = std::sqrt(sum / double(out.left.size() * 2));
    out.hash = audioHash(out.left, out.right);
    processor.reset();
    return out;
}

// --------------------------------------------------------------- selftest --

int fails = 0;

void check(bool ok, const juce::String& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    std::fflush(stdout);
    if (!ok) ++fails;
}

juce::File prepareFixture(const juce::String& fixtureRoot, const juce::String& name)
{
    const juce::File source(fixtureRoot);
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile(name);
    root.deleteRecursively();
    if (!source.isDirectory() || !source.copyDirectoryTo(root)) {
        std::printf("FAIL: cannot copy fixture %s\n", fixtureRoot.toRawUTF8());
        ++fails;
        return {};
    }
    // "Clean state" means clean: anything that ran against the fixture in
    // place (a CLI smoke test, an earlier run) may have left an index behind.
    root.getChildFile(sapp::sfzlib::kIndexFileName).deleteFile();
    return root;
}

int runSelftest(const juce::String& fixtureRoot)
{
    const auto root = prepareFixture(fixtureRoot, "sapporch-headless");
    if (!root.isDirectory()) return 1;

    // ---- issue #1: the index builds headlessly from a clean state ---------
    std::printf("sapporchestra #1 — headless SFZ index\n");
    const auto indexFile = root.getChildFile(sapp::sfzlib::kIndexFileName);
    check(!indexFile.existsAsFile(), "fixture starts with no index file");
    setEnv(sapp::sfzlib::kRootEnvVar, root.getFullPathName());
    {
        auto processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        check(indexFile.existsAsFile(),
              "constructing the plugin wrote the index with no GUI and no user");
        check(processor->sfzLibrary().size() == 3,
              "3 playable instruments enumerated (fragments/empty excluded)");
        check(!indexFile.loadFileAsString().startsWith(juce::String::fromUTF8("\xef\xbb\xbf")),
              "index is UTF-8 with no BOM");
        processor.reset();
    }
    {
        // A library that grew after the index was written must be picked up
        // without anybody opening the editor (SAPP_SFZ_RESCAN=1).
        const auto added = root.getChildFile("zzz-new-library");
        added.createDirectory();
        root.getChildFile("sine.wav").copyFileTo(added.getChildFile("sine.wav"));
        added.getChildFile("Late Arrival.sfz")
            .replaceWithText("<region> sample=sine.wav lokey=0 hikey=127 "
                             "pitch_keycenter=60 transpose=-12\n");
        auto stale = std::make_unique<sapporch::SappOrchestraProcessor>();
        check(stale->sfzLibrary().size() == 3,
              "without a rescan the plugin keeps the indexed list (fast path)");
        stale.reset();

        setEnv(sapporch::kRescanEnvVar, "1");
        auto fresh = std::make_unique<sapporch::SappOrchestraProcessor>();
        check(fresh->sfzLibrary().size() == 4,
              "SAPP_SFZ_RESCAN=1 rebuilt the index at construction (4 entries)");
        fresh.reset();
        setEnv(sapporch::kRescanEnvVar, "");
        added.deleteRecursively();
        // Leave the index describing the tree as it is now.
        setEnv(sapporch::kRescanEnvVar, "1");
        std::make_unique<sapporch::SappOrchestraProcessor>().reset();
        setEnv(sapporch::kRescanEnvVar, "");
    }

    // ---- issue #2: selected vs unselected renders must differ -------------
    std::printf("sapporchestra #2 — the `instrument` parameter must load the SFZ\n");
    const juce::String labelA = "loud/Loud Sine";
    const juce::String labelB = "quiet/Quiet Octave Down";

    bool resolved = false;
    const auto selected = stationRender(labelA, 4000, /*pump=*/false, &resolved);
    const auto unselected = stationRender({}, 4000, /*pump=*/false);
    check(resolved, "the label resolved to a choice index");
    check(selected.instrumentPath ==
              root.getChildFile("loud").getChildFile("Loud Sine.sfz").getFullPathName(),
          "the selected SFZ is the one that loaded: " + selected.instrumentPath);
    check(selected.hash != unselected.hash,
          "selected and unselected renders DIFFER (station repro: they were identical)");
    check(selected.rms > 1.0e-4, "the selected instrument actually sounded");
    check(selected.libraryReady, "libraryReady reads 1 once the selection is loaded");

    // The named instrument is the one that sounded: a second, deliberately
    // different instrument must produce a third distinct render.
    const auto other = stationRender(labelB, 4000, /*pump=*/false);
    check(other.instrumentPath ==
              root.getChildFile("quiet").getChildFile("Quiet Octave Down.sfz").getFullPathName(),
          "the second label loaded its own SFZ");
    check(other.hash != selected.hash && other.hash != unselected.hash,
          "a different selection renders differently again");
    check(other.rms < selected.rms * 0.5,
          "the quiet instrument really is the quiet one (it is what sounded)");

    // A JUCE-style host that does pump the loop must still work.
    const auto pumped = stationRender(labelA, 4000, /*pump=*/true);
    check(pumped.hash == selected.hash,
          "pumping the message loop changes nothing (same render)");

    // ---- a multi-slot state restore must install EVERY slot ---------------
    // A 16-channel session queues sixteen loads at once. The supersede guard
    // is per SLOT for exactly this reason: one global generation would let
    // only the last of them land.
    {
        auto settle = [](sapporch::SappOrchestraProcessor& p) {
            const auto deadline = juce::Time::getMillisecondCounter() + 8000u;
            while (juce::Time::getMillisecondCounter() < deadline && p.isLoading())
                juce::Thread::sleep(10);   // still no dispatch loop anywhere
        };

        auto saved = std::make_unique<sapporch::SappOrchestraProcessor>();
        saved->prepareToPlay(48000.0, 512);
        saved->loadSfzInstrumentIntoSlot(
            root.getChildFile("loud").getChildFile("Loud Sine.sfz"), 0);
        saved->loadSfzInstrumentIntoSlot(
            root.getChildFile("quiet").getChildFile("Quiet Octave Down.sfz"), 1);
        saved->loadSfzInstrumentIntoSlot(
            root.getChildFile("perc").getChildFile("Bright Fifth.sfz"), 2);
        settle(*saved);
        juce::MemoryBlock state;
        saved->getStateInformation(state);
        saved.reset();

        auto restored = std::make_unique<sapporch::SappOrchestraProcessor>();
        restored->prepareToPlay(48000.0, 512);
        restored->setStateInformation(state.getData(), int(state.getSize()));
        settle(*restored);
        check(restored->slotName(0) == "Loud Sine"
                  && restored->slotName(1) == "Quiet Octave Down"
                  && restored->slotName(2) == "Bright Fifth",
              "a 3-slot state restore installed all three slots headlessly");
        restored.reset();
    }

    std::printf("selftest: %s\n", fails == 0 ? "ALL PASS" : "FAILURES");
    return fails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String command = argc > 1 ? juce::String(argv[1]) : juce::String();
    juce::String root, instrument, out, fixture;
    int settleMs = 4000;
    bool pump = false, rescan = false;
    for (int i = 2; i < argc; ++i) {
        const juce::String arg(argv[i]);
        auto next = [&]() -> juce::String { return i + 1 < argc ? juce::String(argv[++i]) : juce::String(); };
        if (arg == "--root") root = next();
        else if (arg == "--fixture") fixture = next();
        else if (arg == "--instrument") instrument = next();
        else if (arg == "--out") out = next();
        else if (arg == "--settle") settleMs = next().getIntValue();
        else if (arg == "--pump") pump = true;
        else if (arg == "--rescan") rescan = true;
    }

    if (command == "selftest") {
        if (fixture.isEmpty()) fixture = root;
#ifdef SAPPORCH_TEST_DATA_DIR
        if (fixture.isEmpty()) fixture = juce::String(SAPPORCH_TEST_DATA_DIR) + "/sfz-headless";
#endif
        return runSelftest(fixture);
    }

    if (command == "index") {
        if (root.isNotEmpty())
            setEnv(sapp::sfzlib::kRootEnvVar, root);
        if (rescan)
            setEnv(sapporch::kRescanEnvVar, "1");
        auto processor = std::make_unique<sapporch::SappOrchestraProcessor>();
        std::printf("root:  %s\n", sapporch::samplesRootForLibrary().toRawUTF8());
        std::printf("index: %s\n", sapporch::indexFilePath().toRawUTF8());
        std::printf("count: %d\n", int(processor->sfzLibrary().size()));
        processor.reset();
        return 0;
    }

    if (command == "render") {
        if (root.isNotEmpty())
            setEnv(sapp::sfzlib::kRootEnvVar, root);
        const auto result = stationRender(instrument, settleMs, pump);
        std::printf("instrument: %s\n", result.instrumentPath.toRawUTF8());
        std::printf("name:       %s\n", result.instrumentName.toRawUTF8());
        std::printf("status:     %s\n", result.status.toRawUTF8());
        std::printf("ready:      %d\n", result.libraryReady ? 1 : 0);
        std::printf("rms:        %.6f\n", result.rms);
        std::printf("peak:       %.6f\n", result.peak);
        std::printf("hash:       %016llx\n", (unsigned long long) result.hash);
        if (out.isNotEmpty()) {
            juce::File file(out);
            file.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
            if (stream != nullptr) {
                std::unique_ptr<juce::AudioFormatWriter> writer(
                    wav.createWriterFor(stream.get(), 48000.0, 2, 24, {}, 0));
                if (writer != nullptr) {
                    stream.release();
                    const float* channels[2] = {result.left.data(), result.right.data()};
                    writer->writeFromFloatArrays(channels, 2, int(result.left.size()));
                }
            }
            std::printf("wrote:      %s\n", out.toRawUTF8());
        }
        return 0;
    }

    std::fprintf(stderr,
                 "sapporchestra-headless — station harness (no GUI, no message loop)\n"
                 "  sapporchestra-headless selftest [--fixture DIR]\n"
                 "  sapporchestra-headless index    [--root DIR] [--rescan]\n"
                 "  sapporchestra-headless render   --instrument LABEL [--out F.wav]\n"
                 "                                  [--root DIR] [--settle MS] [--pump]\n");
    return 2;
}
