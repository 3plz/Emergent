--Module TestC;

-[Init]
mod.add(std:libc2mir);
mod.add(std:io);

var a: int = 7;
var b: int = 3;
var res: int;

ExternC(C_MultiplyAndSum(a, b)) >> res;
io.Print(res); // Prints 121: (7 * 3) + 100.

![C_MultiplyAndSum] {x: int, y: int}
#include <stdio.h>

int C_MultiplyAndSum(int x, int y) {
    int mult = x * y;
    printf("[C block stdout]: multiplying %d * %d = %d\n", x, y, mult);
    return mult + 100;
}
