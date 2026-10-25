# C architecture audit checklist

Use context-dependent judgment; none of these is an unconditional mandate. Apply the project's supported C standard and compiler extensions.

## Headers and compilation

- Public headers expose only necessary functions, types, constants, and macros. Internal functions and data have internal linkage where appropriate.
- Headers are self-contained and guarded; code does not depend accidentally on transitive includes or include order.
- Identify avoidable include chains, broad umbrella headers, and public structure layouts that unnecessarily propagate implementation changes.
- Use incomplete struct declarations and opaque handles when ownership, ABI stability, or rebuild isolation justifies them; do not hide simple value types reflexively.
- Header definitions, `inline` usage, and linkage match the supported C standard and do not create duplicate symbols or unintended per-translation-unit state.
- Configuration macros have a clear owner and consistent values across translation units. Inspect conditional-compilation variants relevant to the reviewed targets.

## Ownership, APIs, and runtime behavior

- Each allocation, file, lock, handle, and other resource has an explicit owner and matching release operation. Borrowing and ownership transfer are clear.
- Initialization, partial-initialization failure, normal cleanup, and early-return paths preserve invariants without leaks, double release, or use after release. Cleanup labels are acceptable when they simplify these paths.
- Allocator/deallocator pairs and allocation domains remain compatible across libraries and callbacks.
- Buffer APIs make capacity, length, units, string termination, nullability, and aliasing constraints clear. Status returns and output parameters have consistent success/failure semantics.
- Callback function signatures, context-pointer lifetimes, registration/unregistration, and reentrancy expectations are explicit. Function-pointer tables have a concrete purpose.
- Avoid unnecessary global mutable state, hidden singleton dependencies, and unmanaged lifetime coupling. Explicit context structs are useful when they represent coherent owned state.
- Threading, locks, asynchronous work, and callbacks do not create hidden ownership or synchronization dependencies.
- Macro-generated interfaces and `_Generic` dispatch, where supported, justify their complexity; prefer ordinary functions when they preserve required semantics.
- Performance, allocation, binary size, and latency tradeoffs are respected in hot paths.

## Component design

- Related functions and data form cohesive modules; public structs and context objects do not bundle unrelated responsibilities.
- Avoid redundant forwarding modules and layers, but preserve meaningful platform, persistence, domain, and testing boundaries.
- Duplicate business rules, conversion logic, validation, or state indicate competing sources of truth.
- Build targets follow intended dependency direction; public/private dependencies and exported symbols match actual API boundaries.
- Tests target stable behavior; do not introduce callback tables or other indirection solely for mocking without a concrete benefit.

## Mixed C/C++ boundaries, when present

- Headers intended for both languages use compatible declarations and appropriate C linkage guards; compile them in each supported language mode when feasible.
- C++ exceptions do not escape into C callers. Wrappers translate failures to the documented C error contract.
- Boundary types, calling conventions, struct layouts, and resource-release functions preserve the required ABI. C linkage alone does not make C++ types C-compatible.

## Evidence-gathering examples (optional)

- `rg --files -g 'CMakeLists.txt' -g 'meson.build' -g 'Makefile' -g '*.h' -g '*.c' -g 'AGENTS.md'`
- `rg '#include' include src` (adapt paths)
- Inspect `compile_commands.json`, compiler include traces, target graphs, and symbol tables where available.
- Build affected targets and run relevant tests using the project's configured tools. Use compiler analyzers or sanitizers only when installed and suitable for the target.

Do not confuse include-edge counts with semantic dependencies or target-level dependencies. Report each separately.
