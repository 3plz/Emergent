--Module DirectCCall;

-[Init]
var result: int;
C_Add(2, 3) >> result;
return result;

![C_Add] {a: int, b: int} -> int
int C_Add(int a, int b) {
    return a + b;
}
