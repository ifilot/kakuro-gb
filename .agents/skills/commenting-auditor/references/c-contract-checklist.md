# C API contract checklist

Apply only where relevant to callers and verified against implementation and tests. Respect the project's C standard and documentation policy.

- State pointer nullability, including whether null is allowed for a zero-length buffer. Describe readable/writable regions and any overlap or aliasing restrictions.
- Distinguish buffer capacity from data length; state units (bytes, elements, code points), whether a terminator is included, and string termination or truncation behavior.
- Distinguish allocated, borrowed, and transferred resources. Identify who releases returned storage/handles, which release function or allocator is required, and how long borrowed pointers remain valid.
- Explain success/error return conventions, sentinel values, and `errno` behavior only where the API actually uses it. State whether output parameters remain unchanged, are cleared, or contain partial results on failure.
- Document initialization requirements, valid lifecycle states, partial-initialization cleanup, and whether destroy/reset operations accept null or repeated calls when these affect callers.
- For callbacks, explain context-pointer lifetime, invocation timing/thread, reentrancy, retention, and unregistration guarantees where applicable.
- Document public macros' argument evaluation and side effects when relevant; do not describe function-like macros as ordinary functions if behavior differs.
- For APIs shared with C++, document the same C ownership and error contract on both sides. Verify any claim that exceptions are translated or contained at the boundary.

Do not introduce C++ terminology such as references, smart pointers, destructors, or thrown exceptions into a pure C contract. Do not infer ownership merely from pointer types or `const`.
