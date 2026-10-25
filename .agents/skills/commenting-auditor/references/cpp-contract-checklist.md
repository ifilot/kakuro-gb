# C++ API contract checklist

Apply only where relevant to callers and verified against implementation and tests. Respect the project's C++ standard and documentation policy.

- Distinguish value semantics, borrowing through pointers/references/views, and ownership through smart pointers or explicit transfer. `const` alone does not establish lifetime or thread safety.
- Explain reference, iterator, span/view, and returned-pointer invalidation where callers depend on it, including invalidation by moves or container mutation.
- Describe constructor/factory failure, exceptions and guarantees on failure where material. Do not infer a strong exception guarantee from RAII alone.
- Document observable copy/move behavior, moved-from state, and destruction requirements only where these impose caller constraints beyond normal language semantics.
- Explain callback/lambda capture lifetime, asynchronous retention, and ownership cycles where they affect callers.
- Document template requirements and specialization/overload constraints when signatures and existing concepts do not sufficiently express the contract.
- For APIs exposed to C, verify the C-facing resource-release and error contracts; do not promise C++ exceptions or types to C callers.
