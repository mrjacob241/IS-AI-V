#ifndef SCREEN_SDK_H
#define SCREEN_SDK_H

#define SCREEN_COLOR_BLACK  0x00000000u
#define SCREEN_COLOR_WHITE  0x00E8E8E8u
#define SCREEN_COLOR_GRAY   0x00606060u
#define SCREEN_COLOR_BORDER 0x00404040u
#define SCREEN_COLOR_YELLOW 0x00FFCC30u

#ifdef __cplusplus
extern "C" {
#endif

void Screen_Clear(unsigned int color);
void Screen_Draw_Rectangle(int x, int y, int w, int h, unsigned int color);
void Screen_Erase_Moved_Rectangle(int old_x, int old_y, int new_x, int new_y,
                                  int w, int h);
void Screen_Update(unsigned int game_frame);
void Screen_Delay(int cycles);

#ifdef __cplusplus
}
#endif

namespace ScreenSDK {

struct LiveConfig {
    const char* out_path;
    int frames_to_write; // 0 means run until Ctrl+C
    int fps;
};

int env_int_allow_zero(const char* name, int fallback);
int run_live_capture(const LiveConfig& config);

} // namespace ScreenSDK

#endif
