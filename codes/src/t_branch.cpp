// Test: all branch types — BEQ, BNE, BLT, BGE, BLTU, BGEU
// Each true condition adds a power of 2 to result.
// All six conditions are true, so result = 1+2+4+8+16+32 = 63.
//
//   if (a == b)   → GCC emits BNE to skip  (a=5, b=5: equal)
//   if (c != d)   → GCC emits BEQ to skip  (c=3, d=7: not equal)
//   if (c < d)    → GCC emits BGE to skip  (3 < 7: true)
//   if (d >= c)   → GCC emits BLT to skip  (7 >= 3: true)
//   if (ua < ub)  → GCC emits BGEU to skip (1 < 0xFFFFFFFF unsigned: true)
//   if (ub >= ua) → GCC emits BLTU to skip (0xFFFFFFFF >= 1 unsigned: true)
//
// Expected: a0 = 63
extern "C" void _start() {
    int a = 5, b = 5;
    int c = 3, d = 7;
    unsigned int ua = 1, ub = 0xFFFFFFFF;
    unsigned int result = 0;

    if (a == b)   result += 1;
    if (c != d)   result += 2;
    if (c < d)    result += 4;
    if (d >= c)   result += 8;
    if (ua < ub)  result += 16;
    if (ub >= ua) result += 32;

    register int a0 asm("a0") = (int)result;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
