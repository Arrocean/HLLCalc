// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace motion {
inline float EaseOutCubic(float t) {
    t=std::clamp(t,0.0f,1.0f);const float inv=1.0f-t;return 1.0f-inv*inv*inv;
}
inline float EaseInOutCubic(float t) {
    t=std::clamp(t,0.0f,1.0f);return t<0.5f?4.0f*t*t*t:1.0f-std::pow(-2.0f*t+2.0f,3.0f)*0.5f;
}
// Exact critically damped response: smooth velocity, no bounce and independent
// of frame rate. Retargeting preserves velocity instead of restarting an ease.
struct Spring {
    float value=0,velocity=0;
    void Snap(float target){value=target;velocity=0;}
    float Step(float target,float dt,float response=22.0f) {
        dt=std::clamp(dt,0.0f,0.1f);
        const float offset=value-target;
        const float impulse=velocity+response*offset;
        const float decay=std::exp(-response*dt);
        value=target+(offset+impulse*dt)*decay;
        velocity=(velocity-response*impulse*dt)*decay;
        if(std::abs(value-target)<0.0001f&&std::abs(velocity)<0.001f)Snap(target);
        return value;
    }
};
inline float SmootherStep(float t) {
    t=std::clamp(t,0.0f,1.0f);return t*t*t*(t*(t*6.0f-15.0f)+10.0f);
}
// Time-based tween can be reversed at its current value, with no positional jump.
struct Tween {
    float from=1,to=1,value=1;
    double started=0,duration=0.18;
    bool active=false;
    float Tick(double now) {
        if(!active)return value;
        const float t=duration>0?static_cast<float>((now-started)/duration):1.0f;
        value=from+(to-from)*EaseOutCubic(t);
        if(t>=1.0f){value=to;active=false;}
        return value;
    }
    void Start(float target,double now,double seconds=0.18) {
        Tick(now);from=value;to=target;started=now;duration=seconds;
        active=std::abs(to-from)>0.0001f;if(!active)value=to;
    }
    void Snap(float target){value=from=to=target;active=false;}
};
}
