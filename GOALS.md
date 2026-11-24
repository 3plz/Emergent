# GOAL: Reliable C interoperability and library-driven FFI

Based on the current code and additional compiler probes. **P0 contains correctness
failures to fix before extending FFI.**

## P0 — Compiler correctness

Sources: [parser.c](src/parser/parser.c), [lexer.c](src/lexer/lexer.c),
[codegen.c](src/codegen/codegen.c).

- [x] **G01 — Preserve parsing errors.** Parsing a foreign C block currently resets
  `had_error`, allowing previously reported syntax errors to compile successfully.
- [x] **G02 — Guarantee parser progress.** A stray `}` after `return 0;` currently
  causes an infinite parsing loop.
- [ ] **G03 — Separate semantic validation from code generation.** Validate
  unreachable statements too. A direct C call after `return` currently bypasses
  the `ExternC` restriction.
- [ ] **G04 — Fix foreign scalar return handling.** Normalize signed and unsigned
  8/16-bit results and C boolean results according to the target ABI. A returned
  `signed char(-1)` currently fails `< 0`.
- [ ] **G05 — Implement short-circuit evaluation.** `false && function()` currently
  executes the function; `true || function()` has the same underlying problem.
- [x] **G06 — Fix constant pointer truthiness.** `if ("hello")` currently evaluates
  as false because a symbol address is parsed as an integer.
- [ ] **G07 — Implement consistent numeric conversions.** Correct integer
  promotions and signed/unsigned arithmetic. The reproduced `i8(-1) < u8(1)`
  comparison currently returns false.
- [ ] **G08 — Validate numeric literals completely.** Reject malformed `0x`,
  detect overflow, support the full `u64` range, and define supported suffixes and
  exponent syntax.
- [ ] **G09 — Make return inference respect lexical scopes.** A branch-local
  variable can currently change the inferred return type of an outer variable
  with the same name.
- [ ] **G10 — Enforce assignment and return type compatibility.** The compiler
  currently accepts a floating-point initializer for `*int` and an ordinary
  integer return from a pointer-returning function.
- [ ] **G11 — Define and validate return-path rules.** Non-void functions currently
  accept `return;` and silently generate zero for missing returns. Reject
  duplicate parameter names too.
- [ ] **G12 — Repair raw foreign-code scanning.** Braces inside C strings or
  comments must not terminate a block. Report unterminated comments and blocks.
- [ ] **G13 — Separate pointer ownership from pointer representation.**
  `pointer >> Null` currently generates `free` for any pointer, including stack
  addresses and string literals. Only owned allocations may be released.

## P1 — Library-driven FFI

Sources: [codegen.c](src/codegen/codegen.c),
[libc2mir.em](std/libc2mir/libc2mir.em).

- [ ] **G14 — Provide a generic native-library interface.** Support loading a
  library, resolving symbols, declaring signatures, and invoking native function
  addresses.
- [ ] **G15 — Make compiler operations library-declared and import-scoped.**
  Replace unconditional name checks for `ExternC` and `Syscall` with declarations
  resolved through the module system.
- [ ] **G16 — Define when foreign-code handlers execute.** Specify compiler-time
  processing versus program-time processing, and implement `.em` execution during
  compilation if handlers require it.
- [ ] **G17 — Add foreign-language handler registration.** Give `.em` handlers the
  block text, signature, source location, and a defined way to return callable
  code and diagnostics.
- [ ] **G18 — Implement the actual c2mir/MIR adapter.** Create contexts, compile C
  through `c2mir_compile`, load and link MIR modules, obtain executable functions,
  and release resources.
- [ ] **G19 — Define generated-code lifetime and execution location.** A JIT
  address created inside the compiler cannot simply be embedded into a separate
  executable. Design the runtime or artifact interface explicitly.
- [ ] **G20 — Establish the canonical module name and dependency.** Resolve
  `std/c2mir` versus existing `std/libc2mir`, provide compatibility if needed, and
  make `std/io` import the module supplying `ExternC`.
