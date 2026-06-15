// Test: subtraction — 20 - 7 = 13
// Expected: a0 = 13
extern "C" void _start() {
    int a = 20;
    int b = 7;
    int r = a - b;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
