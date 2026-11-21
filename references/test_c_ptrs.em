--Module PtrCTest;

-[Init]
mod.add(std:libc2mir);
mod.add(std:io);

var val: int = 10;
var p: *int = &val;

ExternC(C_DoubleIt(p));
io.Print(val); // Prints 20 after the C function updates val through p.

![C_DoubleIt] {ptr: *int}
void C_DoubleIt(int *ptr) {
    if (ptr) {
        *ptr = (*ptr) * 2;
    }
}
