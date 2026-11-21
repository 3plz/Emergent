--Module Libc2mirTest;

-[Init]
mod.add(std:libc2mir); // Import the C compiler and JIT library.

var a, b: int;
a, b = 5;

var x: int;

ExternC(C_Calculate(a, b)) >> x; // Call the C function and store its result in x.

mod.add(std:io);

io.Print(x); // Prints 10.

![C_Calculate] {a: int, b: int}
// Define a C function in a foreign-language submodule.
int C_Calculate(int a, int b){
    int c = a + b;
    return c;
}
