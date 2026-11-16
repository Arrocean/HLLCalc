// SPDX-License-Identifier: GPL-3.0-only
// Independent implementation; no upstream source code is incorporated.
// Protocol research (MIT-licensed csgo-sharecode, AkiVer), pinned revision:
// https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/doc/code_layout.md
// https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/src/share-code.ts
// https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/src/legacy-crosshair.ts
// https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/src/crosshair.ts
// License: https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/LICENSE
// The format facts are independently implemented with fixed-width byte arrays,
// checked multiply/add, explicit two's-complement conversion, and no BigInt or
// platform dependency. A checksum is not a cryptographic authenticity check.
#include "sharecode/cs_crosshair.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace cs_crosshair {
namespace {
constexpr std::string_view Alphabet="ABCDEFGHJKLMNOPQRSTUVWXYZabcdefhijkmnopqrstuvwxyz23456789";
static_assert(Alphabet.size()==57,"CS share codes use base 57");
bool AsciiSpace(char ch) {
    return ch==' '||ch=='\t'||ch=='\r'||ch=='\n'||ch=='\f'||ch=='\v';
}
bool Fail(std::string& error,const char* message) {error=message;return false;}
int SignedByte(std::uint8_t value) {return value<128?static_cast<int>(value):static_cast<int>(value)-256;}
int SignedWord(unsigned value) {return value<32768?static_cast<int>(value):static_cast<int>(value)-65536;}

template<std::size_t N,std::size_t Digits>
bool ReadPayload(const std::array<unsigned,Digits>& digits,std::array<std::uint8_t,N>& bytes,std::string& error) {
    // Share characters are least-significant digit first. The decoded payload
    // itself is a big-endian integer, including its leading zero bytes.
    for(std::size_t digit=Digits;digit>0;--digit) {
        unsigned carry=digits[digit-1];
        for(std::size_t i=N;i>0;--i) {
            const unsigned value=static_cast<unsigned>(bytes[i-1])*57u+carry;
            bytes[i-1]=static_cast<std::uint8_t>(value&255u);carry=value>>8;
        }
        if(carry!=0)return Fail(error,"Share code overflow");
    }
    unsigned sum=0;for(std::size_t i=1;i<N;++i)sum+=bytes[i];
    if(bytes[0]!=(sum&255u))return Fail(error,"Invalid checksum");
    return true;
}
template<std::size_t Digits>
bool ReadDigits(std::string_view code,std::size_t prefix,bool grouped,std::array<unsigned,Digits>& digits,std::string& error) {
    for(std::size_t i=0;i<Digits;++i) {
        const std::size_t at=prefix+i+(grouped?i/5:0);
        if(grouped&&i>0&&i%5==0&&code[at-1]!='-')return Fail(error,"Invalid share code format");
        const auto value=Alphabet.find(code[at]);
        if(value==std::string_view::npos)return Fail(error,"Invalid share code character");
        digits[i]=static_cast<unsigned>(value);
    }
    return true;
}
void DecodeLegacy1(const std::array<std::uint8_t,18>& b,Settings& value) {
    value.gap=SignedByte(b[2])/10.0;
    value.outline=b[3]/2.0;
    value.red=b[4];value.green=b[5];value.blue=b[6];value.alpha=b[7];
    value.color=b[10]&7;value.drawOutline=(b[10]&8)!=0;
    value.thickness=b[12]/10.0;value.style=(b[13]>>1)&7;
    value.dot=(b[13]&0x10)!=0;value.useAlpha=(b[13]&0x40)!=0;value.tStyle=(b[13]&0x80)!=0;
    value.length=b[14]/10.0;
    // Dynamic/recoil/split/weapon-gap and unused bits are not static appearance.
}
bool DecodeLegacyPixels(const std::array<std::uint8_t,18>& b,Settings& value,std::string& error) {
    value.version=b[1];value.pixelUnits=true;
    value.style=b[2]&15;value.dot=(b[2]&0x40)!=0;value.tStyle=(b[2]&0x80)!=0;
    if(value.style>(value.version==3?7:8))return Fail(error,"Unsupported crosshair style");
    value.red=b[3];value.green=b[4];value.blue=b[5];value.alpha=b[6];value.useAlpha=true;
    value.gap=b[7];value.length=b[8];
    const std::uint32_t bits=static_cast<std::uint32_t>(b[10])|
        (static_cast<std::uint32_t>(b[11])<<8)|(static_cast<std::uint32_t>(b[12])<<16)|
        (static_cast<std::uint32_t>(b[13])<<24);
    value.thickness=(bits>>23)&31;
    value.screenHeight=static_cast<int>(b[14])|(static_cast<int>(b[15])<<8);
    if(value.screenHeight==0)return Fail(error,"Invalid crosshair screen height");
    value.outlineMode=value.version==3?((b[2]&0x20)!=0?1:0):static_cast<int>((bits>>28)&3);
    if(value.outlineMode>2)return Fail(error,"Unsupported outline mode");
    value.drawOutline=value.outlineMode!=0;value.outline=value.drawOutline?1.0:0.0;
    return true;
}
bool DecodeCurrent(const std::array<std::uint8_t,32>& b,Settings& value,std::string& error) {
    if(b[1]!=1)return Fail(error,"Unsupported CS2 version");
    const int authoredHeight=static_cast<int>(b[2])|(static_cast<int>(b[3])<<8);
    if(authoredHeight==0)return Fail(error,"Invalid crosshair screen height");
    value.pixelUnits=true;value.screenHeight=std::max(authoredHeight,240);
    // These clamps mirror the current CS2 client's documented import behavior,
    // not an application's pixel conversion. See pinned crosshair.ts above.
    value.style=std::min(static_cast<int>(b[4]&31),9);
    value.dot=(b[4]&0x40)!=0;value.tStyle=(b[4]&0x80)!=0;
    value.red=b[5];value.green=b[6];value.blue=b[7];value.alpha=b[8];value.useAlpha=true;
    value.thickness=std::min(static_cast<int>(b[13]&63),32);
    value.outlineMode=std::min(static_cast<int>(b[13]>>6),2);
    value.drawOutline=value.outlineMode!=0;value.outline=value.drawOutline?1.0:0.0;
    const unsigned word=static_cast<unsigned>(b[14])|(static_cast<unsigned>(b[15])<<8);
    value.gap=std::clamp(SignedWord(word),-3840,3840);value.length=b[16];
    // Custom outline RGBA, scope settings and dynamic/recoil fields cannot be
    // represented by Settings and are deliberately ignored. UI must disclose
    // this appearance approximation; outlineMode still distinguishes half/full.
    return true;
}
}
bool Decode(const std::string& input,Settings& output,std::string& error) {
    std::string_view code(input);
    while(!code.empty()&&AsciiSpace(code.front()))code.remove_prefix(1);
    while(!code.empty()&&AsciiSpace(code.back()))code.remove_suffix(1);
    Settings decoded;
    if(code.size()==34&&code.substr(0,5)=="CSGO-") {
        std::array<unsigned,25> digits{};
        if(!ReadDigits(code,5,true,digits,error))return false;
        std::array<std::uint8_t,18> bytes{};
        if(!ReadPayload(digits,bytes,error))return false;
        if(bytes[1]==1)DecodeLegacy1(bytes,decoded);
        else if(bytes[1]==3||bytes[1]==4) {
            if(!DecodeLegacyPixels(bytes,decoded,error))return false;
        } else return Fail(error,"Unsupported legacy version");
    } else if(code.size()==46&&code.substr(0,2)=="CS") {
        std::array<unsigned,44> digits{};
        if(!ReadDigits(code,2,false,digits,error))return false;
        std::array<std::uint8_t,32> bytes{};
        if(!ReadPayload(digits,bytes,error))return false;
        if(!DecodeCurrent(bytes,decoded,error))return false;
    } else return Fail(error,"Invalid share code format");
    output=decoded;error.clear();return true;
}
}
