--Module Docmodule;

// Documentation submodules describe arguments, return values, and behavior.

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

// Argument and return entries use name;type;description, separated by backslashes.
// In descriptions, a backslash inserts a line break.
// View this documentation with: emc man references/docmodule.em LibraryFunction
