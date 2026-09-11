#pragma once
#include "RowCore.h"
#include <array>

namespace row {
// Read-only sound envelope; never feeds back into propulsion or calibration.
struct AudioMix {
    enum Track { Catch, Pull, Boat, Water, Wind, Count };
    static constexpr double SourcePeak=.5;
    static constexpr std::array<double,Count> MaxGain{.42,.34,.42,.20,.13};
    std::array<double,Count> gain{};
    double boatPitch=.88;
    bool catchNow=false,wasRunning=false;
    unsigned lastStroke=0;

    void tick(const Model& model,double dt) {
        const bool running=model.state==State::Running;
        catchNow=running && wasRunning && model.strokes>lastStroke;
        lastStroke=model.strokes; wasRunning=running;
        const double step=std::isfinite(dt)?std::clamp(dt,0.,.1):0.;
        const double speed=running && std::isfinite(model.speed)?std::clamp(model.speed/5.5,0.,1.):0.;
        const double drive=running && std::isfinite(model.drive)?std::clamp(model.drive,0.,1.):0.;
        const std::array<double,Count> target{MaxGain[Catch],MaxGain[Pull]*drive,
            MaxGain[Boat]*std::pow(speed,.8),MaxGain[Water]*(1-.3*speed),MaxGain[Wind]};
        for(size_t i=0;i<gain.size();++i) {
            // Quick drive attack, gentle recovery and ambient startup.
            const double tau=i==Pull?(target[i]>gain[i]?.045:.12):i==Boat?.35:1.5;
            gain[i]+=(target[i]-gain[i])*(1-std::exp(-step/tau));
        }
        gain[Catch]=MaxGain[Catch];
        // Pausing, lost tracking and setup silence exercise layers immediately.
        // The audio component performs a short de-click fade before stopping.
        if(!running) gain[Pull]=gain[Boat]=0;
        boatPitch=.88+.24*speed;
    }
};
}
