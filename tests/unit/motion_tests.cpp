// SPDX-License-Identifier: GPL-3.0-only
#include "app/motion.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
void Check(bool value,const char* message){if(!value){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main(){
    using namespace motion;
    Check(EaseOutCubic(-1)==0&&EaseOutCubic(1)==1&&EaseOutCubic(2)==1,"clamped endpoints");
    Check(EaseOutCubic(0.5f)>0.8f,"nonlinear ease out");
    Check(std::abs(EaseInOutCubic(0.5f)-0.5f)<0.0001f,"symmetric midpoint");
    float previous=0;for(int i=0;i<=1000;++i){const float value=EaseOutCubic(i/1000.0f);Check(value>=previous&&value<=1,"monotonic bounded easing");previous=value;}
    Tween tween;tween.Snap(0);tween.Start(1,10,0.2);
    const float halfway=tween.Tick(10.1);Check(halfway>0.8f&&halfway<1&&tween.active,"time-based opening");
    tween.Start(0,10.1,0.16);Check(std::abs(tween.value-halfway)<0.0001f,"reverse without jump");
    Check(tween.Tick(10.18)<halfway,"closing after reversal");
    Check(tween.Tick(10.3)==0&&!tween.active,"finished closing");
    tween.Start(1,11,0);Check(tween.Tick(11)==1&&!tween.active,"zero duration snaps safely");
    std::cout<<"All nonlinear motion tests passed.\n";
}
