// SPDX-License-Identifier: GPL-3.0-only
// Independent standard C++17 crosshair share-code decoder. No Win32 dependency.
#pragma once
#include <string>

namespace cs_crosshair {
struct Settings {
    double length=0,thickness=0,gap=0,outline=0;
    int red=0,green=255,blue=0,alpha=255,color=5,style=4;
    bool useAlpha=false,drawOutline=false,dot=false,tStyle=false;
    // Legacy CSGO version 1 uses ConVar units; later formats use authored pixels.
    // version is scoped to the container: legacy-v1 and CS2-v1 both contain 1.
    int version=1;
    bool pixelUnits=false;
    int screenHeight=0; // Authored resolution for pixel formats; 0 for legacy-v1.
    int outlineMode=0;  // 0=off, 1=full, 2=half; pixel formats only.
};
// Complete, case-sensitive formats (optional surrounding ASCII whitespace):
//   CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx : 18 bytes, legacy versions 1/3/4
//   CS<44 base57 characters>           : 32 bytes, CS2 version 1
// No substring extraction, command execution or Win32/application dependencies.
// Failure preserves output and sets error. Success replaces output/clears error.
// RGB, preset color index, alpha (even if disabled) and numeric values remain
// decoded values. Caller resolves palette, disabled alpha, dynamic styles and
// screen-height scaling. Pixel outline uses 1px with outlineMode distinguishing
// full/half; unlike legacy-v1 it has no encoded variable outline thickness.
// Dynamic/unused bits are ignored, not rejected. Unknown versions, impossible
// legacy-pixel style/outline modes and zero authored screen height are rejected.
// Current CS2-v1 mirrors documented client clamps: height>=240, style<=9,
// thickness<=32, outlineMode<=2, and gap in [-3840,3840]. Legacy values are not
// subjected to those new-format clamps. Scope and custom outline-color fields
// are ignored because Settings cannot represent them; disclose this in the UI.
// The checksum is an additive 8-bit integrity check, not authentication; it
// cannot distinguish deliberate modifications with a recomputed/colliding sum.
bool Decode(const std::string& code, Settings& output, std::string& error);
}
