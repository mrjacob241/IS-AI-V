#include "IO_SDK.h"

static volatile unsigned int* const io_key_state =
    (volatile unsigned int*)0x00050000;
static volatile unsigned int* const io_stream_char =
    (volatile unsigned int*)0x00050004;
static volatile unsigned int* const io_stream_seq =
    (volatile unsigned int*)0x00050008;
static volatile unsigned int* const io_stream_ack =
    (volatile unsigned int*)0x0005000C;

KeyboardDevice Keyboard;

static unsigned int keyboard_current_state = 0;
static unsigned int keyboard_previous_state = 0;
static unsigned int keyboard_stream_seq = 0;

void KeyboardDevice::Update() {
    keyboard_previous_state = keyboard_current_state;
    keyboard_current_state = *io_key_state;
}

bool KeyboardDevice::KeyPressed(unsigned int key_mask) const {
    return (keyboard_current_state & key_mask) != 0;
}

bool KeyboardDevice::KeyDown(unsigned int key_mask) const {
    return ((keyboard_current_state & key_mask) != 0) &&
           ((keyboard_previous_state & key_mask) == 0);
}

bool KeyboardDevice::KeyUp(unsigned int key_mask) const {
    return ((keyboard_current_state & key_mask) == 0) &&
           ((keyboard_previous_state & key_mask) != 0);
}

int KeyboardDevice::KeysStream(char* out, int out_size) {
    if (!out || out_size <= 0)
        return 0;

    out[0] = '\0';

    unsigned int seq = *io_stream_seq;
    if (seq == keyboard_stream_seq)
        return 0;

    keyboard_stream_seq = seq;
    unsigned int ch = *io_stream_char;
    *io_stream_ack = seq;

    if ((ch & 0xFFu) == 0)
        return 0;

    out[0] = (char)(ch & 0xFFu);
    if (out_size > 1)
        out[1] = '\0';
    return 1;
}
