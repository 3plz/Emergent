--Module io;

#[Print]
arguments: {val;primitive;Integer, floating-point, string, boolean, or pointer value};
returns: {void;Prints string representation of the value to stdout followed by newline};
description: {Standard console output function. \
Supports automatic formatting of integers, floating-point numbers, \
strings, and pointers with a trailing newline.
};

-[Print] {val: i8} -> void
ExternC(em_io_print_int(val));

-[Print] {val: i16} -> void
ExternC(em_io_print_int(val));

-[Print] {val: int} -> void
ExternC(em_io_print_int(val));

-[Print] {val: i64} -> void
ExternC(em_io_print_i64(val));

-[Print] {val: u8} -> void
ExternC(em_io_print_uint(val));

-[Print] {val: u16} -> void
ExternC(em_io_print_uint(val));

-[Print] {val: uint} -> void
ExternC(em_io_print_uint(val));

-[Print] {val: u64} -> void
ExternC(em_io_print_u64(val));

-[Print] {val: f32} -> void
ExternC(em_io_print_float(val));

-[Print] {val: f64} -> void
ExternC(em_io_print_float(val));

-[Print] {val: str} -> void
ExternC(em_io_print_string(val));

-[Print] {val: bool} -> void
ExternC(em_io_print_uint(val));

-[Print] {val: *void} -> void
ExternC(em_io_print_pointer(val));

![em_io_print_int] {val: int} -> void
#include <stdio.h>

void em_io_print_int(int val) {
    printf("%d\n", val);
}

![em_io_print_uint] {val: uint} -> void
#include <stdio.h>

void em_io_print_uint(unsigned int val) {
    printf("%u\n", val);
}

![em_io_print_i64] {val: i64} -> void
#include <inttypes.h>
#include <stdio.h>

void em_io_print_i64(int64_t val) {
    printf("%" PRId64 "\n", val);
}

![em_io_print_u64] {val: u64} -> void
#include <inttypes.h>
#include <stdio.h>

void em_io_print_u64(uint64_t val) {
    printf("%" PRIu64 "\n", val);
}

![em_io_print_float] {val: f64} -> void
#include <stdio.h>

void em_io_print_float(double val) {
    printf("%f\n", val);
}

![em_io_print_string] {val: str} -> void
#include <stdio.h>

void em_io_print_string(const char *val) {
    puts(val);
}

![em_io_print_pointer] {val: *void} -> void
#include <stdio.h>

void em_io_print_pointer(void *val) {
    printf("%p\n", val);
}
