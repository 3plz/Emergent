--Module MathCTest;

-[Init]
mod.add(std:libc2mir);
mod.add(std:io);

var x: int = 64;
var root: int;

ExternC(C_SquareRoot(x)) >> root;
io.Print(root); // 8

![C_SquareRoot] {n: int}
#include <math.h>

int C_SquareRoot(int n) {
    return (int)sqrt((double)n);
}
