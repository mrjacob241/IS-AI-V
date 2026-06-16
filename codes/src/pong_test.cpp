#include "../../Screen_SDK.h"
#include "../../IO_SDK.h"

static void draw_net();
static void draw_border();
static void draw_scene(int ball_x, int ball_y, int left_y, int right_y);
static int clamp_paddle(int y);

#define PONG_COLOR_RED 0x00CC2020u

extern "C" void _start() {
    int ball_x = 80;
    int ball_y = 60;
    int ball_dx = 2;
    int ball_dy = 1;
    int left_y = 50;
    int right_y = 50;
    int old_ball_x = ball_x;
    int old_ball_y = ball_y;
    int old_left_y = left_y;
    int old_right_y = right_y;
    int move_divider = 0;
    unsigned int game_frame = 0;

    draw_scene(ball_x, ball_y, left_y, right_y);

    while (1) {
        if (move_divider >= 89) {
            Keyboard.Update();
            move_divider = 0;

            if (Keyboard.KeyPressed(IO_KEY_W))
                left_y -= 3;
            if (Keyboard.KeyPressed(IO_KEY_S))
                left_y += 3;
            left_y = clamp_paddle(left_y);

            ball_x += ball_dx;
            ball_y += ball_dy;

            if (ball_y <= 24 || ball_y >= 92)
                ball_dy = -ball_dy;

            if (ball_x <= 20) {
                Screen_Clear(PONG_COLOR_RED);
                game_frame++;
                Screen_Update(game_frame);
                Screen_Delay(90000);

                ball_x = 80;
                ball_y = 60;
                ball_dx = 2;
                ball_dy = 1;
                left_y = 50;
                right_y = 50;
                old_ball_x = ball_x;
                old_ball_y = ball_y;
                old_left_y = left_y;
                old_right_y = right_y;
                move_divider = 0;

                Screen_Clear(SCREEN_COLOR_BLACK);
                draw_scene(ball_x, ball_y, left_y, right_y);
                game_frame++;
                Screen_Update(game_frame);
                Screen_Delay(12000);
            } else if (ball_x <= 32 && ball_dx < 0) {
                if (ball_y + 4 >= left_y && ball_y <= left_y + 20)
                    ball_dx = 2;
            }

            if (ball_x >= 124 && ball_dx > 0) {
                if (ball_y + 4 >= right_y && ball_y <= right_y + 20)
                    ball_dx = -2;
                else
                    ball_dx = -2;
            }

            right_y = clamp_paddle(ball_y - 10);

            Screen_Erase_Moved_Rectangle(old_ball_x, old_ball_y, ball_x, ball_y, 4, 4);
            Screen_Erase_Moved_Rectangle(26, old_left_y, 26, left_y, 4, 20);
            Screen_Erase_Moved_Rectangle(130, old_right_y, 130, right_y, 4, 20);
            draw_net();
            draw_border();

            Screen_Draw_Rectangle(26, left_y, 4, 20, SCREEN_COLOR_WHITE);
            Screen_Draw_Rectangle(130, right_y, 4, 20, SCREEN_COLOR_WHITE);
            Screen_Draw_Rectangle(ball_x, ball_y, 4, 4, SCREEN_COLOR_YELLOW);

            old_ball_x = ball_x;
            old_ball_y = ball_y;
            old_left_y = left_y;
            old_right_y = right_y;

            game_frame++;
            Screen_Update(game_frame);
        } else {
            move_divider++;
        }

        Screen_Delay(1200);
    }
}

static void draw_net() {
    for (int y = 24; y < 96; y += 8)
        Screen_Draw_Rectangle(79, y, 2, 4, SCREEN_COLOR_GRAY);
}

static void draw_border() {
    Screen_Draw_Rectangle(20, 20, 120, 2, SCREEN_COLOR_BORDER);
    Screen_Draw_Rectangle(20, 98, 120, 2, SCREEN_COLOR_BORDER);
}

static void draw_scene(int ball_x, int ball_y, int left_y, int right_y) {
    draw_border();
    draw_net();
    Screen_Draw_Rectangle(26, left_y, 4, 20, SCREEN_COLOR_WHITE);
    Screen_Draw_Rectangle(130, right_y, 4, 20, SCREEN_COLOR_WHITE);
    Screen_Draw_Rectangle(ball_x, ball_y, 4, 4, SCREEN_COLOR_YELLOW);
}

static int clamp_paddle(int y) {
    if (y < 24)
        return 24;
    if (y > 76)
        return 76;
    return y;
}
