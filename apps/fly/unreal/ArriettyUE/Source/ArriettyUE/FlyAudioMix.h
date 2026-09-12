#pragma once
#include "CoreMinimal.h"

struct FFlyAudioInput
{
    bool Active=false, Airborne=false;
    float Speed=0, Cadence=0, Power=0;
    int32 Touchdowns=0;
};

// Independent of actors and sound assets so control semantics are testable.
struct FFlyAudioMix
{
    enum { WindSoft, WindFast, Pedal, Propeller, Ground, Touchdown, Count };
    float Gain[Count]{}, Pitch[Count]{1,1,1,1,1,1};
    int32 LastTouchdown=0;
    bool WasActive=false, TouchdownNow=false;
    static float Unit(float X) { return FMath::IsFinite(X)?FMath::Clamp(X,0.f,1.f):0.f; }
    void Tick(const FFlyAudioInput& In,float Dt)
    {
        const float Speed=Unit(In.Speed/60), Fast=Unit((In.Speed-20)/40);
        const float Cadence=Unit(In.Cadence/120), Power=Unit(In.Power/300);
        const float Drive=In.Cadence>3?FMath::Sqrt(Cadence):0;
        const float Targets[Count]{
            .36f*FMath::Sqrt(Speed)*(1-.45f*Fast), .30f*Fast,
            .18f*Drive*(.65f+.35f*Power), .22f*Drive*(.7f+.3f*Power),
            In.Airborne?0.f:.30f*FMath::Sqrt(Unit(In.Speed/40)), 0};
        const float Pitches[Count]{.85f+.25f*Speed,.85f+.30f*Speed,
            .7f+.70f*Cadence,.75f+.65f*Cadence,.8f+.4f*Speed,1};
        Dt=FMath::IsFinite(Dt)?FMath::Clamp(Dt,0.f,.1f):0;
        for(int I=0;I<Touchdown;++I)
        {
            const float Tau=!In.Active?.04f:(I==Propeller?.40f:.18f);
            const float Alpha=1-FMath::Exp(-Dt/Tau);
            Gain[I]+=((In.Active?Targets[I]:0)-Gain[I])*Alpha;
            if(Gain[I]<.00001f) Gain[I]=0;
            Pitch[I]+=(Pitches[I]-Pitch[I])*(1-FMath::Exp(-Dt/.25f));
        }
        TouchdownNow=In.Active && WasActive && In.Touchdowns>LastTouchdown;
        LastTouchdown=In.Touchdowns; WasActive=In.Active;
        Gain[Touchdown]=In.Active?.45f:0;
    }
};
