#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace row {
enum class WaterTerrainStatus { Ready, AlignLake, Mismatch, Invalid };
struct WaterTerrainAlignment {
    WaterTerrainStatus status=WaterTerrainStatus::Invalid;
    double offsetM=0,minDeltaM=0,maxDeltaM=0;
};
// All five values must be successful, mapped open-water terrain probes minus
// independently established water ellipsoid heights. Never estimates lake level.
inline WaterTerrainAlignment alignLakeTerrain(const std::array<double,5>& deltas,bool ocean) {
    WaterTerrainAlignment result;
    for(double d:deltas) if(!std::isfinite(d)) return result;
    result.minDeltaM=*std::min_element(deltas.begin(),deltas.end());
    result.maxDeltaM=*std::max_element(deltas.begin(),deltas.end());
    if(result.maxDeltaM<=.03) { result.status=WaterTerrainStatus::Ready; return result; }
    // A nearly horizontal elevated lake sheet can reflect a dataset discrepancy,
    // not a measured new lake height. Bound the display correction. Steep
    // shore/land, large offsets, oceans and unknown samples remain blocking.
    if(!ocean && result.minDeltaM>.03 && result.maxDeltaM<=50 && result.maxDeltaM-result.minDeltaM<=.25) {
        result.status=WaterTerrainStatus::AlignLake;
        result.offsetM=-result.maxDeltaM;
    } else result.status=WaterTerrainStatus::Mismatch;
    return result;
}
}
