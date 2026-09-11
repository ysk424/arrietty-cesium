#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace row {
// Linear deep-water gravity waves: eta_t=|D|phi, phi_t=-g*eta-p/rho.
// Moving bow/stern pressure excites the transverse AND divergent Kelvin waves;
// no V-shaped mask or prescribed wake angle. SI units; mean lake level is zero.
// Original radix-2 FFT implementation, no external libraries. See docs/WATER.md.
class KelvinWake {
public:
    static constexpr int N=256;
    static constexpr float Cell=.25f,Step=1.f/30.f,Gravity=9.81f;
    static constexpr double Pi=3.14159265358979323846;
    double originX=-32,originY=-32;
    std::vector<float> height;

    KelvinWake():height(N*N),potential(N*N),pressure(N*N),shifted(N*N),
        spectrum(N*N),scratch(N*N),cosine(N*N),etaFromPhi(N*N),phiFromEta(N*N),absorb(N*N) {
        for(int i=0;i<N;++i) {
            int reversed=0;
            for(int bit=0;bit<8;++bit) reversed=(reversed<<1)|((i>>bit)&1);
            reverse[i]=reversed;
            const float phase=float(-2*Pi*i/N);
            roots[i]={std::cos(phase),std::sin(phase)};
        }
        for(int y=0;y<N;++y) for(int x=0;x<N;++x) {
            const int i=y*N+x;
            const float k=float(2*Pi/(N*Cell))*std::hypot(float(x<=N/2?x:x-N),float(y<=N/2?y:y-N));
            const float omega=std::sqrt(Gravity*k);
            cosine[i]=std::cos(omega*Step);
            const float s=omega>0?std::sin(omega*Step)/omega:Step;
            etaFromPhi[i]=k*s; phiFromEta[i]=-Gravity*s;
            // 8 m sponge lies outside the 22 m fully visible radius. Attenuate
            // before waves can wrap around the FFT's periodic boundary.
            const float edge=std::min({x,y,N-1-x,N-1-y})*Cell;
            const float rim=std::clamp((8.f-edge)/8.f,0.f,1.f);
            absorb[i]=std::exp(-Step*(.055f+9.f*rim*rim));
        }
    }
    void clear() {
        std::fill(height.begin(),height.end(),0.f);
        std::fill(potential.begin(),potential.end(),0.f);
    }
    // Translate samples, never rotate or attach an existing wake to the boat.
    // New cells are calm; a teleport discards the old domain completely.
    void center(double x,double y) {
        if(!std::isfinite(x)||!std::isfinite(y)) return;
        const double nx=std::floor((x-32)/Cell)*Cell,ny=std::floor((y-32)/Cell)*Cell;
        const double sx=(nx-originX)/Cell,sy=(ny-originY)/Cell;
        if(std::abs(sx)>=N||std::abs(sy)>=N) { clear(); originX=nx; originY=ny; return; }
        const int dx=int(std::round(sx)),dy=int(std::round(sy));
        if(!dx&&!dy) return;
        for(auto* field:{&height,&potential}) {
            std::fill(shifted.begin(),shifted.end(),0.f);
            for(int j=0;j<N;++j) for(int i=0;i<N;++i) {
                const int ix=i+dx,iy=j+dy;
                if(ix>=0&&ix<N&&iy>=0&&iy<N) shifted[j*N+i]=(*field)[iy*N+ix];
            }
            field->swap(shifted);
        }
        originX=nx; originY=ny;
    }
    void tick(double boatX,double boatY,double heading,float speed) {
        std::fill(pressure.begin(),pressure.end(),0.f);
        if(std::isfinite(boatX)&&std::isfinite(boatY)&&std::isfinite(heading)&&std::isfinite(speed)&&speed>.12f) {
            const double fx=std::cos(heading),fy=std::sin(heading);
            const float strength=std::clamp(speed/3.f,0.f,1.f);
            // p/rho in m^2/s^2. Smooth quadratic onset, bounded at fast rowing.
            // Two compact hull loads give bow/stern interference, with no thrust
            // or other feedback into boat motion or exercise/session metrics.
            load(boatX+2*fx,boatY+2*fy,.42f,Gravity*.045f*strength*strength);
            load(boatX-1.8*fx,boatY-1.8*fy,.50f,Gravity*.032f*strength*strength);
        }
        // Symmetric pressure kicks around exact dispersive propagation. Pack
        // both real fields into one complex transform to keep CPU cost bounded.
        for(int i=0;i<N*N;++i) spectrum[i]={height[i],potential[i]-.5f*Step*pressure[i]};
        transform(false);
        for(int y=0;y<N;++y) for(int x=0;x<N;++x) {
            const int i=y*N+x,j=((N-y)%N)*N+(N-x)%N;
            if(i>j) continue;
            const Complex h=.5f*(spectrum[i]+std::conj(spectrum[j]));
            const Complex p=Complex(0,-.5f)*(spectrum[i]-std::conj(spectrum[j]));
            const Complex nextH=cosine[i]*h+etaFromPhi[i]*p;
            const Complex nextP=cosine[i]*p+phiFromEta[i]*h;
            spectrum[i]=nextH+Complex(0,1)*nextP;
            spectrum[j]=std::conj(nextH)+Complex(0,1)*std::conj(nextP);
        }
        spectrum[0]=0; // No uniform lift of the Z=0 reference surface.
        transform(true);
        for(int i=0;i<N*N;++i) {
            height[i]=spectrum[i].real()*absorb[i];
            potential[i]=(spectrum[i].imag()-.5f*Step*pressure[i])*absorb[i];
        }
    }
private:
    using Complex=std::complex<float>;
    std::vector<float> potential,pressure,shifted;
    std::vector<Complex> spectrum,scratch;
    std::vector<float> cosine,etaFromPhi,phiFromEta,absorb;
    std::array<int,N> reverse{};
    std::array<Complex,N> roots{};
    void load(double x,double y,float radius,float strength) {
        const double gx=(x-originX)/Cell,gy=(y-originY)/Cell;
        if(gx<0||gx>=N||gy<0||gy>=N) return;
        const int cx=int(std::round(gx)),cy=int(std::round(gy)),r=int(std::ceil(4*radius/Cell));
        for(int j=std::max(0,cy-r);j<std::min(N,cy+r+1);++j)
            for(int i=std::max(0,cx-r);i<std::min(N,cx+r+1);++i) {
                const float dx=float(originX+i*Cell-x),dy=float(originY+j*Cell-y);
                pressure[j*N+i]+=strength*std::exp(-(dx*dx+dy*dy)/(2*radius*radius));
            }
    }
    void rows(bool inverse) {
        for(int y=0;y<N;++y) {
            auto* row=spectrum.data()+y*N;
            for(int i=0;i<N;++i) if(i<reverse[i]) std::swap(row[i],row[reverse[i]]);
            for(int size=2;size<=N;size*=2) for(int start=0;start<N;start+=size)
                for(int j=0;j<size/2;++j) {
                    const auto w=inverse?std::conj(roots[j*N/size]):roots[j*N/size];
                    const auto a=row[start+j],b=w*row[start+j+size/2];
                    row[start+j]=a+b; row[start+j+size/2]=a-b;
                }
        }
    }
    void transpose() {
        // Small blocks avoid strided cache misses on the 256-square field.
        for(int by=0;by<N;by+=16) for(int bx=0;bx<N;bx+=16)
            for(int y=by;y<by+16;++y) for(int x=bx;x<bx+16;++x) scratch[x*N+y]=spectrum[y*N+x];
        spectrum.swap(scratch);
    }
    void transform(bool inverse) {
        rows(inverse); transpose(); rows(inverse); transpose();
        if(inverse) for(auto& v:spectrum) v*=1.f/(N*N);
    }
};
}
