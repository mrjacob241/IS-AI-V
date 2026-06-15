// Test: LUI + ADDI (32-bit constant construction)
// 0x000A0050 needs lui a5, 0xA0 then addi a5, a5, 0x50
// = 655360 + 80 = 655440
// Expected: a0 = 655440 (0x000A0050)
// Note: qemu exit code is truncated to 8 bits → 0x50 = 80
extern "C" void _start() {
    int r = 0x000A0050;

    register int a0 asm("a0") = r;
    register int a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    __builtin_unreachable();
}
