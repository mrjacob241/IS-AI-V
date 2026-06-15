extern "C" void _start() {
    int a = 2;
    
    for (int c = 0; c<2; c++) { 
      a = a+2; 
    }

    register int a0 asm("a0") = a;   // exit code = 5
    register int a7 asm("a7") = 93;  // Linux SYS_exit

    asm volatile(
        "ecall"
        :
        : "r"(a0), "r"(a7)
        : "memory"
    );

    __builtin_unreachable();
}
