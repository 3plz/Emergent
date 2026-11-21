--Module TestDoc;

-[LibraryFunction] {a: i8, b: i8}
var x: i8;
a + b >> x;
return x;

#[LibraryFunction]
arguments: {a;i8;First addend\ b;i8;Second addend};
returns: {x;i8;Sum of a and b};
description: {Adds a and b, stores the result in x, \
    and returns x.
};

-[Init]
mod.add(std:io);
var r: i8;
LibraryFunction(20, 22) >> r;
Print(r);
