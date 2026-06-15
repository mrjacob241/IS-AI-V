// Test: SLT (signed less-than) and SLTU (unsigned less-than)
//   SLT:  (3 < 7)  = 1  (signed)
//   SLTU: (1 < 0xFFFFFFFF) = 1  (unsigned; 0xFFFFFFFF is max uint)
//   r = slt*10 + sltu*5 = 10 + 5 = 15
// Expected: a0 = 15
extern "C" void _start() {
    int a = 3, b = 7;
    int slt = (a < b);

    unsigned int ua = 1, ub = 0xFFFFFFFF;
    int sltu = (int)(ua < ub);

    int r = slt * 10 + sltu * 5;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
