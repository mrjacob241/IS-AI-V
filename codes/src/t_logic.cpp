// Test: bitwise AND, OR, XOR
// a=0x0F, b=0x36
//   AND: 0x0F & 0x36 = 0x06 =  6
//   OR:  0x0F | 0x36 = 0x3F = 63
//   XOR: 0x0F ^ 0x36 = 0x39 = 57
//   sum = 6 + 63 + 57 = 126
// Expected: a0 = 126
extern "C" void _start() {
    int a = 0x0F;
    int b = 0x36;
    int r_and = a & b;
    int r_or  = a | b;
    int r_xor = a ^ b;
    int r = r_and + r_or + r_xor;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
