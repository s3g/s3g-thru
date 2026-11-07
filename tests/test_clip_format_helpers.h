#pragma once
// Convert a current, checksum-free payload to the preceding format for migration fixtures.
inline void stripSliceExtension(std::vector<unsigned char>& bytes) {
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
