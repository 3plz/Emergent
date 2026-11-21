--Module Mirc;

-[Init]
mod.add(std:mirc); // Библиотека MIR которая является C JIT Compiler'ом 

var a, b: int;
a, b = 5;

var x: int;

ExternC(C_Calculate(a, b)) >> x; // Функция ExternC из библиотеки std:mirc вызывает сабмодуль другого языка* и записывает результат в x

mod.add(std:io);

io.Print(x); // 10

![C_Calculate] {a: int, b: int}
// Inline C Code
int C_Calculate(int a, int b){
    int c = a + b;
    return c;
}

// *Сабмодули начинающиеся с ![] являются сабмодулями другого языка, правила стандартного Emergent на них не распростроняются, одна из причин отделения через ![] это просто напросто что бы компилятору было легко отделить эмерджент код от другого, есть еще один тип сабмодуля про который раскожу позже