- [ ] **G21 — Implement or remove advertised library operations.** `Include` and
  `Compile` currently have documentation but no implementation.

## P1 — C compatibility requirements

Sources: [types.c](src/codegen/types.c), [ast.h](src/parser/ast.h).

- [ ] **G22 — Define “100% C-compatible” precisely.** Specify supported C
  standards, target ABIs, memory layout, and which Emergent semantics intentionally
  differ from C.
- [ ] **G23 — Implement platform-dependent C types.** Correctly represent `char`,
  `short`, `long`, `size_t`, `ptrdiff_t`, qualifiers, and typedef aliases. Unknown
  names currently become four-byte integers.
- [ ] **G24 — Implement aggregate types and layout.** Add structures, unions,
  enums, arrays, alignment, padding, bit fields, and aggregate argument/return
  handling.
- [ ] **G25 — Implement function types and callbacks.** Support typed function
  pointers, indirect calls, C callbacks, and variadic calls with correct argument
  promotions.
- [ ] **G26 — Verify foreign signatures against their implementation.** `.em`
  declarations and raw C definitions currently can disagree without a diagnostic.
- [ ] **G27 — Complete required language constructs.** Implement loops, explicit
  casts, indexing, pointer arithmetic, bitwise operations, and size/alignment
  queries. Several keywords are recognized without corresponding implementation.

## P2 — Modules, paths, and build behavior

Sources: [module.c](src/parser/module.c), [driver.c](src/driver/driver.c),
[main.c](src/main.c).

- [x] **G28 — Reject invalid or missing imports.** Missing modules currently
  disappear silently. Additional import arguments are ignored, and imports inside
  conditionals are skipped.
- [ ] **G29 — Implement explicit module dependencies and selective imports.**
  Preserve each module's source directory and dependency graph instead of relying
  on merged global declarations.
- [ ] **G30 — Define module initialization behavior.** Imported `Init` blocks are
  currently discarded; ordinary initialization code cannot load or configure an
  FFI backend.
- [ ] **G31 — Fix path storage and resolution.** `realpath` writes into a fixed
  512-byte buffer. Remove unsafe buffers and silent truncation; consistently
  resolve standard-library paths and relative C headers.
- [ ] **G32 — Remove silent collection limits.** Replace limits of 128 modules,
  32 libraries, 32 recorded returns, 32 grouped variables, and 64 documentation
  matches with growing collections or explicit errors.
- [ ] **G33 — Propagate execution failures.** `emc --run` currently returns
  success when the program exits with code `7`. Handle signals, failed execution,
  and interrupted waits.
- [x] **G34 — Validate command-line arguments.** Reject unknown flags, missing
  option values, and missing source files instead of falling back to
  `references/hello.em`.
- [ ] **G35 — Configure the toolchain explicitly.** Support tool paths, temporary
  directories, include/library paths, linker options, and consistent host/target
  settings.
- [ ] **G36 — Handle temporary-file I/O failures.** Check C-file creation and every
  write, handle partial writes, and preserve the actual process error instead of
  replacing it with misleading exit-code messages.

## P3 — Documentation and verification

- [ ] **G37 — Match documentation to implemented behavior.** Remove current
  claims of working c2mir JIT and zero-overhead direct syscalls until verified. The
  current syscall path calls libc, whose error convention differs from the
  documentation.
- [x] **G38 — Correct documentation output.** Preserve pointer depth and return
  types, show all requested foreign entries, and remove example-file lookup
  fallbacks and silent result truncation.
- [ ] **G39 — Add regression tests for the reproduced failures above.** Include
  malformed-input timeouts, unreachable foreign calls, import failures, numeric
  boundaries, scope inference, and ABI return values.
- [ ] **G40 — Establish measurable compatibility checks.** Compare defined
  programs and ABI probes against C compilers on supported targets. Track
  compatibility by tested requirements.
- [ ] **G41 — Keep implementation conventions consistent.** Use GNU comment style
  and 1TBS, and update documentation and tests whenever language behavior changes.
