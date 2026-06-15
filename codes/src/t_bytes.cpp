// Test: sub-word stores and loads — SB, SH, LBU, LHU
//   unsigned char  b = 42  → SB to store, LBU to load as int = 42
//   unsigned short h = 200 → SH to store, LHU to load as int = 200
//   r = 42 + 200 = 242
// Expected: a0 = 242
extern "C" void _start() {
    unsigned char  b = 42;
    unsigned short h = 200;
    int r = (int)b + (int)h;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
