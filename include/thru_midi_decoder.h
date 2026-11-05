#ifndef S3G_THRU_MIDI_DECODER_H
#define S3G_THRU_MIDI_DECODER_H

#include <cstdint>

namespace s3g_thru_midi {

// One instance per input source and channel. Commit only a complete MSB-first
// 14-bit value; coarse/fine intermediates never reach the audio controls.
struct Decoder {
    int msb = -1, lsb = -1, data = -1;
    bool selected = false;

    void reset() { msb=lsb=data=-1;selected=false; }

    template<class Apply>
    bool cc(int number, int value, Apply apply) {
        if (number==121) {reset();return true;}
        if (number==101 || number==100) {reset();return true;}
        if (number==99) {
            // Starting a new selection must never retain an old address LSB
            // or a pending Data Entry MSB from another parameter.
            msb=value;lsb=data=-1;selected=false;return true;
        }
        if (number==98) {
            lsb=value;data=-1;selected=msb>=0 && !(msb==127 && lsb==127);return true;
        }
        if (number==6) {data=selected?value:-1;return true;}
        if (number==38) {
            if(selected && data>=0)apply((msb<<7)|lsb,(data<<7)|value);
            data=-1;return true;
        }
        // Standard data increment/decrement are deliberately consumed; the
        // contract uses complete absolute pairs, not controller-specific steps.
        return number==96 || number==97;
    }
};

} // namespace s3g_thru_midi
#endif
