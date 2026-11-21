# Function overloads

Functions in one module may share a name when their parameter types or counts
differ. The compiler chooses an overload from the argument types at the call site.

```em
--Module example;

-[Describe] {value: int} -> str
return "integer";

-[Describe] {value: str} -> str
return "string";

-[Init]
mod.add(std:io);
io.Print(Describe(42));
io.Print(Describe("hello"));
return 0;
```

This prints `integer` and `string`, each on its own line.

An exact type match takes precedence over a conversion. Widening within the same
numeric family takes precedence over other numeric conversions. An overload wins
only when it converts no argument worse than another candidate and converts at
least one argument better. If several candidates remain, the call is ambiguous
and compilation fails. Arguments are converted to the selected parameter types;
out-of-range integer literals are rejected.

`int` and `i32` are the same type for overload resolution, as are `uint` and `u32`.
Defining overloads that differ only by those aliases or their return types is an
error. Pointer depth matters: `*int` and `**int` are distinct. Typed pointers may
convert to `*void`; an exact pointer overload takes precedence.

Qualified calls such as `io.Print(value)` resolve in the named module. For an
unqualified call, declarations in the current module shadow imported functions
with that name. Conflicting imported candidates produce an ambiguity error.

Return types may be declared with `-> type` or inferred from return expressions.
Declared signatures are available before function bodies are generated, including
calls to functions defined later in the file. Use explicit return types for
recursive functions when inference cannot determine the intended type.

## Standard output

`std/io/io.em` implements `Print` as ordinary overloads for signed and unsigned
integers, `f32`, `f64`, `str`, `bool`, and `*void`. Each call takes one value and
prints a trailing newline. Booleans print as `0` or `1`. Import the module with
`mod.add(std:io)` or use `sub.add(std:io:Print)` for unqualified calls.

Formatting and libc calls live in foreign C helpers in that module. Each `Print`
overload invokes its helper through `ExternC`. C handles `printf`'s variadic
argument rules. The compiler has no special handling for the name `Print`, and
programs may define their own function with that name.

## Foreign functions and void

Foreign `![Name]` functions use their declared parameter types and optional
`-> type` return type. An omitted foreign return type retains the existing `int`
default. Calls to undeclared functions are errors; there is no name-based guess
for a C function's return type. C functions cannot be overloaded because they
retain their linker symbol names.

Emergent code must call foreign C functions through `ExternC(C_Function(args))`.
A direct `C_Function(args)` call is a compilation error. `ExternC` accepts exactly
one call to a declared foreign C function, including a qualified call. It cannot
wrap an ordinary Emergent function or an arbitrary expression. Any foreign calls
inside the arguments need their own `ExternC` wrapper:

```em
ExternC(C_Outer(ExternC(C_Inner())));
```

Use `-> void` for functions that do not return a value. A void function may end
without a return statement or use `return;`. Void calls can be statements, but
their result cannot initialize a variable, participate in arithmetic, or be used
as an argument or condition.
