--Module libc2mir;

#[libc2mir]
description: {C compiler and JIT driver module powered by Vladimir Makarov's MIR (Medium Internal Representation) and c2mir. \
Enables seamless integration with C submodules (![Name]), C99/C11 inline compilation, header inclusion, external C symbol resolution, and fast in-memory execution.
};

#[ExternC]
arguments: {call;any;Foreign C function call expression};
returns: {any;Result returned by the C function};
description: {Executes or links a foreign C submodule defined with ![Name] and passes arguments according to C calling conventions.
};

#[Include]
arguments: {header;str;Header file to include (e.g. '<stdio.h>' or 'myheader.h')};
returns: {void;Registers the header in the C compilation context};
description: {Registers C standard library or custom header files for foreign C submodules.
};

#[Compile]
arguments: {source;str;C source code string to compile};
returns: {ptr;Function pointer or module handle to the compiled machine code};
description: {Compiles raw C source code on the fly in memory using the c2mir JIT compiler engine.
};

-[Init]
mod.link("mir", "m", "dl", "pthread");
