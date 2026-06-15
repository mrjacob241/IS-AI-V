extern "C" void _start() {
    int a = 3;
    int b = 2;
    int c = a + b;

    register int a0 asm("a0") = c;   // exit code = 5
    register int a7 asm("a7") = 93;  // Linux SYS_exit

    asm volatile(
        "ecall"
        :
        : "r"(a0), "r"(a7)
        : "memory"
    );

    __builtin_unreachable();
}
