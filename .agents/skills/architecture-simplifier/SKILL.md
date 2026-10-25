---
name: architecture-simplifier
description: Audit and simplify C and C++ architectures for low coupling, high cohesion, clear ownership, lean abstractions, header/build dependency hygiene, and localized change impact. Use for architectural reviews, refactoring proposals, and pre-release handoffs.
---

# C and C++ Architecture Simplifier

## Purpose

Find the **simplest maintainable architecture that satisfies actual requirements**. Challenge unnecessary wrappers, interfaces, dependencies, layers, and build coupling. Preserve legitimate abstraction and module boundaries. Prefer evidence over style preferences and do not optimize metrics blindly.

## Modes and safety

- **Audit (default):** Read-only investigation and prioritized report. Do not edit files.
- **Refactor:** Investigate, propose small high-value changes, implement incrementally, and validate. Only use this mode when explicitly requested. Do not perform unrelated rewrites.
- **Release Gate:** Read-only review focused on release readiness, architectural regressions, high-severity debt, and verification gaps.

If mode is omitted, use **Audit**. If requirements or project invariants are uncertain, explicitly mark assumptions; do not invent guarantees. Respect repository `AGENTS.md` and documented architecture constraints.

## Workflow

### 1. Discover before judging

- Identify build systems (CMake, Meson, Bazel, Make, etc.), languages, language standards, and compiler extensions per target. Use build configuration to resolve ambiguous headers; do not assume every `.h` file is C or C++.
- Map libraries/targets, modules, translation units, major types and functions, public interfaces, ownership boundaries, and major data/control flows. Include namespaces and classes for C++.
- Locate tests, CI, formatting/static-analysis config, benchmarks, compatibility rules, platform constraints, and release criteria.
- Identify generated/vendor code and keep it out of refactoring scope unless directly relevant.
- Read [c-audit-checklist.md](references/c-audit-checklist.md) for C components and [cpp-audit-checklist.md](references/cpp-audit-checklist.md) for C++ components. For mixed projects, use both and inspect the language boundary. Do not impose C++ idioms on C or recommend a language migration without a requirement.
- Use [metrics-and-report.md](references/metrics-and-report.md) for evidence and reporting.

### 2. Examine dependency structure

Find module/function-level fan-in/fan-out (and class-level relationships in C++), dependency cycles, long call/dependency chains, excessive concrete dependencies, shared mutable state, layered indirection, broad public APIs, and confused ownership. Examine header includes, transitive includes, exported headers/symbols, and target-level dependency directions, plus template instantiation in C++. Consider compile-time *and* runtime costs.

### 3. Examine cohesion and abstraction

Ask of each important component:

- What responsibility does it actually own?
- Does related state and behavior live together?
- Are unrelated concerns bundled together?
- Is logic duplicated or split across artificial wrappers/managers/helpers?
- Would **merge**, **split**, **move**, **hide**, or **delete** make a change easier to understand?
- Does an opaque handle, callback table, macro abstraction, factory, or forwarding layer have a concrete reason to exist? In C++, also examine virtual hierarchies, templates, and PImpl.

Do not automatically reject single-implementation interfaces, dependency injection, forward declarations, opaque types, or C++ inheritance/PImpl; account for testing boundaries, ABI, translation-unit dependencies, plugin systems, polymorphism, and platform needs.

### 4. Test change locality

Trace at least 2–3 representative potential changes when possible: modifying a domain rule, replacing an implementation, and adding a capability or field. Note which production modules/types/headers/targets need to change, versus tests, generated files, and unavoidable API or serialization boundaries. Report change-propagation hotspots.

### 5. Rank proposed improvements

Prioritize by: (a) actual maintenance pain or change blast radius, (b) reduction of unnecessary coupling, (c) cohesion and ownership improvement, (d) simplicity gain, (e) risk/effort. Prefer targeted improvements over sweeping rewrites. Explain the tradeoffs for every material recommendation.

### 6. Verify with available tools

If feasible, inspect compiler include traces, `compile_commands.json`, include-what-you-use, clang-tidy, build target graphs, and test/build output. Only run tools that are installed and suitable for the repo. Never claim an inferred or sampled metric is complete or measured. Do not change build configuration solely to measure coupling without permission.

For **Refactor** mode: establish baseline, perform small coherent edits, run available builds/tests, and reassess. Respect API/ABI, memory ownership, thread safety, real-time constraints, error handling and cleanup (including exception safety in C++), and performance invariants. Report what was *not* verified.

## Output contract

Return an actionable report with:

1. **Scope & confidence:** inspected directories/targets; skipped code; tools available.
2. **Architecture map:** short overview of key target/module dependencies and boundaries.
3. **Prioritized findings:** severity, precise file/line evidence, actual dependency path or responsibility conflict, and reason it matters.
4. **Recommendations:** minimal proposed change, alternatives considered, expected benefit, risks, and rough effort.
5. **Metrics:** real numbers with method/scope, or explicitly marked estimates; before/after only if observed.
6. **Change-locality analysis:** concrete scenarios and expected files/components affected.
7. **Release verdict (Release Gate only):** `Ready`, `Ready with caveats`, or `Not ready`, with evidence and any blocking risks. This is an engineering assessment, not a guarantee.
8. **Validation:** exact build/test/analyzer commands run, outcomes, limitations. For Audit/Release Gate, state that files were unchanged.

Prefer the top 5–10 issues with the highest payoff over a giant generic checklist. Include positive findings when they explain why an existing boundary should remain.

## Non-negotiable principles

- Optimize for **low coupling, high cohesion, clear ownership, few justified moving parts, and localized changes**.
- Minimize **total complexity**, not simply module/class count, include count, abstraction count, or lines of code.
- Avoid speculative abstraction, cargo-cult SOLID, overengineering, and cosmetic churn.
- Do not exchange compile-time improvements for poor runtime behavior or ownership ambiguity without stating the tradeoff.
- Never change behavior, stability contracts, or performance-sensitive architecture without explicit justification and verification.
