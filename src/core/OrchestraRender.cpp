#include "OrchestraRender.h"

#include <algorithm>
#include <cmath>

#include "SappLinkCCMap.h"

namespace sapp::orchestra {

using sapp::sounds::MidiEvent;
using sapp::sounds::TimedMidiEvent;

OrchestraRenderOutput renderOrchestra(const sapp::sounds::InstrumentPtr& instrument,
                                      const std::vector<TimedMidiEvent>& events,
                                      const OrchestraRenderOptions& options,
                                      int articulationIndex)
{
    OrchestraRenderOutput out;
    out.sampleRate = options.sampleRate;
    if (!instrument) return out;

    OrchestraEngine engine;
    engine.prepare(options.sampleRate, options.blockFrames);
    engine.reseed(options.seed);
    engine.setParams(options.params);
    engine.setInstrument(instrument);
    if (articulationIndex >= 0) engine.selectArticulation(articulationIndex);

    double lastEvent = 0.0;
    for (const auto& e : events) lastEvent = std::max(lastEvent, e.seconds);
    const uint64_t totalFrames =
        uint64_t((lastEvent + std::max(0.5, options.tailSeconds)) * options.sampleRate) + 1;

    out.left.assign(size_t(totalFrames), 0.0f);
    out.right.assign(size_t(totalFrames), 0.0f);

    std::vector<TimedMidiEvent> sorted = events;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto& a, const auto& b) { return a.seconds < b.seconds; });

    std::vector<MidiEvent> block;
    block.reserve(256);
    size_t next = 0;

    // SappLink CC-in: mapped controllers steer OrchestraParams during the
    // render, exactly as they steer the plugin's parameters live. The CC
    // event is still forwarded to the sampler (SFZ locc/hicc conditions,
    // engine-native CC1/CC11/CC64 behavior).
    OrchestraParams liveParams = options.params;

    for (uint64_t frame = 0; frame < totalFrames; frame += uint64_t(options.blockFrames)) {
        const int frames = int(std::min<uint64_t>(uint64_t(options.blockFrames), totalFrames - frame));
        block.clear();
        bool paramsChanged = false;
        const double blockEnd = double(frame + uint64_t(frames)) / options.sampleRate;
        while (next < sorted.size() && sorted[next].seconds < blockEnd) {
            const auto& e = sorted[next];
            MidiEvent m;
            m.frame = uint32_t(std::clamp(int64_t(e.seconds * options.sampleRate) - int64_t(frame),
                                          int64_t(0), int64_t(frames - 1)));
            switch (e.status) {
                case 0x90: m.type = MidiEvent::Type::NoteOn; m.note = e.data1; m.value = e.data2; break;
                case 0x80: m.type = MidiEvent::Type::NoteOff; m.note = e.data1; break;
                case 0xB0:
                    m.type = MidiEvent::Type::Controller; m.note = e.data1; m.value = e.data2;
                    paramsChanged |= sapplink::applyCcToParams(liveParams, e.data1, e.data2);
                    break;
                case 0xE0: m.type = MidiEvent::Type::PitchBend; m.bend14 = e.bend14; break;
                default: ++next; continue;
            }
            block.push_back(m);
            ++next;
        }
        if (paramsChanged)
            engine.setParams(liveParams);
        engine.process(block.data(), int(block.size()),
                       out.left.data() + frame, out.right.data() + frame, frames);
    }

    double sumSq = 0.0;
    for (size_t i = 0; i < out.left.size(); ++i) {
        out.peak = std::max({out.peak, std::abs(out.left[i]), std::abs(out.right[i])});
        sumSq += double(out.left[i]) * out.left[i] + double(out.right[i]) * out.right[i];
    }
    out.rms = float(std::sqrt(sumSq / double(std::max<size_t>(1, out.left.size() * 2))));
    return out;
}

} // namespace sapp::orchestra
