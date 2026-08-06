#include "SappLinkCCMap.h"

#include <algorithm>
#include <cmath>

namespace sapp::orchestra::sapplink {

// CC assignment follows the SappLink conventions (PROTOCOL.md): standard MMA
// CCs where one exists (7 volume, 91 reverb send), free CCs 14–31 otherwise.
// Ranges are the plugin's real APVTS ranges — the manifest mirrors these.
const std::array<CCMapping, kNumMappings>& mappings()
{
    static const std::array<CCMapping, kNumMappings> table { {
        { 7,  "masterGain",  &OrchestraParams::masterGainDb, -24.0f, 12.0f, Curve::Linear },
        { 14, "earlyLevel",  &OrchestraParams::earlyLevel,   0.0f,   1.0f,  Curve::Linear },
        { 15, "hallDecay",   &OrchestraParams::hallDecay,    0.3f,   12.0f, Curve::Log },
        { 16, "stageX",      &OrchestraParams::stageX,       -1.0f,  1.0f,  Curve::Linear },
        { 17, "stageDepth",  &OrchestraParams::stageDepth,   0.0f,   1.0f,  Curve::Linear },
        { 18, "width",       &OrchestraParams::width,        0.0f,   2.0f,  Curve::Linear },
        { 19, "hallDamping", &OrchestraParams::hallDamping,  0.0f,   1.0f,  Curve::Linear },
        { 26, "dnaAmount",   &OrchestraParams::dnaAmount,    0.0f,   1.0f,  Curve::Linear },
        { 91, "tailLevel",   &OrchestraParams::tailLevel,    0.0f,   1.0f,  Curve::Linear },
        { 92, "hallSize",    &OrchestraParams::hallSize,     0.2f,   1.5f,  Curve::Linear },
    } };
    return table;
}

const CCMapping* findMapping(int cc)
{
    for (const auto& mapping : mappings())
        if (mapping.cc == cc)
            return &mapping;
    return nullptr;
}

float ccToEngineering(const CCMapping& mapping, int ccValue)
{
    const float t = float(std::clamp(ccValue, 0, 127)) / 127.0f;
    if (mapping.curve == Curve::Log)
        return mapping.lo * std::pow(mapping.hi / mapping.lo, t);
    return mapping.lo + (mapping.hi - mapping.lo) * t;
}

bool applyCcToParams(OrchestraParams& params, int cc, int ccValue)
{
    const auto* mapping = findMapping(cc);
    if (mapping == nullptr)
        return false;
    params.*(mapping->field) = ccToEngineering(*mapping, ccValue);
    return true;
}

} // namespace sapp::orchestra::sapplink
