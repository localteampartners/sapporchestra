# Prompt — performance smoke

**Role.** You are doing a performance smell-check by reading code, not a
benchmark. Flag the order-of-magnitude mistakes; leave micro-optimization
alone. No profiling infra is assumed or installed.

**Scope.** Hot paths: request handlers, render loops, startup, anything
inside a loop over user data. Skip cold paths (one-shot scripts, admin
tooling) unless egregious.

## Method

Look for the classics, and only file what you can point at:

1. **N+1 and friends.** Query-per-row loops; fetch-in-a-map over IDs;
   API calls inside iteration that batch endpoints could replace.
2. **Unbounded work.** Queries with no LIMIT feeding lists; loading whole
   tables/files into memory; missing pagination on growing collections.
3. **Blocking the hot path.** Sync filesystem/network calls in request
   handlers or UI threads; sequential awaits that are independent
   (could be parallel); sleeps in loops.
4. **Repeated work.** Recomputing invariants inside loops; re-reading
   config/files per request; missing memoization where inputs repeat;
   O(n²) scans where a map/set is natural.
5. **Growth.** Caches without eviction; listeners registered per call and
   never removed; logs/arrays appended forever.
6. **Payload size.** Whole-object serialization where a projection is
   needed; obviously heavy dependencies imported on hot startup paths.

For each: estimate the cost in one sentence ("N queries per page load,
N = cart size"). Severity: user-visible today → sev2; bites at 10x scale →
sev3; tidiness → sev4. sev1 only for active outage-class behavior.

## Output contract

- One finding per issue: `findings/F-NNNN-<slug>.md` from
  `templates/finding.template.md`, `category: perf`, repro = the code path
  and the input size that hurts, validated against
  `schemas/finding.schema.json`.
- Verdict + sync per PLAYBOOK C3/C4 when run standalone.
