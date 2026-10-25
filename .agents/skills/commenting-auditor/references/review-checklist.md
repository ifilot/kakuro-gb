# C and C++ documentation review checklist

Use alongside the actual repository `AGENTS.md`; its rules take precedence.

## Doxygen completeness (when required or used as the default)

- Each in-scope function and method has the documentation required by repository policy, at the appropriate declaration/definition. With the skill's Doxygen defaults, use a `/** ... */` block.
- `@brief` accurately communicates purpose without simply restating the name.
- Every parameter has the correct `@param[in]`, `@param[out]`, or `@param[in,out]` tag and a correct description.
- Non-void functions have an accurate `@return`, including meaningful error or sentinel values.
- Parameter names and return documentation match the current signature.
- Documentation is not unnecessarily copied to an out-of-line definition.

## Useful contract information, as applicable

- Preconditions, postconditions, validation, error behavior, and exceptional conditions.
- Ownership transfer versus borrowing, lifetime requirements, pointers that may be null, and invalidation rules.
- Thread affinity, synchronization expectations, callbacks, and reentrancy.
- Units, coordinate spaces, inclusive/exclusive bounds, encoding and locale assumptions.
- Resource/context requirements, including OpenGL, window, database and background-worker contexts.
- Side effects, persistence, state mutation, caching and order dependence.
- Stable IDs, on-disk compatibility and externally observable behavior.

## Comment quality

- Correct with respect to implementation and tests.
- Explains non-obvious rationale, invariants, or tradeoffs.
- No outdated TODO, obsolete implementation story, or contradictory claim.
- Avoids repetitive line-by-line narration, unsupported guarantees, and excessive prose.
- Does not repeat full design specifications better maintained in `DESIGN.md` or versioned documents; link instead when appropriate.

## Minimal examples (C++)

These illustrate comment quality; apply the same principles to C functions using the repository's C syntax and naming conventions.

Too vague:

```cpp
/**
 * @brief Gets the count.
 * @return The count.
 */
int GetCount() const;
```

More informative when grounded in behavior:

```cpp
/**
 * @brief Returns the number of unresolved cells in the current board.
 * @return Unresolved cell count; zero when the board is complete.
 */
int GetUnresolvedCellCount() const;
```

Do not invent semantics such as `zero when complete` unless verified.

For implementations, prefer a reason:

```cpp
// Retain the lease until queued writes finish, so the worker cannot use a closed connection.
DrainPendingWrites();
```

Avoid a narration:

```cpp
// Drain pending writes.
DrainPendingWrites();
```
