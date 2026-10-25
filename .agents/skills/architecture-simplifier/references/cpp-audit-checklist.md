# C++ architecture audit checklist

Use context-dependent judgment; none of these is an unconditional mandate.

## Headers and compilation

- Public headers expose only necessary interfaces and types; implementation details remain private where feasible.
- Headers are self-contained; code does not depend accidentally on transitive includes.
- Unused includes, large umbrella headers, avoidable header dependency chains, inline-heavy implementation, and template instantiation hotspots are identified.
- Forward declarations are used only where types can legally remain incomplete (consider inline destructors, member storage, templates, type traits, and unique_ptr incomplete-type constraints).
- PImpl is considered when ABI boundaries or rebuild isolation justify complexity; not added reflexively.
- C++20 modules are considered only if toolchain/build ecosystem support and project needs justify migration.

## Ownership, APIs, and runtime behavior

- Ownership and lifetimes are clear: values, references, `unique_ptr`, `shared_ptr`, `weak_ptr`, raw observing pointers and handles have intentional meaning.
- RAII, rule of zero, strong invariants, and exception safety are appropriate for the component.
- No unnecessary global mutable state, singleton dependency, hidden service locator, or unmanaged lifetime coupling.
- Virtual dispatch, interfaces, inheritance, template polymorphism, std::function, and type erasure are justified rather than automatic.
- Dependencies expose minimal stable capabilities; public APIs do not leak avoidable concrete types or internal data structures.
- Threading, locks, asynchronous work, callbacks, and ownership across threads do not create hidden coupling.
- Performance, allocation, binary size, or latency tradeoffs are respected in hot paths.

## Component design

- Each class owns a coherent concept; responsibilities do not overlap or form a distributed workflow for simple operations.
- Avoid redundant Manager/Service/Helper/Facade/Factory/Adapter layers, but preserve meaningful isolation boundaries.
- Merge tiny wrappers only when it clarifies responsibility; split god objects only when new boundaries are coherent.
- Duplicate business rules, conversion logic, validation, or state indicate competing sources of truth.
- Build targets follow intended dependency direction; public/private target dependencies are accurate.
- Tests target stable behavior; architecture doesn't exist purely to facilitate mocking.

## Evidence-gathering examples (optional)

- `rg --files -g 'CMakeLists.txt' -g '*.hpp' -g '*.h' -g '*.cpp' -g '*.cc' -g 'AGENTS.md'`
- `rg '#include' include src` (adapt paths)
- `cmake --build <build-directory>` (only if configured)
- `ctest --test-dir <build-directory> --output-on-failure` (only if configured)
- `clang-tidy` / include-what-you-use / `clang-scan-deps` when installed and configured

Do not confuse `#include` edge counts with semantic dependencies or target-level dependencies. Report each separately.
