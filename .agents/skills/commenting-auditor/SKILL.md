---
name: commenting-auditor
description: Audit and improve C and C++ comments and Doxygen documentation according to the current repository's policy. Use for documentation quality reviews, API documentation cleanup, commenting standards, and documentation release checks. Do not use for ordinary code edits unless explicitly requested.
---

# C and C++ Commenting Auditor

## Purpose

Improve the **correctness, usefulness, and maintainability** of C and C++ comments while respecting the current repository's `AGENTS.md` and documented coding and documentation conventions.

This skill is about documenting intent and contracts, not maximizing the number of comments. Do not make unrelated code or architectural changes.

## Modes

- **Audit (default)**: review and report findings without editing source files.
- **Fix**: apply scoped comment/documentation improvements with behavior-preserving edits.
- **Release Gate**: assess documentation regressions and material documentation risks in changed or public-facing code, expanding scope only when justified.

## Repository policy and language selection

Before auditing, read the current repository `AGENTS.md` and relevant design documents. Identify the languages and supported standards from the build configuration; do not infer a header's language from `.h` alone. Follow the destination repository's style guide; do not impose Google C++ style on C.

Apply the repository's documentation requirements. When it has no documented convention, use the following Doxygen defaults for the scoped review; do not expand the scope into a repository-wide conversion:

- Use Doxygen `/** ... */` immediately before function or method declarations, or definitions when there is no separate declaration.
- Put caller-facing documentation at header declarations; avoid repeating it at out-of-line implementation definitions.
- Require `@brief`; document each parameter with `@param[in]`, `@param[out]`, or `@param[in,out]` as appropriate; use `@return` for non-void functions to explain the return value and failure indicators. Omit tags that have no matching parameter or return.
- Explain constraints and non-obvious behavior where relevant.
- Keep comments aligned with signatures, behavior, and actual invariants.
- Use implementation comments for non-obvious reasoning.
- Do not treat Doxygen generation as an authorized change unless separately requested.

Read [review-checklist.md](references/review-checklist.md) for shared audit criteria. Also read [c-contract-checklist.md](references/c-contract-checklist.md) for C APIs and [cpp-contract-checklist.md](references/cpp-contract-checklist.md) for C++ APIs. For mixed projects, use both where applicable, including cross-language ownership and error contracts.

## Audit procedure

1. Establish scope: changed files by default, or the named module / entire repository when explicitly requested.
2. Read APIs, implementations, tests, and relevant design documents before asserting intent.
3. Identify missing mandatory documentation; stale or factually incorrect comments; conflicting documentation; missing ownership/lifetime, nullability, thread-safety, error-handling, units, indexing, performance, or state-transition contracts where those affect callers.
4. Identify misleading documentation: comments that promise stronger guarantees than the code provides, describe historical behavior as current, or disagree with tests and public signatures.
5. Flag comments that merely narrate implementation mechanics. Prefer concise, truthful wording, but do not remove comments required by `AGENTS.md`.
6. Report findings with file paths and line numbers, severity, supporting evidence, and small proposed fixes.
7. Distinguish verified facts from inferred intent. When contract intent is ambiguous, flag the ambiguity rather than inventing a guarantee.

## Fix mode

- Modify only the relevant documentation and comments unless the user authorizes code changes.
- Maintain the repository's documentation syntax (Doxygen by default) and exact signature alignment.
- Avoid repeating identical documentation in headers and source definitions (`.c`, `.cc`, `.cpp`, etc.).
- Prefer an accurate, concise contract over lengthy boilerplate.
- Document *why* in implementations, *what callers can rely on* at API boundaries.
- Treat ownership and lifetime, borrowing, thread-safety, units, coordinate systems, exceptional conditions, and side effects as important contract elements where applicable.
- Do not turn undocumented assumptions into guarantees.
- Preserve formatting and nearby style; do not reflow unrelated files.
- Check the diff; run lightweight syntax/documentation checks if available; run builds/tests if any non-comment code changed.
- Review `CHANGELOG.md` under the repository policy. Comment-only corrections usually should not produce a changelog entry unless they change documented behavior.

## Results

Report:

1. Scope and documents examined.
2. Findings ranked Critical / Major / Minor, each with a location and concise explanation.
3. Proposed or applied fixes and why they improve correctness or maintainability.
4. Contradictions or ambiguous contracts requiring an engineering decision.
5. Checks performed, scope gaps, and final verdict.

Never claim to have verified a behavior you only inferred.
