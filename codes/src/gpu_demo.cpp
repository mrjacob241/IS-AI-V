static volatile unsigned int* const fb =
    (volatile unsigned int*)0x00020000;

extern "C" void _start() {
    for (int y = 0; y < 30; y++) {
        for (int x = 0; x < 40; x++) {
            unsigned int r = (unsigned int)(x << 1);
            unsigned int g = (unsigned int)(y << 1);
            unsigned int b = ((x >> 3) ^ (y >> 3)) & 1 ? 0x80u : 0x20u;
            fb[y * 160 + x] = (r << 16) | (g << 8) | b;
        }
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
