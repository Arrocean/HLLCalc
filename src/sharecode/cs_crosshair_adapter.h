// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "sharecode/cs_crosshair.h"
#include "crosshair/crosshair.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace cs_crosshair {
// Only static geometry is transferred. No CS input, movement, recoil, weapon,
// firing or dynamic-spread behavior is registered or simulated by this adapter.
// Legacy v1 uses an approximate 480-line HUD reference (including the +4 gap
// baseline). Pixel formats scale only from their encoded reference height.
inline bool ToStaticConfig(const Settings& source,const crosshair::Config& current,
    int targetHeight,crosshair::Config& output,std::string& error) {
    if(source.color<0||source.color>5||source.style<0||source.style>(source.pixelUnits?9:5)) {
        error="此代码使用了暂不支持的颜色或准星样式。";return false;
    }
    if(!std::isfinite(source.length)||!std::isfinite(source.thickness)||
       !std::isfinite(source.gap)||!std::isfinite(source.outline)||
       source.length<0||source.thickness<0||source.outline<0||
       (source.pixelUnits&&source.screenHeight<=0)) {
        error="准星尺寸或参考分辨率无效。";return false;
    }
    const double height=static_cast<double>(std::clamp(targetHeight,240,16384));
    const double unit=source.pixelUnits?height/source.screenHeight:height/480.0;
    auto pixels=[](double value,int low,int high) {
        return static_cast<int>(std::lround(std::clamp(value,static_cast<double>(low),static_cast<double>(high))));
    };
    auto next=current;
    next.enabled=1;next.shape=0;next.cross=source.tStyle?1:0;
    next.dot=source.dot?1:0;next.ring=0;next.ruler=0;
    next.rotation=0;next.scale=100;next.offsetX=0;next.offsetY=0;
    next.length=pixels(source.length*unit,0,500);
    next.thickness=pixels(source.thickness*unit,1,100);
    next.dotSize=next.thickness;
    next.gap=pixels((source.gap+(source.pixelUnits?0.0:4.0))*unit,-500,500);
    next.outline=source.drawOutline&&source.outline>0?1:0;
    // Pixel share codes encode full/half outline modes, not a thickness. The
    // desktop renderer approximates either enabled mode with a whole-pixel border.
    next.outlineThickness=source.pixelUnits?1:pixels(source.outline*unit,1,20);
    next.opacity=source.useAlpha?std::clamp(source.alpha,0,255):255;
    constexpr int palette[5][3]={{250,50,50},{50,250,50},{250,250,50},{50,50,250},{50,250,250}};
    if(source.color<5) {
        next.red=palette[source.color][0];next.green=palette[source.color][1];next.blue=palette[source.color][2];
    } else {
        next.red=std::clamp(source.red,0,255);next.green=std::clamp(source.green,0,255);next.blue=std::clamp(source.blue,0,255);
    }
    output=next;error.clear();return true;
}
}
