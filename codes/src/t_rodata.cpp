// t_rodata — verifies that .rodata string bytes land at the right RAM addresses.
// Reads "Hello" from a string literal and returns the sum of its bytes.
// 'H'=72 + 'e'=101 + 'l'=108 + 'l'=108 + 'o'=111 = 500  → expected a0 = 500

static const char msg[] = "Hello";

extern "C" void _start() {
    const char *p = msg;
    int sum = 0;
    while (*p) sum += (unsigned char)*p++;

    register int _a0 asm("a0") = sum;
    register int _a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(_a0), "r"(_a7) : "memory");
    __builtin_unreachable();
}
