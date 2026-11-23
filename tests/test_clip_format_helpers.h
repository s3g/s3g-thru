#pragma once
inline void stripResonDensity(std::vector<unsigned char>& bytes){if(bytes.size()<16||bytes[8]!=61)return;constexpr std::size_t size=4+64*5*4;const auto at=bytes.size()-size;assert(bytes[at]=='R'&&bytes[at+1]=='D'&&bytes[at+2]=='N'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=60;}
inline void stripLowformPrograms(std::vector<unsigned char>& bytes){stripResonDensity(bytes);if(bytes.size()<16||bytes[8]!=60)return;constexpr std::size_t size=4+64*5*4*32*4;const auto at=bytes.size()-size;assert(bytes[at]=='L'&&bytes[at+1]=='F'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=59;}
inline void stripPartialsPrograms(std::vector<unsigned char>& bytes){stripLowformPrograms(bytes);if(bytes.size()<16||bytes[8]!=59)return;constexpr std::size_t size=4+64*5*4*104*4;const auto at=bytes.size()-size;assert(bytes[at]=='P'&&bytes[at+1]=='T'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=58;}
inline void stripResonPrograms(std::vector<unsigned char>& bytes){stripPartialsPrograms(bytes);if(bytes.size()<16||bytes[8]!=58)return;constexpr std::size_t size=4+64*5*4*32*4;const auto at=bytes.size()-size;assert(bytes[at]=='R'&&bytes[at+1]=='S'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=57;}
inline void stripNoisePrograms(std::vector<unsigned char>& bytes){stripResonPrograms(bytes);if(bytes.size()<16||bytes[8]!=57)return;constexpr std::size_t size=4+64*5*4*88*4;const auto at=bytes.size()-size;assert(bytes[at]=='N'&&bytes[at+1]=='I'&&bytes[at+2]=='S'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=56;}
inline void stripFmExtras(std::vector<unsigned char>& bytes){
 stripNoisePrograms(bytes);
 if(bytes.size()<16||bytes[8]!=56)return;constexpr std::size_t size=4+64*5*4*9*4;const auto at=bytes.size()-size;assert(bytes[at]=='F'&&bytes[at+1]=='M'&&bytes[at+2]=='X'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=55;
}
inline void stripFmPrograms(std::vector<unsigned char>& bytes){
 stripFmExtras(bytes);
 if(bytes.size()<16||bytes[8]!=55)return;constexpr std::size_t size=4+64*5*4*23*4;const auto at=bytes.size()-size;assert(bytes[at]=='F'&&bytes[at+1]=='M'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=54;
}
inline void stripScannedAmpPrograms(std::vector<unsigned char>& bytes){
 stripFmPrograms(bytes);
 if(bytes.size()<16||bytes[8]!=54)return;constexpr std::size_t size=4+64*5*4*8*4;const auto at=bytes.size()-size;
 assert(bytes[at]=='S'&&bytes[at+1]=='A'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=53;
}
#include "thru_sequence.h"
inline void stripScannedHitPrograms(std::vector<unsigned char>& bytes){
 stripScannedAmpPrograms(bytes);
 if(bytes.size()<16||bytes[8]!=53)return;constexpr std::size_t size=4+64*5*4*7*4;const auto at=bytes.size()-size;
 assert(bytes[at]=='S'&&bytes[at+1]=='H'&&bytes[at+2]=='P'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=52;
}
inline void stripScannedPrograms(std::vector<unsigned char>& bytes){
 stripScannedHitPrograms(bytes);
 if(bytes.size()<16||bytes[8]!=52)return;constexpr std::size_t size=4+64*5*4*16*4;const auto at=bytes.size()-size;
 assert(bytes[at]=='S'&&bytes[at+1]=='C'&&bytes[at+2]=='N'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=51;
}
inline void stripSequenceSoundLocks(std::vector<unsigned char>& bytes){
 stripScannedPrograms(bytes);
 if(bytes.size()<16||bytes[8]!=51)return;std::uint32_t size=0;for(int i=0;i<4;++i)size|=std::uint32_t(bytes[bytes.size()-4+i])<<(8*i);assert(size>=264&&size<bytes.size());const auto at=bytes.size()-size;assert(bytes[at]=='S'&&bytes[at+1]=='L'&&bytes[at+2]=='K'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=50;
}
inline void stripTablePrograms(std::vector<unsigned char>&bytes){
 stripSequenceSoundLocks(bytes);
 if(bytes.size()<16||bytes[8]!=50)return;constexpr std::size_t size=4+64*5*4*40*4;const auto at=bytes.size()-size;
 assert(bytes[at]=='T'&&bytes[at+1]=='A'&&bytes[at+2]=='B'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=49;
}
inline void stripLfoAuto(std::vector<unsigned char>&bytes){
 stripTablePrograms(bytes);
 if(bytes.size()<16||bytes[8]!=49)return;constexpr std::size_t size=4+64*8*72;const auto at=bytes.size()-size;
 assert(bytes[at]=='L'&&bytes[at+1]=='A'&&bytes[at+2]=='T'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=48;
}
inline void stripPooledLfos(std::vector<unsigned char>& bytes){
 stripLfoAuto(bytes);
 if(bytes.size()<16||bytes[8]!=48)return;constexpr std::size_t size=4+64*(8*200+4*2*32*72);const auto at=bytes.size()-size;
 assert(bytes[at]=='L'&&bytes[at+1]=='F'&&bytes[at+2]=='P'&&bytes[at+3]=='2');bytes.resize(at);
 const auto u32=[&](std::size_t n){assert(n+4<=bytes.size());std::uint32_t v=0;for(int i=0;i<4;++i)v|=std::uint32_t(bytes[n+i])<<(8*i);return v;};
 std::vector<unsigned char> out(bytes.begin(),bytes.begin()+240);std::size_t pos=240;
 const auto copy=[&](std::size_t n){assert(pos+n<=bytes.size());out.insert(out.end(),bytes.begin()+pos,bytes.begin()+pos+n);pos+=n;};
 for(int c=0;c<64;++c){copy(4+176*4+4*(4+768+64)+16*4+36);const auto notes=u32(pos),points=u32(pos+4);copy(8);const auto countAt=out.size()-4;copy(notes*24);std::uint32_t kept=0;
  for(std::uint32_t i=0;i<points;++i){if(u32(pos+16)<17){copy(24);++kept;}else pos+=24;}for(int i=0;i<4;++i)out[countAt+i]=static_cast<unsigned char>(kept>>(8*i));}
 copy(bytes.size()-pos);bytes.swap(out);bytes[8]=47;
}
inline void stripControlLfos(std::vector<unsigned char>& bytes){
 stripPooledLfos(bytes);
 if(bytes.size()<16||bytes[8]!=47)return;
 constexpr std::size_t size=4+64*(10*124+4*2*32*24);const auto at=bytes.size()-size;
 assert(bytes[at]=='L'&&bytes[at+1]=='F'&&bytes[at+2]=='O'&&bytes[at+3]=='1');bytes.resize(at);
 const auto u32=[&](std::size_t n){assert(n+4<=bytes.size());std::uint32_t v=0;for(int i=0;i<4;++i)v|=std::uint32_t(bytes[n+i])<<(8*i);return v;};
 std::vector<unsigned char> out(bytes.begin(),bytes.begin()+240);std::size_t pos=240;
 const auto copy=[&](std::size_t n){assert(pos+n<=bytes.size());out.insert(out.end(),bytes.begin()+pos,bytes.begin()+pos+n);pos+=n;};
 for(int c=0;c<64;++c){copy(4+176*4+4*(4+768+64)+16*4+36);const auto notes=u32(pos),points=u32(pos+4);copy(8);const auto countAt=out.size()-4;copy(notes*24);std::uint32_t kept=0;
  for(std::uint32_t i=0;i<points;++i){if(u32(pos+16)<16){copy(24);++kept;}else pos+=24;}
  for(int i=0;i<4;++i)out[countAt+i]=static_cast<unsigned char>(kept>>(8*i));
 }
 copy(bytes.size()-pos);bytes.swap(out);bytes[8]=46;
}
inline std::size_t clipLegacyTailSize(const std::vector<unsigned char>& bytes){
 auto clean=bytes;stripSequenceSoundLocks(clean);
 return clean.size()-(bytes.size()>12&&bytes[8]>=50?(4+64*5*4*40*4):0)-(bytes.size()>12&&bytes[8]>=49?(4+64*8*72):0)-(bytes.size()>12&&bytes[8]>=48?(4+64*(8*200+4*2*32*72)):0)-(bytes.size()>12&&bytes[8]>=47?(4+64*(10*124+4*2*32*24)):0);
}
// Convert a current, checksum-free payload to the preceding format for migration fixtures.
inline void stripSequencePaths(std::vector<unsigned char>& bytes){
    stripControlLfos(bytes);
    if(bytes.size()<16||bytes[8]!=46)return;
    constexpr std::size_t size=4+64*4*4;const auto at=bytes.size()-size;
    assert(bytes[at]=='P'&&bytes[at+1]=='T'&&bytes[at+2]=='H'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=45;
}
inline void stripSequenceShifts(std::vector<unsigned char>& bytes){
    stripSequencePaths(bytes);
    if(bytes.size()<16||bytes[8]!=45)return;
    constexpr std::size_t size=4+64*4*4;const auto at=bytes.size()-size;
    assert(bytes[at]=='S'&&bytes[at+1]=='H'&&bytes[at+2]=='T'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=44;
}
inline void stripSequenceDirections(std::vector<unsigned char>& bytes) {
    stripSequenceShifts(bytes);
    if(bytes.size()<16||bytes[8]!=44)return;
    constexpr std::size_t size=4+64*4*8;
    assert(bytes.size()>size);const auto at=bytes.size()-size;
    assert(bytes[at]=='S'&&bytes[at+1]=='D'&&bytes[at+2]=='I'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=43;
}
inline void stripSequenceTypes(std::vector<unsigned char>& bytes) {
    stripSequenceDirections(bytes);
    if(bytes.size()<16||bytes[8]!=43)return;
    const auto n=bytes.size();std::uint32_t length=0;for(int i=0;i<4;++i)length|=std::uint32_t(bytes[n-4+i])<<(8*i);
    assert(length>=4&&length<n-12);const auto at=n-4-length;
    assert(bytes[at]=='S'&&bytes[at+1]=='Y'&&bytes[at+2]=='T'&&bytes[at+3]=='1');bytes.resize(at);bytes[8]=42;
}
inline void stripPulsarSeedExtension(std::vector<unsigned char>& bytes) {
    stripSequenceTypes(bytes);
    if(bytes.size()<12||bytes[8]!=42)return;
    constexpr std::size_t extra=4+64*4*4;
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]=='P'&&bytes[at+1]=='S'&&bytes[at+2]=='E'&&bytes[at+3]=='1');
    bytes.resize(at);bytes[8]=41;
}
inline void stripSplitLanePreferences(std::vector<unsigned char>& bytes) {
    stripPulsarSeedExtension(bytes);
    if(bytes.size()<12||bytes[8]!=41)return;
    constexpr std::size_t extra=4+64*4*4;
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]=='S'&&bytes[at+1]=='L'&&bytes[at+2]=='P'&&bytes[at+3]=='1');
    bytes.resize(at);bytes[8]=40;
}
inline void stripSequenceCycleExtension(std::vector<unsigned char>& bytes) {
    stripSplitLanePreferences(bytes);
    if(bytes.size()<12||bytes[8]!=40)return;
    constexpr std::size_t extra=4+64*4*32*2;
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]=='S'&&bytes[at+1]=='C'&&bytes[at+2]=='Y'&&bytes[at+3]=='1');
    bytes.resize(at);bytes[8]=39;
}
inline void stripSequenceMixExtension(std::vector<unsigned char>& bytes) {
    stripSequenceCycleExtension(bytes);
    if(bytes.size()<12||bytes[8]!=39)return;
    constexpr std::size_t perClip=4*(2+32*9)+4*6+8;
    constexpr std::size_t extra=4+64*perClip*4;
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]=='S'&&bytes[at+1]=='Q'&&bytes[at+2]=='T'&&bytes[at+3]=='2');
    bytes.resize(at);bytes[8]=38;
}
inline void stripSequenceExtension(std::vector<unsigned char>& bytes) {
    stripSequenceMixExtension(bytes);
    if(bytes.size()<12||bytes[8]!=38)return;
    constexpr std::size_t extra=4+64*s3g_thru_sequence::patternSize*4;
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]==0x31&&bytes[at+1]==0x51&&bytes[at+2]==0x45&&bytes[at+3]==0x53);
    bytes.resize(at);bytes[8]=37;
}
inline void stripLayersExtension(std::vector<unsigned char>& bytes) {
    stripSequenceExtension(bytes);
    if(bytes.size()<12||bytes[8]!=37)return;
    // Format 37 has four 242-field corners, four eight-field FX slots and 17 settings.
    constexpr std::size_t extra=4+64*(8+(4*242+32+17)*4);
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]==0x31&&bytes[at+1]==0x59&&bytes[at+2]==0x41&&bytes[at+3]==0x4c);
    bytes.resize(at);bytes[8]=36;
}
inline void stripSliceExtension(std::vector<unsigned char>& bytes) {
    stripLayersExtension(bytes);
    if(bytes.size()<12||bytes[8]!=36)return;
    constexpr std::size_t extra=4+64*4*(4+2*(8+65*4+64*24));
    assert(bytes.size()>extra);const auto at=bytes.size()-extra;
    assert(bytes[at]==0x53&&bytes[at+1]==0x4c&&bytes[at+2]==0x43&&bytes[at+3]==0x31);
    bytes.resize(at);bytes[8]=35;
}
inline void stripBankPadExtension(std::vector<unsigned char>& bytes) {
    stripSliceExtension(bytes);
    if(bytes.size()>=12&&bytes[8]==35)bytes[8]=34;
    if(bytes.size()>=12 && (bytes[8]==33 || bytes[8]==34)) {
        constexpr std::size_t extra=4+64*(8+16*12);
        assert(bytes.size()>extra);const auto at=bytes.size()-extra;
        assert(bytes[at]==0x31&&bytes[at+1]==0x44&&bytes[at+2]==0x50&&bytes[at+3]==0x42);
        bytes.resize(at);bytes[8]=32;
    }
}
inline void stripBankExtension(std::vector<unsigned char>& bytes) {
    stripBankPadExtension(bytes);
    if(bytes.size()<12||bytes[8]!=32)return;
    constexpr std::size_t size=4+64*(24+16*(768+32));
    assert(bytes.size()>size);const auto at=bytes.size()-size;
    assert(bytes[at]==0x31&&bytes[at+1]==0x4e&&bytes[at+2]==0x41&&bytes[at+3]==0x42);
    bytes.resize(at);bytes[8]=31;
}
inline void stripPriorityExtension(std::vector<unsigned char>& bytes) {
    stripBankExtension(bytes);
    if(bytes.size()<12||bytes[8]!=31)return;
    constexpr std::size_t size=4+64*5*8*4;
    assert(bytes.size()>size);const auto at=bytes.size()-size;
    assert(bytes[at]==0x31&&bytes[at+1]==0x43&&bytes[at+2]==0x55&&bytes[at+3]==0x44);
    bytes.resize(at);bytes[8]=30;
}
inline void stripPulsarExtension(std::vector<unsigned char>& bytes) {
    stripPriorityExtension(bytes);
    if(bytes.size()<12||(bytes[8]!=29&&bytes[8]!=30))return;
    const std::size_t size=4+64*5*(bytes[8]==30?18:10)*4;assert(bytes.size()>size);const auto at=bytes.size()-size;
    assert(bytes[at]==0x31&&bytes[at+1]==0x4c&&bytes[at+2]==0x55&&bytes[at+3]==0x50);
    bytes.resize(at);bytes[8]=28;
}
inline void stripStochasticExtension(std::vector<unsigned char>& bytes) {
    stripPulsarExtension(bytes);
    if(bytes.size()<12||(bytes[8]!=26&&bytes[8]!=27&&bytes[8]!=28))return;
    const std::size_t size=4+64*5*(bytes[8]==28?11:bytes[8]==27?10:8)*4;
    assert(bytes.size()>size);const auto at=bytes.size()-size;
    assert(bytes[at]==0x31&&bytes[at+1]==0x4f&&bytes[at+2]==0x54&&bytes[at+3]==0x53);
    bytes.resize(at);bytes[8]=25;
}
inline void stripVoicePlayExtension(std::vector<unsigned char>& bytes) {
    stripStochasticExtension(bytes);
    if(bytes.size()<12||bytes[8]!=25)return;
    std::uint32_t size=0;for(int b=0;b<4;++b)size|=static_cast<std::uint32_t>(bytes[bytes.size()-4+b])<<(b*8);
    assert(size>4&&size<bytes.size());const auto at=bytes.size()-size;
    assert(bytes[at]==0x31&&bytes[at+1]==0x4c&&bytes[at+2]==0x50&&bytes[at+3]==0x56);
    bytes.resize(at);bytes[8]=24;
}
inline void stripVoiceEngineExtension(std::vector<unsigned char>& bytes) {
    stripVoicePlayExtension(bytes);
    if(bytes.size()<12||bytes[8]!=24)return;
    constexpr std::size_t extension=4+64*5*12*4;
    assert(bytes.size()>extension);
    const auto at=bytes.size()-extension;
    assert(bytes[at]==0x31&&bytes[at+1]==0x4e&&bytes[at+2]==0x45&&bytes[at+3]==0x56);
    bytes.resize(at);bytes[8]=23;
}
