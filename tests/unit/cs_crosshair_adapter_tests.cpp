// SPDX-License-Identifier: GPL-3.0-only
#include "sharecode/cs_crosshair_adapter.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
void Check(bool condition,const char* message) {
    if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
}
int main() {
    using namespace cs_crosshair;
    crosshair::Config current;current.monitor=2;current.rotation=123;current.scale=200;
    current.ring=1;current.ruler=1;current.offsetX=100;current.offsetY=-50;
    Settings legacy;legacy.length=2.5;legacy.thickness=0.5;legacy.gap=-3;
    legacy.outline=0.5;legacy.drawOutline=true;legacy.color=5;
    legacy.red=175;legacy.green=81;legacy.blue=213;legacy.alpha=137;
    legacy.useAlpha=true;legacy.dot=true;legacy.tStyle=true;legacy.style=2;
    crosshair::Config result;std::string error;
    Check(ToStaticConfig(legacy,current,1080,result,error),"legacy static conversion");
    Check(result.length==6&&result.thickness==1&&result.gap==2,"legacy HUD units and gap baseline");
    Check(result.red==175&&result.green==81&&result.blue==213&&result.opacity==137,"custom color and alpha");
    Check(result.dot==1&&result.dotSize==1&&result.shape==0&&result.cross==1,"square dot and static T shape");
    Check(result.outline==1&&result.outlineThickness==1,"outline rounding");
    Check(result.monitor==2&&result.offsetX==0&&result.offsetY==0&&result.rotation==0&&result.scale==100,"keep monitor, reset transforms");
    Check(result.enabled==1&&result.ruler==0&&result.ring==0,"no unrelated native components");
    legacy.useAlpha=false;legacy.alpha=0;
    Check(ToStaticConfig(legacy,current,1080,result,error)&&result.opacity==255,"disabled alpha ignores byte");
    constexpr int colors[5][3]={{250,50,50},{50,250,50},{250,250,50},{50,50,250},{50,250,250}};
    for(int color=0;color<5;++color){
        legacy.color=color;
        Check(ToStaticConfig(legacy,current,1080,result,error),"fixed color conversion");
        Check(result.red==colors[color][0]&&result.green==colors[color][1]&&result.blue==colors[color][2],"CS fixed palette");
    }
    for(int style=0;style<=5;++style){legacy.style=style;Check(ToStaticConfig(legacy,current,1080,result,error)&&result.cross==1,"all supported styles become static geometry");}
    Settings pixels;pixels.pixelUnits=true;pixels.version=4;pixels.screenHeight=1080;
    pixels.length=7;pixels.thickness=2;pixels.gap=-3;pixels.outline=1;
    pixels.drawOutline=true;pixels.outlineMode=2;pixels.color=5;pixels.useAlpha=true;pixels.alpha=200;
    Check(ToStaticConfig(pixels,current,2160,result,error),"new pixel code conversion");
    Check(result.length==14&&result.thickness==4&&result.gap==-6,"pixel units use only target/source ratio, no legacy gap offset");
    Check(result.outline==1&&result.outlineThickness==1,"half outline approximated by one pixel");
    auto unchanged=result;unchanged.length=77;result=unchanged;
    pixels.screenHeight=0;
    Check(!ToStaticConfig(pixels,current,1080,result,error)&&result.length==77&&!error.empty(),"invalid source resolution leaves output unchanged");
    pixels.screenHeight=1080;pixels.color=6;
    Check(!ToStaticConfig(pixels,current,1080,result,error)&&result.length==77,"unknown color rejected without mutation");
    pixels.color=5;pixels.style=10;
    Check(!ToStaticConfig(pixels,current,1080,result,error)&&result.length==77,"unknown style rejected without mutation");
    pixels.style=9;Check(ToStaticConfig(pixels,current,1080,result,error),"known current pixel style becomes static cross");
    result=unchanged;
    pixels.style=4;pixels.length=std::numeric_limits<double>::quiet_NaN();
    Check(!ToStaticConfig(pixels,current,1080,result,error)&&result.length==77,"non-finite geometry rejected");
    pixels.length=1e300;pixels.thickness=1e300;pixels.gap=-1e300;
    Check(ToStaticConfig(pixels,current,2160,result,error)&&result.length==500&&result.thickness==100&&result.gap==-500,"safe bounded pixel conversion");
    std::cout<<"All static CS-to-overlay adapter tests passed.\n";
}
