// Test: if/else branch — max(7, 3) = 7
// Expected: a0 = 7
extern "C" void _start() {
    int a = 7;
    int b = 3;
    int result;
    if (a > b)
        result = a;
    else
        result = b;

    register int a0 asm("a0") = result;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
