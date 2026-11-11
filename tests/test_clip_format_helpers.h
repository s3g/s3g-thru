#pragma once
#include "thru_sequence.h"
// Convert a current, checksum-free payload to the preceding format for migration fixtures.
inline void stripSequenceCycleExtension(std::vector<unsigned char>& bytes) {
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
