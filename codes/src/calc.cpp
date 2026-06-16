// calc.cpp — integer adder (loop until 'q')
//
// Prompts for two integers, prints their sum, repeats.
// Type 'q' at either prompt to quit.
//
// Output/exit syscalls:
//   a7=11  putch(a0)          print one character
//   a7=93  exit(a0)
//
// Input comes from IO_SDK Keyboard.KeysStream().

#include "../../IO_SDK.h"

// Forward declarations (_start must be first in .text)
static void    putch(char c);
static void    print_str(const char *s);
static void    print_int(int n);
static int     getch(void);
static int     read_num(int *out);

extern "C" void _start() {
    while (1) {
        int a, b;

        print_str("first  (or q to quit): ");
        if (read_num(&a) < 0) break;

        print_str("second (or q to quit): ");
        if (read_num(&b) < 0) break;

        print_str("= ");
        print_int(a + b);
        putch('\n');
    }

    print_str("bye\n");

    register int _a0 asm("a0") = 0;
    register int _a7 asm("a7") = 93;
    asm volatile("ecall" : : "r"(_a0), "r"(_a7) : "memory");
    __builtin_unreachable();
}

// ---- I/O primitives --------------------------------------------------------

static void putch(char c) {
    register int _a0 asm("a0") = (int)(unsigned char)c;
    register int _a7 asm("a7") = 11;
    asm volatile("ecall" : : "r"(_a0), "r"(_a7) : "memory");
}

static void print_str(const char *s) {
    while (*s) putch(*s++);
}

// Print integer without using / or % (not in base RV32I).
// Uses a stack-local power-of-10 table and repeated subtraction.
static void print_int(int n) {
    if (n == 0) { putch('0'); return; }
    if (n < 0)  { putch('-'); n = -n; }

    int p[10];
    p[0] = 1000000000;
    p[1] = 100000000;
    p[2] = 10000000;
    p[3] = 1000000;
    p[4] = 100000;
    p[5] = 10000;
    p[6] = 1000;
    p[7] = 100;
    p[8] = 10;
    p[9] = 1;

    int started = 0;
    for (int i = 0; i < 10; i++) {
        int d = 0;
        while (n >= p[i]) { n -= p[i]; d++; }
        if (d || started) { putch('0' + d); started = 1; }
    }
}

static int getch(void) {
    char buf[2];
    while (1) {
        Keyboard.Update();
        if (Keyboard.KeysStream(buf, sizeof(buf)) > 0) {
            int c = (int)(unsigned char)buf[0];
            if (c == '\r')
                c = '\n';
            putch((char)c);
            return c;
        }
    }
}

static int is_line_end(int c) {
    return c == '\n' || c == '\r';
}

// Read a decimal integer from stdin.
// Returns 0 on success (value in *out), -1 if the user typed 'q'/'Q'.
// Parses digits with (num<<3)+(num<<1)+digit to avoid the mul instruction.
static int read_num(int *out) {
    int c = getch();

    if (c == 'q' || c == 'Q') {
        while (!is_line_end(c) && c > 0) c = getch();
        return -1;
    }

    int neg = 0;
    if (c == '-') { neg = 1; c = getch(); }

    int num = 0;
    while (c >= '0' && c <= '9') {
        num = (num << 3) + (num << 1) + (c - '0');
        c = getch();
    }
    while (!is_line_end(c) && c > 0) c = getch();

    *out = neg ? -num : num;
    return 0;
}
