# Architecture Simplifier — Codex skill for C and C++

A reusable, read-only-by-default architectural audit and controlled refactoring workflow for C, C++, and mixed projects. It follows the current repository's `AGENTS.md` and selects language-specific checks without imposing C++ idioms on C.

## Install globally (recommended when using many repositories)

Extract the `architecture-simplifier` directory into:

- macOS / Linux: `~/.agents/skills/architecture-simplifier/`
- Windows: `%USERPROFILE%\.agents\skills\architecture-simplifier\`

The path must contain `SKILL.md` directly inside `architecture-simplifier`.

Example (macOS/Linux, from folder containing this skill directory):

```sh
mkdir -p "$HOME/.agents/skills"
cp -R architecture-simplifier "$HOME/.agents/skills/"
```

## Install in a single repository (good for teams)

Copy the entire directory to:

```
<repo>/.agents/skills/architecture-simplifier/
```

Commit `.agents/skills/architecture-simplifier/**` if you want everyone on the team to have the same version. Do not duplicate it in each repository unless version-locking or team sharing matters.

## How to invoke

- `$architecture-simplifier Audit this C repository and identify the highest-impact coupling and cohesion problems.`
- `$architecture-simplifier Audit this mixed C/C++ repository, including ownership and error handling across the language boundary.`
- `$architecture-simplifier Refactor mode: simplify the dependency structure with small, behavior-preserving, validated changes.`
- `$architecture-simplifier Release Gate: assess architecture readiness, risks, and regressions before handoff.`

The default mode is **Audit** (no file edits). You must explicitly request **Refactor mode** for modifications.

If Codex does not discover a newly installed skill, restart the Codex session. You can also inspect the skills selector.

## Should this go into the repository?

- **Personal standard across multiple codebases:** install globally.
- **Shared, reviewed policy for a team:** check it into the repository.
- **Best combined setup:** global skill + per-repo `AGENTS.md` specifying target architecture, supported C/C++ standards, API/ABI constraints, latency, toolchain, and release requirements.

A simple repository `AGENTS.md` addition:

```
# Architecture constraints
- Follow the existing dependency direction between libraries.
- Maintain public API/ABI contracts and supported C/C++ toolchains.
- Prefer low coupling and high cohesion; do not introduce speculative layers.
- For release handoff, use architecture-simplifier in Release Gate mode.
```

Note: an AGENTS.md instruction alone may not ensure a skill is automatically invoked in every workflow; explicitly invoke it when a gate matters.

## Contents

- `SKILL.md`: skill trigger, policy, process, and output contract.
- `references/c-audit-checklist.md`: C-specific investigation and mixed-language boundary checks.
- `references/cpp-audit-checklist.md`: C++-specific investigation checklist.
- `references/metrics-and-report.md`: evidence standards, metric definitions, severity and release verdict rubric.

No external dependencies, network access, or scripts are required. The skill works using the codebase and tools already accessible to Codex. It does not itself implement an automated CI gate.
