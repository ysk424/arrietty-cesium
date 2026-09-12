#include "RowTerrain.h"
#include <cstdlib>
#include <iostream>
#include <limits>

int main() {
    int checks=0;
    auto check=[&](bool ok,const char* message) {
        ++checks;
        if(!ok) { std::cerr<<"FAIL "<<message<<'\n'; std::exit(1); }
    };
    using Status=row::WaterTerrainStatus;
    for(bool ocean:{false,true}) {
        const auto submerged=row::alignLakeTerrain({-80,-2,-.3,-14,-1},ocean);
        check(submerged.status==Status::Ready && submerged.offsetM==0,"submerged terrain never replaces known water height");
        check(row::alignLakeTerrain({0,.03,-.02,.01,0},ocean).status==Status::Ready,"existing three centimetre tolerance is retained");
    }
    const auto lake=row::alignLakeTerrain({35.98,35.97,36.00,35.99,35.96},false);
    check(lake.status==Status::AlignLake && lake.offsetM==-36,"flat elevated lake receives a bounded downward terrain translation");
    check(lake.maxDeltaM+lake.offsetM==0 && lake.minDeltaM+lake.offsetM>=-.25,"aligned probe range is below the unchanged water surface");
    check(row::alignLakeTerrain({35.98,35.97,36,35.99,35.96},true).status==Status::Mismatch,"ocean never receives lake correction");
    check(row::alignLakeTerrain({34.57,35.99,28.69,36,26.90},false).status==Status::Mismatch,"observed steep shoreline samples still block");
    check(row::alignLakeTerrain({50.01,50.01,50.01,50.01,50.01},false).status==Status::Mismatch,"offset beyond fifty metres blocks");
    check(row::alignLakeTerrain({.2,.3,.4,.46,.3},false).status==Status::Mismatch,"non-flat terrain blocks even for a small offset");
    check(row::alignLakeTerrain({-.01,.1,.1,.1,.1},false).status==Status::Mismatch,"water-crossing terrain is not treated as an elevated lake sheet");
    for(double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}) {
        const auto result=row::alignLakeTerrain({36,36,invalid,36,36},false);
        check(result.status==Status::Invalid && result.offsetM==0,"unknown terrain cannot become ready or zero height");
    }
    std::cout<<"PASS "<<checks<<" terrain checks\n";
}
