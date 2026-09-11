#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
namespace row {
// A 64 m moving local water domain. Global coordinates survive domain shifts.
// Damped 2D wave equation, fixed 1/60s, absorbing border; CFL c*dt/dx < 1/sqrt(2).
class Waves {
public:
    static constexpr int N=256;
    static constexpr float Cell=.25f,Step=1.f/60.f;
    double originX=-32,originY=-32;
    std::vector<float> height,previous,foam,bowFoam;
    Waves():height(N*N),previous(N*N),foam(N*N),bowFoam(N*N),next(N*N),scratch(N*N) {}
    void center(double x,double y) {
        const double nx=std::floor((x-32)/Cell)*Cell,ny=std::floor((y-32)/Cell)*Cell;
        const int dx=int(std::round((nx-originX)/Cell)),dy=int(std::round((ny-originY)/Cell));
        if(!dx&&!dy) return;
        auto shift=[&](std::vector<float>& a) {
            std::fill(scratch.begin(),scratch.end(),0.f);
            for(int j=0;j<N;++j) for(int i=0;i<N;++i) {
                const int sx=i+dx,sy=j+dy;
                if(sx>=0&&sx<N&&sy>=0&&sy<N) scratch[j*N+i]=a[sy*N+sx];
            }
            a.swap(scratch);
        };
        shift(height); shift(previous); shift(foam); shift(bowFoam); originX=nx; originY=ny;
    }
    void disturb(double x,double y,float amplitude,float radius,float bubbles) {
        const int cx=int(std::round((x-originX)/Cell)),cy=int(std::round((y-originY)/Cell));
        const int r=int(std::ceil(radius*3/Cell));
        for(int j=std::max(2,cy-r);j<std::min(N-2,cy+r+1);++j)
            for(int i=std::max(2,cx-r);i<std::min(N-2,cx+r+1);++i) {
                const float px=float(originX+i*Cell-x),py=float(originY+j*Cell-y);
                const float d2=px*px+py*py;
                const float g=std::exp(-d2/(2*radius*radius)); const int k=j*N+i;
                // Balanced pressure source (Mexican hat), little net added volume.
                height[k]+=amplitude*g*(1-d2/(2*radius*radius));
                foam[k]=std::min(1.f,foam[k]+bubbles*g);
            }
    }
    void tick() {
        constexpr float speed=2.5f,c2=(speed*Step/Cell)*(speed*Step/Cell);
        std::fill(next.begin(),next.end(),0.f);
        for(int y=1;y<N-1;++y) for(int x=1;x<N-1;++x) {
            const int k=y*N+x,border=std::min({x,y,N-1-x,N-1-y});
            const float damp=.996f*std::min(1.f,float(border)/12.f);
            const float lap=height[k-1]+height[k+1]+height[k-N]+height[k+N]-4*height[k];
            next[k]=std::clamp((2*height[k]-previous[k]+c2*lap)*damp,-.18f,.18f);
            foam[k]*=.992f*std::min(1.f,float(border)/8.f);
            // Breaking bow foam dissipates quickly; oar bubbles keep their
            // original longer lifetime. Avoid a persistent white carpet.
            bowFoam[k]*=.965f*std::min(1.f,float(border)/8.f);
        }
        previous.swap(height); height.swap(next);
    }
private:
    std::vector<float> next,scratch;
};
}
