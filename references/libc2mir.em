--Module Libc2mirTest;

-[Init]
mod.add(std:libc2mir); // C compiler and JIT driver library

var a, b: int;
a, b = 5;

var x: int;

ExternC(C_Calculate(a, b)) >> x; // Call foreign C function and move result to x

mod.add(std:io);

io.Print(x); // 10

![C_Calculate] {a: int, b: int}
// Inline C Code
int C_Calculate(int a, int b){
    int c = a + b;
    return c;
}
