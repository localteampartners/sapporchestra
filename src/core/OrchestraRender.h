#pragma once
// Deterministic offline render through the full orchestra chain
// (sampler → dynamics/expression → stage → early reflections → hall).

#include <cstdint>
#include <vector>

#include <sapp/sounds/MidiFile.h>

#include "OrchestraEngine.h"

namespace sapp::orchestra {

struct OrchestraRenderOptions {
    double sampleRate = 48000.0;
    int blockFrames = 512;
    double tailSeconds = 4.0;
    uint32_t seed = 0x5A9F00D5;
    OrchestraParams params;
};

struct OrchestraRenderOutput {
    std::vector<float> left, right;
    double sampleRate = 48000.0;
    float peak = 0.0f;
    float rms = 0.0f;
};

OrchestraRenderOutput renderOrchestra(const sapp::sounds::InstrumentPtr& instrument,
                                      const std::vector<sapp::sounds::TimedMidiEvent>& events,
                                      const OrchestraRenderOptions& options,
                                      int articulationIndex = -1);

} // namespace sapp::orchestra
