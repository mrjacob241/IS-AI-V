// Test: SLL, SRL, SRA
//   SLL: 3 << 3 = 24
//   SRL: 96u >> 2 = 24  (unsigned, zero-fill)
//   SRA: -64 >> 2 = -16 (signed, sign-fill)
//   sum = 24 + 24 + (-16) = 32
// Expected: a0 = 32
extern "C" void _start() {
    int a = 3,  n = 3;
    int sll = a << n;

    unsigned int b = 96, m = 2;
    int srl = (int)(b >> m);

    int c = -64, p = 2;
    int sra = c >> p;

    int r = sll + srl + sra;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
