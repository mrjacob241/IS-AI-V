#ifndef IO_SDK_H
#define IO_SDK_H

#ifdef __cplusplus
extern "C" {
#endif

enum IOKeyboardKey {
    IO_KEY_W = 1u << 0,
    IO_KEY_S = 1u << 1,
    IO_KEY_Q = 1u << 2
};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class KeyboardDevice {
public:
    void Update();
    bool KeyPressed(unsigned int key_mask) const;
    bool KeyDown(unsigned int key_mask) const;
    bool KeyUp(unsigned int key_mask) const;
    int KeysStream(char* out, int out_size);

private:
};

extern KeyboardDevice Keyboard;

#endif

#endif
