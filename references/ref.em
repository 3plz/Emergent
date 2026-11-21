--Module ref;

-[Init]
// Demonstrate value transfer with the >> operator.
var x: cint = 7; // A C-compatible integer.
var y: cint;
sub.add(std:io:Print); // Import the Print submodule directly.
Print(x); // Prints 7.
x >> y;
Print(x); // Invalid use: x has been moved into y.
Print(y); // Prints the transferred value: 7.
