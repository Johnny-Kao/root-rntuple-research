# ROOT P1 CI failure audit — 2026-11-13

Scope: research-only. No upstream changes and no automatic costly retry.

## Latest run #43424659436

- Source SHA: `5281bc2342c5cba711cb507f4ef53c623ba4f942`, ROOT 6.43.01.
- ROOTNTuple target **built successfully** (3,607 Ninja build steps).
- Test source compiled; test binary *linking* failed after approximately 72 minutes because `$(root-config --libs)` requests libraries not built by the scoped target (`-lHist`, `-lGraf`, `-lTree`, `-lROOTNTupleUtil`, etc.).
- None of the 12 test cases ran. Thus the CI failure is not evidence of a P1 bug.
- Run: https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/43424659436

## Corrective changes

1. Link test only against `ROOTNTuple`, `RIO`, `Core`, and supply the built library path explicitly, instead of using full `root-config --libs`. These are the direct libraries needed by the test; remaining library dependencies are linked into the built ROOTNTuple target. **The final binary has not yet been built with this correction.**
2. Use an explicit `ROOT::Internal::RNTupleMergeOptions` whose `fCompressionSettings` equals `c.outComp` when invoking `RNTupleMerger::Merge`. In current master the merger's default option means fast merge regardless of the destination sink's compression setting. Without this change, the P1 matrix could silently fail to exercise L2.
3. Use C++20 deliberately after `root-config --cflags`, to avoid standard-version overrides.
4. Turn off automatic `push` workflow trigger; dispatch manually only. This also stops duplicate runs on adjacent commits.
5. Add build cache restore and save immediately after successful ROOTNTuple build, **before** the test compilation step; if a test fails later, successful build can be reused. Cache upload is nonblocking if the cache service fails. The cache is not yet populated by the earlier failed runs.
6. Preserve the pinned upstream source SHA and isolated per-case 25-second process limits.
7. Run `actionlint` against the committed workflow; it passed with zero findings.

## Cost / validation gates

- Earlier successful ROOTNTuple compile consumed ~72 minutes (GitHub-hosted Ubuntu runner). New cache begins empty and an initial master build is still expensive.
- Before the next costly run, verify intended semantics of the test's output compression and individual page checksum metadata. `fCompressionSettings` addresses the former; detailed page metadata assertions remain a future enhancement.
- No new ROOT master test was executed in this audit. **Actual P1 outcome remains unknown.**
- No upstream modifications, PRs, or issues.

Links:
- CI workflow: .github/workflows/p1-master-source.yml
- Matrix driver: validation/recheck_checksum_invariants.cxx
