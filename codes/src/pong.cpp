static volatile unsigned int* const fb =
    (volatile unsigned int*)0x00020000;

static void fill_rect(int x, int y, int w, int h, unsigned int color);
static void draw_net();
static void draw_border();
static int clamp_paddle(int y);

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

    draw_border();
    draw_net();

    for (int frame = 0; frame < 1; frame++) {
        fill_rect(old_ball_x, old_ball_y, 4, 4, 0x00000000u);
        fill_rect(26, old_left_y, 4, 20, 0x00000000u);
        fill_rect(130, old_right_y, 4, 20, 0x00000000u);
        draw_net();
        draw_border();

        old_ball_x = ball_x;
        old_ball_y = ball_y;
        old_left_y = left_y;
        old_right_y = right_y;
        left_y = clamp_paddle(ball_y - 10);
        right_y = clamp_paddle(ball_y - 10);

        fill_rect(26, left_y, 4, 20, 0x00E8E8E8u);
        fill_rect(130, right_y, 4, 20, 0x00E8E8E8u);
        fill_rect(ball_x, ball_y, 4, 4, 0x00FFCC30u);

        ball_x += ball_dx;
        ball_y += ball_dy;

        if (ball_y <= 24 || ball_y >= 92)
            ball_dy = -ball_dy;

        if (ball_x <= 32 || ball_x >= 124)
            ball_dx = -ball_dx;
    }

    register int a0 asm("a0") = 0;
    register int a7 asm("a7") = 93;

    asm volatile(
        "ecall"
        :
        : "r"(a0), "r"(a7)
        : "memory"
    );

    __builtin_unreachable();
}

static void fill_rect(int x, int y, int w, int h, unsigned int color) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        int row = (py << 7) + (py << 5);
        for (int xx = 0; xx < w; xx++)
            fb[row + x + xx] = color;
    }
}

static void draw_net() {
    for (int y = 24; y < 96; y += 8)
        fill_rect(79, y, 2, 4, 0x00606060u);
}

static void draw_border() {
    fill_rect(20, 20, 120, 2, 0x00404040u);
    fill_rect(20, 98, 120, 2, 0x00404040u);
}

static int clamp_paddle(int y) {
    if (y < 24)
        return 24;
    if (y > 76)
        return 76;
    return y;
}
