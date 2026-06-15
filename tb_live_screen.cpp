#include "Screen_SDK.h"

static ScreenSDK::LiveConfig BuildLiveConfig(int argc, char** argv) {
    ScreenSDK::LiveConfig config;
    config.out_path = argc > 1 ? argv[1] : "screen.ppm";
    config.frames_to_write = ScreenSDK::env_int_allow_zero("LIVE_FRAMES", 0);
    config.fps = ScreenSDK::env_int_allow_zero("LIVE_FPS", 30);
    return config;
}

int main(int argc, char** argv) {
    return ScreenSDK::run_live_capture(BuildLiveConfig(argc, argv));
}
