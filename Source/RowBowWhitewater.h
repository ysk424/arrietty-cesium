#pragma once
#include "RowWaves.h"

namespace row {
// Artistic bow breaking/entrained-air layer, separate from the Kelvin solver.
// Coordinates and speed are metres and m/s. The actual hull tip is +2.30 m.
class BowWhitewater {
public:
    float amount=0,length=0,width=0,rate=0;
    explicit BowWhitewater(float speed) {
        if(!std::isfinite(speed)) return;
        const float t=std::clamp((speed-.4f)/2.2f,0.f,1.f);
        amount=t*t*(3-2*t);
        length=.9f+1.1f*amount;
        width=.13f+.10f*amount;
        rate=amount*(12.f+8.f*std::clamp(speed,0.f,3.f));
    }
    float coverage(float along,float across) const {
        if(amount<=0 || along<2.4f-length || along>2.7f || std::abs(across)>1.5f) return 0;
        const float aft=2.4f-along;
        const float t=std::clamp((aft-length*.4f)/(length*.6f),0.f,1.f);
        const float taper=1-t*t*(3-2*t);
        // Two narrow shoulders outside the waterline, joined just ahead of the
        // tip. Avoid filling the masked hull or painting a solid white disk.
        const float lateral=(std::abs(across)-(.07f+.48f*std::max(0.f,aft)))/width;
        const float front=std::min(0.f,aft)/.13f;
        return taper*std::exp(-.5f*(lateral*lateral+front*front));
    }
    float crest(float along,float across,double time) const {
        const float mask=coverage(along,across);
        if(mask==0) return 0;
        // At most 2.2 cm of local visual crest: independent of hull/camera pose.
        // The same displaced surface supplies normals in RowWater::Upload.
        const float churn=.8f+.2f*float(std::sin(time*9-along*7+std::abs(across)*11));
        return .022f*amount*mask*churn;
    }
    void emit(Waves& waves,double x,double y,double heading,float dt) const {
        if(amount<=0 || !std::isfinite(dt) || dt<=0 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(heading)) return;
        const double fx=std::cos(heading),fy=std::sin(heading);
        const double gx=(x+fx*1.7-waves.originX)/Waves::Cell,gy=(y+fy*1.7-waves.originY)/Waves::Cell;
        if(gx<0||gx>=Waves::N||gy<0||gy>=Waves::N) return;
        const int cx=int(std::round(gx)),cy=int(std::round(gy));
        constexpr int radius=10;
        for(int j=std::max(1,cy-radius);j<std::min(Waves::N-1,cy+radius+1);++j)
            for(int i=std::max(1,cx-radius);i<std::min(Waves::N-1,cx+radius+1);++i) {
                const double dx=waves.originX+i*Waves::Cell-x,dy=waves.originY+j*Waves::Cell-y;
                const float mask=coverage(float(dx*fx+dy*fy),float(-dx*fy+dy*fx));
                if(mask==0) continue;
                float& foam=waves.bowFoam[j*Waves::N+i];
                // Rate-based saturation: the same elapsed time creates the
                // same foam at 30, 60 or 90 FPS; it stays in world space.
                foam=.85f-(.85f-foam)*std::exp(-rate*mask*dt);
            }
    }
};
}
