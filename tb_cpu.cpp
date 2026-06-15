#include "Vcpu.h"
#include "verilated.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int failures = 0;

#define ASSERT(name, got, exp) do { \
    uint32_t _g = (uint32_t)(got); \
    uint32_t _e = (uint32_t)(exp); \
    if (_g != _e) { \
        printf("FAIL: %-34s  got=0x%08X  expected=0x%08X\n", name, _g, _e); \
        failures++; \
    } else { \
        printf("PASS: %s\n", name); \
    } \
} while (0)

static void eval(Vcpu& top) {
    top.eval();
}

static void tick(Vcpu& top) {
    top.clk = 0;
    top.eval();
    top.clk = 1;
    top.eval();
}

static void reset(Vcpu& top) {
    top.clk = 0;
    top.rst = 1;
    top.dbg_ram_we = 0;
    top.dbg_ram_addr = 0;
    top.dbg_ram_wd = 0;
    top.dbg_rf_we = 0;
    top.dbg_rf_rd = 0;
    top.dbg_rf_wd = 0;
    tick(top);
    top.rst = 0;
    eval(top);
}

static void store_instr(Vcpu& top, uint32_t addr, uint32_t instr) {
    top.dbg_ram_addr = addr;
    top.dbg_ram_wd = instr;
    top.dbg_ram_we = 1;
    tick(top);
    top.dbg_ram_we = 0;
    eval(top);
}

static void write_reg(Vcpu& top, uint8_t rd, uint32_t value) {
    top.dbg_rf_rd = rd;
    top.dbg_rf_wd = value;
    top.dbg_rf_we = 1;
    tick(top);
    top.dbg_rf_we = 0;
    eval(top);
}

static void load_program(Vcpu& top, const uint32_t* program, size_t words) {
    top.rst = 1;
    eval(top);
    for (size_t i = 0; i < words; i++)
        store_instr(top, (uint32_t)(i * 4), program[i]);
    top.rst = 0;
    eval(top);
}

static void run_cycles(Vcpu& top, int cycles) {
    for (int i = 0; i < cycles; i++)
        tick(top);
}

static bool parse_disassembly_word(const char* line, uint32_t* addr, uint32_t* word) {
    const char* colon = strchr(line, ':');
    if (!colon)
        return false;

    char* end = nullptr;
    unsigned long parsed_addr = strtoul(line, &end, 16);
    if (end != colon)
        return false;

    const char* p = colon + 1;
    while (*p == ' ' || *p == '\t')
        p++;

    char hex[9] = {};
    for (int i = 0; i < 8; i++) {
        if (!((p[i] >= '0' && p[i] <= '9') ||
              (p[i] >= 'a' && p[i] <= 'f') ||
              (p[i] >= 'A' && p[i] <= 'F')))
            return false;
        hex[i] = p[i];
    }

    *addr = (uint32_t)parsed_addr;
    *word = (uint32_t)strtoul(hex, nullptr, 16);
    return true;
}

static void load_disassembly(Vcpu& top, const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        printf("FAIL: open %s\n", path);
        failures++;
        return;
    }

    top.rst = 1;
    eval(top);

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        uint32_t addr = 0;
        uint32_t word = 0;
        if (parse_disassembly_word(line, &addr, &word))
            store_instr(top, addr, word);
    }

    fclose(file);
    top.rst = 0;
    eval(top);
}

static void run_until_ecall(Vcpu& top, int max_cycles) {
    for (int cycle = 0; cycle < max_cycles; cycle++) {
        eval(top);
        if ((uint32_t)top.dbg_instr == 0x00000073)
            return;
        tick(top);
    }

    printf("FAIL: timeout waiting for ecall\n");
    failures++;
}

static void test_program_a() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0x00A00093, // addi x1, x0, 10
        0x00300113, // addi x2, x0, 3
        0x002081B3, // add  x3, x1, x2
        0x40208233, // sub  x4, x1, x2
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 4);
    ASSERT("CPU-A x1", top.dbg_x1, 10);
    ASSERT("CPU-A x2", top.dbg_x2, 3);
    ASSERT("CPU-A x3", top.dbg_x3, 13);
    ASSERT("CPU-A x4", top.dbg_x4, 7);
}

static void test_program_b() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0x02A00093, // addi x1, x0, 42
        0x04000113, // addi x2, x0, 64
        0x00112023, // sw   x1, 0(x2)
        0x00012183, // lw   x3, 0(x2)
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 4);
    ASSERT("CPU-B x3", top.dbg_x3, 42);
}

static void test_program_c() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0x00100093, // addi x1, x0, 1
        0x00200113, // addi x2, x0, 2
        0x00208463, // beq  x1, x2, +8
        0x06300193, // addi x3, x0, 99
        0x00000213, // addi x4, x0, 0
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 5);
    ASSERT("CPU-C x3", top.dbg_x3, 99);
}

static void test_program_d() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0x00500093, // addi x1, x0, 5
        0x00500113, // addi x2, x0, 5
        0x00208463, // beq  x1, x2, +8
        0x03700193, // addi x3, x0, 55
        0x04D00193, // addi x3, x0, 77
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 5);
    ASSERT("CPU-D x3", top.dbg_x3, 77);
}

static void test_program_e() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0x00A00093, // addi x1, x0, 10
        0x008002EF, // jal  x5, +8
        0x06300113, // addi x2, x0, 99
        0x02A00113, // addi x2, x0, 42
        0x00028067, // jalr x0, 0(x5)
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 5);
    ASSERT("CPU-E x2", top.dbg_x2, 99);
}

static void test_program_f() {
    Vcpu top;
    reset(top);
    const uint32_t program[] = {
        0xABCDE0B7, // lui  x1, 0xABCDE
        0x12308093, // addi x1, x1, 0x123
    };
    load_program(top, program, sizeof(program) / sizeof(program[0]));
    run_cycles(top, 2);
    ASSERT("CPU-F x1", top.dbg_x1, 0xABCDE123);
}

static void test_program_g() {
    Vcpu top;
    reset(top);
    load_disassembly(top, "codes/instr/add_instr.txt");
    top.rst = 1;
    eval(top);
    write_reg(top, 2, 0x0001FFF0);
    top.rst = 0;
    eval(top);
    run_until_ecall(top, 64);
    ASSERT("CPU-G ecall", top.dbg_instr, 0x00000073);
    ASSERT("CPU-G a0", top.dbg_x10, 5);
}

int main() {
    test_program_a();
    test_program_b();
    test_program_c();
    test_program_d();
    test_program_e();
    test_program_f();
    test_program_g();

    printf("\n%s (%d failure%s)\n",
           failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
