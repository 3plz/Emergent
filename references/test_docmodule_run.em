--Module TestDoc;

-[LibraryFunction] {a: i8, b: i8}
var x: i8;
a + b >> x;
return x;

#[LibraryFunction]
arguments: {a;i8;Первый аргумент сабмодуля\ b;i8;Второй аргумент сабмодуля};
returns: {x;i8;Результат сложения 2+2};
description: {Складывает 2 числа, переносит \
    Результат в функцию x затем возращает его.
};

-[Init]
mod.add(std:io);
var r: i8;
LibraryFunction(20, 22) >> r;
Print(r);
