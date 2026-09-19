#pragma once
// PS4 Bluetooth report layout independently implemented from SDL's published
// HID driver. No effects, rumble, lightbar or speaker flags are enabled.
#include "RowImu.h"

namespace row {
inline uint32_t ps4Crc(uint8_t prefix,const uint8_t* bytes,size_t n) {
    uint32_t crc=0xffffffffu;
    auto add=[&](uint8_t b) { crc^=b; for(int i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320u:0); };
    add(prefix); for(size_t i=0;i<n;++i) add(bytes[i]); return ~crc;
}
struct Ps4Packet {
    ImuVector acceleration,gyro;
    uint16_t tick=0;
    bool square=false,triangle=false,l1=false,r1=false,l2=false,r2=false;
};
inline bool parsePs4(const uint8_t* b,size_t n,Ps4Packet& out) {
    // Windows pads input to the collection's largest report. Only the first
    // 78 bytes are the CRC-protected Bluetooth 0x11 report. Basic 0x01 is NOT IMU.
    if(n<78 || b[0]!=0x11 || !(b[1]&0x80)) return false;
    uint32_t crc=0; for(unsigned i=0;i<4;++i) crc|=uint32_t(b[74+i])<<(8*i);
    if(crc!=ps4Crc(0xa1,b,74)) return false;
    auto value=[&](size_t i) { const int x=int(b[i])+(int(b[i+1])<<8); return double(x>=32768?x-65536:x); };
    Ps4Packet p; p.tick=uint16_t(b[12]|(uint16_t(b[13])<<8));
    p.gyro={value(15)/16.,value(17)/16.,value(19)/16.};
    p.acceleration={value(21)*9.80665/8192.,value(23)*9.80665/8192.,value(25)*9.80665/8192.};
    p.square=(b[7]&0x10)!=0; p.triangle=(b[7]&0x80)!=0;
    p.l1=(b[8]&1)!=0; p.r1=(b[8]&2)!=0;
    p.l2=(b[8]&4)!=0; p.r2=(b[8]&8)!=0;
    out=p; return true;
}
inline std::vector<uint8_t> ps4EnableReport(size_t size) {
    if(size<78 || size>4096) return {};
    std::vector<uint8_t> b(size); b[0]=0x11; b[1]=0xc4;
    const auto crc=ps4Crc(0xa2,b.data(),74);
    for(unsigned i=0;i<4;++i) b[74+i]=uint8_t(crc>>(8*i));
    return b;
}
struct Ps4Controls {
    double received=-1e9,steer=0;
    uint64_t starts=0,stops=0;
    bool valid=false;
    bool fresh(double now) const { return valid && std::isfinite(steer) && std::abs(steer)<=1 && now>=received && now-received<.25; }
};
struct Ps4Buttons {
    Ps4Controls state;
    bool known=false,square=true,triangle=true;
    void disconnect() { state.valid=false; state.steer=0; known=false; }
    void tick(const Ps4Packet& p,double now) {
        // Stop also wins when held during reconnect; square requires a fresh
        // release/press so reconnect cannot inject a new start command.
        if(p.triangle && (!known || !triangle)) ++state.stops;
        else if(known && p.square && !square && !p.triangle) ++state.starts;
        known=true; square=p.square; triangle=p.triangle;
        const double left=p.l2?1:p.l1?.5:0,right=p.r2?1:p.r1?.5:0;
        state.steer=left && right?0:right-left;
        state.received=now; state.valid=true;
    }
};
inline ImuVector cross(ImuVector a,ImuVector b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct ImuQuaternion {
    double w=1,x=0,y=0,z=0;
    void normalize() { const double n=std::sqrt(w*w+x*x+y*y+z*z); w/=n;x/=n;y/=n;z/=n; }
    ImuVector rotate(ImuVector v) const {
        const ImuVector q{x,y,z}; const auto t=cross(q,v)*2.; return v+t*w+cross(q,t);
    }
    ImuVector inverse(ImuVector v) const { return ImuQuaternion{w,-x,-y,-z}.rotate(v); }
    ImuQuaternion multiply(ImuQuaternion b) const {
        return {w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,
            w*b.y-x*b.z+y*b.w+z*b.x,w*b.z+x*b.y-y*b.x+z*b.w};
    }
    void integrate(ImuVector r,double dt) {
        const auto d=cross({x,y,z},r); const double nw=w-.5*dt*(x*r.x+y*r.y+z*r.z);
        x+=.5*dt*(w*r.x+d.x); y+=.5*dt*(w*r.y+d.y); z+=.5*dt*(w*r.z+d.z); w=nw; normalize();
    }
};
// Body gyro integrates every fresh hardware report, independent of UE cadence.
// Bounded complementary gravity correction limits drift; short rowing impulses
// are separated by the motion filter. Initial yaw is arbitrary; two strokes fit
// its motion axis. These are estimated velocities, never measured room positions.
class Ps4Fusion {
public:
    ImuSample sample;
    ImuQuaternion attitude;
    double gravityLength=9.80665;
    bool tick(const Ps4Packet& p,double now) {
        if(!std::isfinite(now) || !finite(p.acceleration) || !finite(p.gyro) || magnitude(p.gyro)>2000 || magnitude(p.acceleration)>80) return false;
        const double dt=now-sample.received;
        if(!sample.valid || dt>=.25) {
            const double n=magnitude(p.acceleration); if(n<7 || n>12) return false;
            const auto a=sample.valid?attitude.rotate(p.acceleration*(1./n)):p.acceleration*(1./n);
            if(!sample.valid) gravityLength=n;
            ImuQuaternion tilt;
            if(a.z<-.9999) tilt={0,1,0,0};
            else { const auto axis=cross(a,{0,0,1}); tilt={1+a.z,axis.x,axis.y,axis.z}; tilt.normalize(); }
            attitude=sample.valid?tilt.multiply(attitude):tilt;
        } else {
            if(dt<=0) return false;
            auto radians=p.gyro*(3.14159265358979323846/180.);
            attitude.integrate(radians,dt);
            const double n=magnitude(p.acceleration);
            if(n>7 && n<14) {
                const auto error=cross(p.acceleration*(1./n),attitude.inverse({0,0,1}));
                attitude.integrate(error*2.,dt);
            }
        }
        sample.acceleration=attitude.rotate(p.acceleration);
        sample.angularVelocity=p.gyro; sample.angles={}; sample.compensated=true;
        sample.received=now; ++sample.sequence; sample.valid=true; return true;
    }
};
}
