#pragma once
// WT9011DCL BLE 5.0 0x55/0x61 notification format, independently implemented
// from WITMOTION's published protocol. No configuration/control writes.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace row {
struct ImuVector { double x=0,y=0,z=0; };
inline ImuVector operator+(ImuVector a,ImuVector b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline ImuVector operator-(ImuVector a,ImuVector b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline ImuVector operator*(ImuVector a,double b) { return {a.x*b,a.y*b,a.z*b}; }
inline double dot(ImuVector a,ImuVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline double magnitude(ImuVector a) { return std::sqrt(dot(a,a)); }
inline bool finite(ImuVector a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
struct ImuSample {
    ImuVector acceleration,angularVelocity,angles; // m/s^2 (includes gravity), deg/s, deg
    double received=-1e9;
    uint64_t sequence=0;
    bool valid=false;
    bool fresh(double now) const {
        return valid && finite(acceleration) && finite(angularVelocity) && finite(angles)
            && std::isfinite(now) && now>=received && now-received<.25;
    }
};
inline bool parseImu(const uint8_t* data,size_t size,double now,ImuSample& out) {
    // BLE characteristic values are complete protocol packets, not a byte
    // stream. Reject malformed sizes; 0x71 register replies cannot revive motion.
    if(size!=20 || data[0]!=0x55 || data[1]!=0x61 || !std::isfinite(now)) return false;
    auto value=[&](size_t index,double scale) {
        const int raw=int(data[index])+(int(data[index+1])<<8);
        return (raw>=32768?raw-65536:raw)*scale/32768.;
    };
    out.acceleration={value(2,16*9.80665),value(4,16*9.80665),value(6,16*9.80665)};
    out.angularVelocity={value(8,2000),value(10,2000),value(12,2000)};
    out.angles={value(14,180),value(16,180),value(18,180)};
    out.received=now; ++out.sequence; out.valid=true; return true;
}
// Rotate the sensor axes to its AHRS frame (Rz(yaw)*Ry(pitch)*Rx(roll)).
// This frame has no relationship to SteamVR room north; never use it for HMD steering.
inline ImuVector imuEarthAcceleration(const ImuSample& sample) {
    constexpr double radians=3.14159265358979323846/180.;
    const double r=sample.angles.x*radians,p=sample.angles.y*radians,y=sample.angles.z*radians;
    const double cr=std::cos(r),sr=std::sin(r),cp=std::cos(p),sp=std::sin(p),cy=std::cos(y),sy=std::sin(y);
    const auto a=sample.acceleration;
    const ImuVector roll{a.x,cr*a.y-sr*a.z,sr*a.y+cr*a.z};
    const ImuVector pitch{cp*roll.x+sp*roll.z,roll.y,-sp*roll.x+cp*roll.z};
    return {cy*pitch.x-sy*pitch.y,sy*pitch.x+cy*pitch.y,pitch.z};
}
struct ImuFit {
    ImuVector gravity,axis;
    bool valid=false;
    bool usable() const { return valid && finite(gravity) && finite(axis) &&
        magnitude(gravity)>=7 && magnitude(gravity)<=12 && std::abs(magnitude(axis)-1)<.01; }
};
// A short, damped velocity estimate for stroke phase, never an absolute pose.
// Integrate each received sample once, independent of rendering frequency.
struct ImuMotion {
    double velocity=0,quiet=0;
    ImuFit fit;
    ImuSample previous;
    void start(const ImuFit& calibration,const ImuSample& sample) {
        *this=ImuMotion{}; fit=calibration; previous=sample;
    }
    bool tick(const ImuSample& sample,double now) {
        if(!fit.usable() || !sample.fresh(now)) return false;
        if(sample.sequence==previous.sequence) return sample.received==previous.received;
        const double dt=sample.received-previous.received;
        if(sample.sequence<previous.sequence || dt<=0 || dt>=.25) return false;
        const auto acceleration=imuEarthAcceleration(sample)-fit.gravity;
        const auto change=imuEarthAcceleration(sample)-imuEarthAcceleration(previous);
        previous=sample;
        if(magnitude(acceleration)>40 || magnitude(sample.angularVelocity)>350) return false;
        // Also reject residual constant bias: a quiet sensor cannot supply
        // indefinite drive, even while stale positive FTMS watts remain fresh.
        const bool still=magnitude(sample.angularVelocity)<4 &&
            (magnitude(acceleration)<.2 || magnitude(change)<.07);
        quiet=still?quiet+dt:0;
        // Keep a normal stroke's velocity long enough to distinguish its braking
        // acceleration from an actual reversal. Quiet/gap gates remove drift.
        velocity=std::clamp(velocity*std::exp(-dt/4.)+dot(acceleration,fit.axis)*dt,-2.5,2.5);
        if(quiet>=.35) velocity=0;
        return true;
    }
};
// Fit only the IMU's horizontal motion line. The first deliberate movement is
// a pull from the extended handle, defining the sign without magnetic north.
inline ImuFit fitImu(const std::vector<ImuSample>& samples,ImuVector gravity,unsigned& cycles) {
    cycles=0;
    ImuFit fit; fit.gravity=gravity;
    if(samples.size()<30 || !finite(gravity) || magnitude(gravity)<7 || magnitude(gravity)>12) return fit;
    double xx=0,xy=0,yy=0;
    for(const auto& sample:samples) {
        const auto a=imuEarthAcceleration(sample)-gravity;
        xx+=a.x*a.x; xy+=a.x*a.y; yy+=a.y*a.y;
    }
    const double spread=std::hypot(xx-yy,2*xy),major=(xx+yy+spread)*.5,minor=(xx+yy-spread)*.5;
    if(major/samples.size()<.09 || minor/major>.15) return fit;
    const double angle=.5*std::atan2(2*xy,xx-yy);
    fit.axis={std::cos(angle),std::sin(angle),0}; fit.valid=true;
    ImuMotion motion; motion.start(fit,samples.front());
    double firstTravel=0; bool signedAxis=false;
    for(size_t i=1;i<samples.size();++i) {
        if(!motion.tick(samples[i],samples[i].received)) { fit.valid=false; return fit; }
        const double dt=samples[i].received-samples[i-1].received;
        if(std::abs(motion.velocity)<.10) { firstTravel=0; continue; }
        if(firstTravel*motion.velocity<0) firstTravel=0;
        firstTravel+=motion.velocity*dt;
        if(std::abs(firstTravel)>.10) {
            if(firstTravel>0) fit.axis=fit.axis*-1.;
            signedAxis=true; break;
        }
    }
    if(!signedAxis) { fit.valid=false; return fit; }
    motion.start(fit,samples.front());
    double pull=0,recovery=0; bool pulled=false;
    for(size_t i=1;i<samples.size();++i) {
        if(!motion.tick(samples[i],samples[i].received)) { fit.valid=false; return fit; }
        const double delta=motion.velocity*(samples[i].received-samples[i-1].received);
        if(!pulled && motion.velocity<-.10) {
            pull-=delta;
            if(pull>.10) { pulled=true; recovery=0; }
        } else if(pulled && motion.velocity>.10) {
            recovery+=delta;
            if(recovery>.12) { ++cycles; pulled=false; pull=0; }
        }
    }
    cycles=std::min(2u,cycles); fit.valid=cycles>=2; return fit;
}
}
