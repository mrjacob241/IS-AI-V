// Test: function call — JAL (call) and JALR (ret)
// add(5, 7) = 12
// Expected: a0 = 12
//
// _start must be defined first so the linker places it at 0x0000.
// add is forward-declared here and defined below _start.

__attribute__((noinline)) static int add(int a, int b);

extern "C" void _start() {
    int r = add(5, 7);

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}

__attribute__((noinline))
static int add(int a, int b) {
    return a + b;
}
