// SPDX-License-Identifier: GPL-3.0-only
// New test/encoder implementation is independent, GPL-3.0-only.
// Public test-vector data/expected facts adapted from:
// https://github.com/akiver/csgo-sharecode/blob/139b9e284756110b44a894331f285b0ddabdf6ef/src/index.test.ts
// The separate upstream MIT notice for those fixtures is retained here:
/*
The MIT License (MIT)
Copyright (c) 2017-present AkiVer
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "sharecode/cs_crosshair.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
using cs_crosshair::Settings;
constexpr char Alphabet[]="ABCDEFGHJKLMNOPQRSTUVWXYZabcdefhijkmnopqrstuvwxyz23456789";
unsigned checks=0;
void Check(bool condition,const char* message) {
    ++checks;if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
bool Near(double a,double b) {return std::abs(a-b)<1e-9;}
bool Same(const Settings& a,const Settings& b) {
    return Near(a.length,b.length)&&Near(a.thickness,b.thickness)&&Near(a.gap,b.gap)&&Near(a.outline,b.outline)&&
        a.red==b.red&&a.green==b.green&&a.blue==b.blue&&a.alpha==b.alpha&&a.color==b.color&&a.style==b.style&&
        a.useAlpha==b.useAlpha&&a.drawOutline==b.drawOutline&&a.dot==b.dot&&a.tStyle==b.tStyle&&
        a.version==b.version&&a.pixelUnits==b.pixelUnits&&a.screenHeight==b.screenHeight&&a.outlineMode==b.outlineMode;
}
Settings Sentinel() {
    Settings s;s.length=9.8;s.thickness=7.6;s.gap=-5.4;s.outline=3.5;
    s.red=12;s.green=34;s.blue=56;s.alpha=78;s.color=2;s.style=3;
    s.useAlpha=true;s.drawOutline=true;s.dot=true;s.tStyle=true;
    s.version=99;s.pixelUnits=true;s.screenHeight=777;s.outlineMode=2;return s;
}
void Bad(const std::string& code,const char* errorPart=nullptr) {
    auto output=Sentinel();const auto before=output;std::string error="old";
    Check(!cs_crosshair::Decode(code,output,error),"reject invalid code");
    Check(Same(output,before),"failure preserves every output field");
    Check(!error.empty(),"failure supplies an error");
    if(errorPart)Check(error.find(errorPart)!=std::string::npos,"specific error category");
}
Settings Good(const std::string& code) {
    Settings output=Sentinel();std::string error="old";
    Check(cs_crosshair::Decode(code,output,error),"decode valid code");Check(error.empty(),"success clears error");return output;
}
template<std::size_t N> void Checksum(std::array<std::uint8_t,N>& b) {
    unsigned sum=0;for(std::size_t i=1;i<N;++i)sum+=b[i];b[0]=static_cast<std::uint8_t>(sum&255u);
}
// Independent test-only encoder: repeated long division, not the decoder's
// multiply/add. Public fixture equality below independently checks digit order.
template<std::size_t N> std::string Encode(std::array<std::uint8_t,N> value) {
    const std::size_t count=N==18?25:44;std::string code=N==18?"CSGO-":"CS";
    for(std::size_t digit=0;digit<count;++digit) {
        unsigned remainder=0;
        for(auto& byte:value) {
            const unsigned partial=remainder*256u+byte;
            byte=static_cast<std::uint8_t>(partial/57u);remainder=partial%57u;
        }
        if(N==18&&digit>0&&digit%5==0)code+='-';code+=Alphabet[remainder];
    }
    for(auto byte:value)Check(byte==0,"test encoder emits all bits");return code;
}
std::array<std::uint8_t,18> Legacy() {
    std::array<std::uint8_t,18> b{};b[1]=1;b[4]=17;b[5]=33;b[6]=91;b[7]=137;b[10]=5;b[13]=8;b[14]=20;Checksum(b);return b;
}
void PublicVectors() {
    const char* first="CSGO-Cn37R-YE7vo-pLCAL-aURmZ-z6zkG";
    auto s=Good(first);
    Check(Near(s.gap,-1.3)&&Near(s.outline,2)&&Near(s.thickness,1.2)&&Near(s.length,4.6),"public legacy1 fractional values");
    Check(s.red==175&&s.green==81&&s.blue==213&&s.alpha==137&&s.color==5,"public legacy1 RGBA");
    Check(s.style==2&&s.useAlpha&&s.drawOutline&&s.dot&&s.tStyle&&!s.pixelUnits&&s.version==1,"public legacy1 flags");
    std::array<std::uint8_t,18> fixture{{0,1,243,4,175,81,213,137,6,30,109,84,12,244,46,0,0,0}};
    Checksum(fixture);Check(Encode(fixture)==first,"independent encoder reproduces public vector exactly");
    s=Good("CSGO-LibdP-VCVEd-ESayK-rSivi-2UBtG");
    Check(Near(s.gap,1)&&Near(s.outline,1)&&Near(s.thickness,0.5)&&Near(s.length,5),"second legacy1 numeric vector");
    Check(s.red==50&&s.green==250&&s.blue==50&&s.alpha==200&&s.color==1&&s.style==3,"preset index and stored RGB are distinct fields");
    Check(!s.useAlpha&&!s.drawOutline&&!s.dot&&!s.tStyle,"disabled alpha keeps raw alpha and flags");
    s=Good("CSGO-9JzcN-4dZtA-DdXis-8qz5T-rCnkP");
    Check(s.style==1&&s.dot&&s.useAlpha&&!s.tStyle&&Near(s.length,5),"legacy1 style1 and weapon-gap flag sample");
    s=Good("CSGO-fCUBz-CBHss-a74RP-SEdO8-mvZpG");
    Check(s.style==2&&s.dot&&s.useAlpha&&Near(s.gap,1)&&Near(s.thickness,0.5),"recoil bit does not alter static fields");
    s=Good("CSGO-WsnnD-eHaMw-QNDf9-oxuDh-ydOUD");
    Check(Near(s.gap,-2.2)&&Near(s.length,10)&&Near(s.thickness,0.6)&&s.drawOutline&&s.useAlpha,"negative gap with recoil sample");
    s=Good("CSGO-ZrEjo-yASEP-OAdce-Sf44w-rhK5O");
    Check(Near(s.gap,-1.2)&&Near(s.length,7.2)&&Near(s.thickness,1.5)&&s.tStyle&&s.dot&&!s.useAlpha,"public T style and fractional size");
    Check(s.red==232&&s.green==88&&s.blue==227&&s.alpha==136,"public custom color sample");
    Check(Same(Good(std::string(" \t\r\n\f\v")+first+"\v\f\n\r\t "),Good(first)),"all six surrounding ASCII whitespace forms");
    s=Good("CSGO-MnUCC-89iG7-2cVar-wy7Yn-amCpF");
    Check(s.version==3&&s.pixelUnits&&s.screenHeight==1080&&s.style==6&&s.dot&&!s.tStyle,"public legacy3 format and flags");
    Check(s.red==252&&s.green==15&&s.blue==192&&s.alpha==255&&Near(s.gap,4)&&Near(s.length,8)&&Near(s.thickness,3)&&s.drawOutline&&s.outlineMode==1,"public legacy3 pixel fields");
    s=Good("CSGO-tsJdC-2PFX6-fw8qD-6GDaX-rYCUM");
    Check(s.version==3&&s.screenHeight==768&&s.style==4&&s.dot&&!s.drawOutline&&Near(s.gap,0)&&Near(s.length,0)&&Near(s.thickness,3),"public legacy3 zero lengths");
    s=Good("CSGO-G8oAC-RyvWc-Hi3CZ-voSwn-QJbfE");
    Check(s.version==4&&s.pixelUnits&&s.screenHeight==768&&s.style==8&&s.dot&&s.outlineMode==1&&Near(s.gap,25)&&Near(s.length,7)&&Near(s.thickness,20),"public legacy4 fields");
    s=Good("CSGO-sP6xU-TSyN9-sZcO5-2D48M-UppkP");
    Check(s.version==4&&s.style==2&&s.red==255&&s.green==0&&s.blue==0&&s.dot&&!s.drawOutline&&Near(s.length,5)&&Near(s.thickness,1),"public legacy4 recoil-only ignored");
    s=Good("CSvbPubOq37zTGqtsPTP5QTrp5CB4xFXiKRLfzJsm49ZRe");
    Check(s.version==1&&s.pixelUnits&&s.screenHeight==768&&s.style==2&&s.dot&&!s.tStyle,"public game-generated CS2 container");
    Check(s.red==255&&s.green==0&&s.blue==0&&s.alpha==255&&s.color==5&&s.useAlpha&&!s.drawOutline&&Near(s.length,5)&&Near(s.thickness,1)&&Near(s.gap,0),"public CS2 static appearance");
    const auto base=s;
    s=Good("CSoYQNXCQtL5b44fPZKwPPZZCUpsywopyvJW3koh75uFih");Check(Same(s,base),"scope-dot flag is ignored");
    s=Good("CSHt3ObzKS3ikGJLRBVrKjPUrhOCikyke53na8cJFtNZKa");Check(Same(s,base),"scope-dot scale is ignored");
    s=Good("CSAB6Oby2JUDOrBRy4jRcsR6JOQdhbszKF9GuKQGhKVwAc");
    Check(s.screenHeight==1440&&s.style==9&&s.tStyle&&!s.dot&&s.outlineMode==2&&s.red==10&&s.green==20&&s.blue==30&&s.alpha==200&&Near(s.gap,-7)&&Near(s.length,255)&&Near(s.thickness,12),"public CS2 half-outline signed-gap fields");
    s=Good("CS7uw3wMkJBJ4rMDLEaYbyfTJFFPvkLAhOmBFi8zOM6H2d");
    Check(s.screenHeight==240&&s.style==9&&s.outlineMode==2&&Near(s.thickness,32)&&Near(s.gap,-3840),"CS2 documented client sanitization");
    s=Good("CSDfPcMpj4e9NRxx6SGiVoFCHzYRYGmb34WHpbYHy6E6id");Check(Near(s.gap,3840),"CS2 positive signed-gap clamp");
}
void LegacyRoundTrips() {
    for(unsigned flags=0;flags<256;++flags) {
        auto b=Legacy();b[2]=static_cast<std::uint8_t>(flags);b[3]=static_cast<std::uint8_t>(255-flags);
        b[8]=0xff;b[9]=0x80;b[10]=0xfd;b[11]=0xff;b[12]=static_cast<std::uint8_t>(flags);b[13]=static_cast<std::uint8_t>(flags);
        b[14]=static_cast<std::uint8_t>(255-flags);b[15]=0x55;b[16]=0x80;b[17]=0xff;Checksum(b);
        const auto s=Good(Encode(b));
        const int signedGap=flags<128?static_cast<int>(flags):static_cast<int>(flags)-256;
        Check(Near(s.gap,signedGap/10.0)&&Near(s.outline,(255-flags)/2.0)&&Near(s.thickness,flags/10.0)&&Near(s.length,(255-flags)/10.0),"roundtrip every signed byte/fractional size");
        Check(s.style==static_cast<int>((flags>>1)&7)&&s.dot==((flags&16)!=0)&&s.useAlpha==((flags&64)!=0)&&s.tStyle==((flags&128)!=0),"roundtrip all byte13 flags including unused bit");
        Check(s.color==5&&s.drawOutline&&s.red==17&&s.green==33&&s.blue==91&&s.alpha==137,"dynamic/unused payload bits do not contaminate static fields");
    }
    for(unsigned palette=0;palette<8;++palette) {
        auto b=Legacy();b[10]=static_cast<std::uint8_t>(palette);Checksum(b);const auto s=Good(Encode(b));
        Check(s.color==static_cast<int>(palette)&&s.red==17&&s.green==33&&s.blue==91,"palette selector preserved; caller resolves preset colors 0..4 and handles 6/7");
    }
    auto b=Legacy();b.fill(0);b[1]=1;b[2]=255;Checksum(b);Check(b[0]==0,"leading-zero checksum fixture");
    Check(Near(Good(Encode(b)).gap,-0.1),"leading zero bytes are padded, not dropped");
    for(std::size_t i=0;i<b.size();++i){auto broken=b;broken[i]^=1;Bad(Encode(broken),"checksum");}
    for(unsigned version:{0u,2u,5u,255u}){auto unknown=b;unknown[1]=static_cast<std::uint8_t>(version);Checksum(unknown);Bad(Encode(unknown),"version");}
}
void PixelRoundTrips() {
    std::uint32_t random=0x12345678u;
    for(unsigned n=0;n<1024;++n) {
        std::array<std::uint8_t,32> b{};
        for(auto& v:b){random=random*1664525u+1013904223u;v=static_cast<std::uint8_t>(random>>24);}
        b[1]=1;b[2]=0xa0;b[3]=5;Checksum(b);const auto s=Good(Encode(b));
        Check(s.pixelUnits&&s.screenHeight==1440&&s.style==std::min<int>(b[4]&31,9)&&s.dot==((b[4]&64)!=0)&&s.tStyle==((b[4]&128)!=0),"32-byte independent roundtrip flags");
        const unsigned word=static_cast<unsigned>(b[14])|(static_cast<unsigned>(b[15])<<8);
        const int gap=word<32768?static_cast<int>(word):static_cast<int>(word)-65536;
        Check(Near(s.gap,std::max(-3840,std::min(gap,3840)))&&Near(s.length,b[16])&&Near(s.thickness,std::min<int>(b[13]&63,32)),"32-byte independent roundtrip sizes");
        Check(s.red==b[5]&&s.green==b[6]&&s.blue==b[7]&&s.alpha==b[8]&&s.useAlpha&&s.color==5,"32-byte independent roundtrip RGBA");
        Check(s.outlineMode==std::min<int>(b[13]>>6,2)&&s.drawOutline==(s.outlineMode!=0),"32-byte independent roundtrip full/half outlines");
    }
    for(unsigned version:{3u,4u})for(unsigned mode=0;mode<3;++mode) {
        std::array<std::uint8_t,18> b{};b[1]=static_cast<std::uint8_t>(version);b[2]=0xc4;b[3]=11;b[4]=22;b[5]=33;b[6]=77;b[7]=255;b[8]=254;
        const std::uint32_t bits=(31u<<23)|(mode<<28)|0x123456u;
        for(unsigned i=0;i<4;++i)b[10+i]=static_cast<std::uint8_t>(bits>>(i*8));
        b[14]=0x38;b[15]=4;if(version==3&&mode!=0)b[2]|=32;Checksum(b);
        const auto s=Good(Encode(b));Check(s.version==static_cast<int>(version)&&s.screenHeight==1080&&s.pixelUnits&&s.style==4&&s.dot&&s.tStyle,"legacy pixel version and flags roundtrip");
        Check(Near(s.length,254)&&Near(s.gap,255)&&Near(s.thickness,31)&&s.alpha==77&&s.useAlpha,"legacy pixel raw-size roundtrip");
        Check(s.outlineMode==static_cast<int>(version==3?(mode!=0?1:0):mode),"legacy pixel half/full mode roundtrip");
    }
    std::array<std::uint8_t,32> b{};b[1]=1;b[2]=1;Checksum(b);auto valid=Encode(b);Check(Good(valid).screenHeight==240,"nonzero screen height minimum");
    for(std::size_t i=0;i<b.size();++i){auto broken=b;broken[i]^=1;Bad(Encode(broken),"checksum");}
    b[1]=2;Checksum(b);Bad(Encode(b),"version");b[1]=1;b[2]=0;Checksum(b);Bad(Encode(b),"screen height");
    std::array<std::uint8_t,18> legacy{};legacy[1]=3;Checksum(legacy);Bad(Encode(legacy),"screen height");
    legacy[14]=1;legacy[2]=15;Checksum(legacy);Bad(Encode(legacy),"style");legacy[1]=4;legacy[2]=4;legacy[13]=0x30;Checksum(legacy);Bad(Encode(legacy),"outline");
}
void InvalidInputs() {
    const std::string good="CSGO-Cn37R-YE7vo-pLCAL-aURmZ-z6zkG";
    for(const auto& value:std::vector<std::string>{"", "CSGO",good.substr(1),good+"A","x"+good,"\""+good+"\"",good+";quit",std::string("copy ")+good,std::string("\xc2\xa0")+good,good+std::string(1,'\0')})Bad(value,"format");
    auto lower=good;lower.replace(0,4,"csgo");Bad(lower,"format");
    for(std::size_t separator:{10u,16u,22u,28u}){auto wrong=good;wrong[separator]='A';Bad(wrong,"format");}
    for(char ch:std::string("01Igl_ -\x80")){auto wrong=good;wrong[7]=ch;Bad(wrong,"character");}
    Bad("CSGO-99999-99999-99999-99999-99999","overflow");Bad(std::string("CS")+std::string(44,'9'),"overflow");
    Bad("CSGO-eRqP8-AkrwM-3Wsqh-pKDTh-eAPtQ","version");
    Bad("CSGO-L9spZ-ihuov-cyhtE-kxbqa-FkBAA","checksum");
    Bad("CSuZhVyU8icUwjTjopBSVi6ctEbq5GiOXJBRdC6LNLKaFL","version");
    Bad("CSkSCHQKunGr2yPsJr2U3RTd8ycMA2pEs9jWnxK5KjMmWL","checksum");
    Bad("CSoQhciXBHZUKAbfexZBeMWFKxkrXL5MLumDLj3JYW4snP","screen height");
}
}
int main() {
    PublicVectors();LegacyRoundTrips();PixelRoundTrips();InvalidInputs();
    std::cout<<"PASS: "<<checks<<" crosshair decoder checks (legacy v1/v3/v4 and CS2 v1).\n";
}
