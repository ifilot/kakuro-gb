# Evidence and reporting rubric

## Metrics, ordered by usefulness

1. **Dependency cycles:** concrete cycles across targets/modules/functions/classes, with exact graph scope.
2. **Change blast radius:** representative change -> production units affected, why, and unavoidable boundaries.
3. **Target coupling:** direct dependencies and inappropriate layer crossings (with build-system evidence).
4. **Header coupling:** direct includes, transitive chains and especially widely included heavyweight headers.
5. **Component coupling:** module/function fan-in/fan-out and, for C++, class collaborators; distinguish calls, type/storage dependencies, callback relationships, and inheritance where applicable.
6. **Abstraction count:** wrappers, forwarding hops, indirection depth where actual maintenance cost is evident.
7. **Ownership conflicts:** duplicated state/rules and ambiguous authoritative owner.

Raw numbers should include numerator/denominator, definition, scope, and method. Avoid invented health scores. Compare baselines only using like-for-like definitions.

## Severity

- **Critical:** demonstrable architectural defect threatening correctness, safety or maintainability of release-critical components (e.g. ownership/lifetime hazards, dangerous cycles).
- **High:** recurring change amplification, tangled module boundaries, or major architectural bottleneck with supported examples.
- **Medium:** localized coupling/cohesion debt with clear, low-risk payoff.
- **Low:** small quality concern; avoid cosmetic churn.

## Suggested finding template

- **Finding:** short actionable title; severity and confidence.
- **Evidence:** file paths/line ranges and exact relationship (not just component names).
- **Impact:** scenario that exhibits maintenance/test/build or change-locality pain.
- **Smallest fix:** precise merge/split/move/delete/hide adjustment.
- **Counterargument:** what benefits current architecture provides, and why change still wins.
- **Risk & validation:** API/ABI, runtime, ownership, tests, build, performance, migration.

## Release gate

- **Ready:** no known material architectural blockers within inspected scope, sufficient verification for reviewed changes.
- **Ready with caveats:** manageable debt or incomplete visibility; caveats explicit.
- **Not ready:** material risk or unacceptable verification gap with concrete basis.

Do not equate no findings with proof of quality. Say what was not reviewed.
