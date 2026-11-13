# ROOT P1 — current-master runtime verification (2026-11-13)

## Execution / provenance

- ROOT upstream source SHA: `5281bc2342c5cba711cb507f4ef53c623ba4f942`
- Reported ROOT version: `6.43.01`
- Research-only, non-fork repo and branch; no upstream changes
- GitHub Actions run: https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/43448524898
- This is an actual **compiled-current-master**, linked, executable 12-case run (not just static source inspection).
- ROOTNTuple build cache `rootntuple-5281bc2342c5-release-minimal-nocurl-v1` restored successfully; ROOT build step skipped.
- The test driver was compiled with C++17 to match ROOT's C++17 `std::__ROOT::span` ABI. The exported symbol `ROOT::Internal::RNTupleMerger::Merge(std::__ROOT::span<ROOT::Internal::RPageSource*>, ROOT::Internal::RNTupleMergeOptions const&)` was verified in `libROOTNTuple.so`.
- `RNTupleMergeOptions::fCompressionSettings` was explicitly set per-case to exercise L2 recompression when source compression differs. The API used is `ROOT::Internal::RNTupleMerger`, not the obsolete experimental namespace.
- Each case independently creates two valid Writer output files, verifies each source, calls the merger, then attempts to verify all 260 output rows.

## Results

| Case | Source compression/checksum and destination | Exit | Observation |
|---|---|---:|---|
| 0 | ON/0 + ON/505 -> ON/505 | 0 | PASS 260/260 |
| 1 | OFF/0 + OFF/505 -> OFF/505 | 0 | PASS 260/260 |
| 2 | OFF/0 + ON/505 -> ON/505 | 134 | `dataLen > nbytes` assertion |
| 3 | ON/505 + OFF/0 -> ON/505 | 134 | `dataLen > nbytes` assertion |
| 4 | ON/0 + ON/505 -> OFF/505 | 139 | Segmentation violation |
| 5 | ON/505 + OFF/0 -> OFF/0 | 139 | Segmentation violation |
| 6 | OFF/0 + ON/505 -> OFF/505 | 0 | PASS 260/260 |
| 7 | OFF/0 + OFF/505 -> ON/505 | 134 | `dataLen > nbytes` assertion |
| 8 | OFF/0 + OFF/505 -> ON/505, same-page enabled | 134 | Maintainer's guard correctly warns and disables same-page merging; then `dataLen > nbytes` assertion (H1) |
| 9 | ON/0 + ON/505 -> ON/505, same-page enabled | 0 | PASS 260/260 |
| 10 | ON/505 + ON/505 -> OFF/505 | 0 | PASS 260/260, L1 |
| 11 | OFF/505 + OFF/505 -> ON/505 | 0 | PASS 260/260, L1 |

Overall: **6 PASS / 6 FAIL**. Cases 2/3/4/5/7 reproduce H1 on current master. Case 8 is not evidence of H2 regression: the H2 guard executed and its warning is present, but H1 still causes a downstream length assertion.

## Scope and caution

- Confirmed: same valid source inputs can trigger aborts or SIGSEGV in the compiled current-master internal merger with mixed checksum/compression settings.
- The run is not an AddressSanitizer-instrumented ROOT library build. Exact OOB/first-corruption site and exploitability remain unproven.
- ROOT's stacktrace helper script was absent from the scoped build, so Case 4/5's exact frames were not captured in this run; older 6.40.00 traces pointed to `ChecksumIfEnabled()`, which cannot automatically be asserted as the current master faulting frame.
- No silent wrong values were observed in cases returning success; failed cases did not reach complete output verification.
- Direct `hadd` public CLI reachability is not yet verified by this run.
- This is a *research reproducer*, not a submitted fix. No upstream actions.

## Next gated research

1. On this same pinned source, verify public `TFileMerger`/`hadd` reproduces at least one of the H1 cases.
2. Instrument ROOTNTuple with ASan and obtain first invalid memory access for Case 4 or 5, preserving exact inputs and log.
3. Prototype minimal fix in research only; compare source-preserving vs destination-normalizing checksum semantics in light of #20112 discussion; rerun 12-case matrix and inspect page metadata.
