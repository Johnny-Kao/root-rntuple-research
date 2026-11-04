# H1 patched runtime evidence — 2026-11-05

## Goal
Test if the minimum two-line ownership changes to `RNTupleMerger.cxx` eliminate checksum/recompression crashes without rebuilding LLVM/Clang.

## Baseline
Prebuilt ROOT 6.40.00, GitHub Actions [#41923898857](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41923898857).
10 H1 cases: 0,1,6,10,11 pass with exact 260-record readback; 2,3,4,5,7 abort/segfault. No successful silent numeric mismatch observed.

## Patch
- `patches/h1-root-v6-40-00.patch` targets v6-40-00; `patches/h1-output-checksum-consistency.patch` targets pinned master 916c66b.
- Recompressed `RSealedPage` checksum flag derives from destination write options.
- Recompression output allocation includes destination page checksum size, not source checksum size.
- Source checksum verification stays enabled.

## Checks
- Source patch applies to pinned master: PASS [#41925030051](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41925030051).
- Single patched v6-40-00 `RNTupleMerger.cxx` compiled/linked with matching prebuilt ROOT wheel: compilation PASS [#41927428490](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41927428490).
- Patched runtime matrix: cases 0,1,6,10,11 PASS, cases 2,3,4,5,7 FAIL; **NOT a regression pass**.
- Case 2 stack references the patched executable, so patched source is executing. In the one-case size diagnostic [#41928202249](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41928202249): `H1TRACE func packed=4 buf=4 ck=8 src=0 dst=1 elems=1`, followed by `Assertion fBufSize >= bytesPacked + checksumSize`.
- This means the recompression functor still receives four bytes when it expects at least twelve in this case. The discrepancy is **not explained**. Possibilities to investigate: the allocation code actually used at runtime vs patched code, binary/source compatibility, an additional allocation path, or state lifetime. None established as the cause.
- Diagnostic workflow scripting encountered YAML formatting and string-match failures [#41927972781](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41927972781), [#41928097963](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41928097963). These are infrastructure failures, not ROOT failures.

## Stop / next gate
Do not claim fix. Do not open upstream PR. Do not call a 6.40.00 reproduction proof of master runtime behavior. Before more paid CI, perform code-path review and design an isolated check of destination buffer size and exact call to `RChangeCompressionFunc`. If that check cannot establish faithful patched binary behavior, abandon this partial-object override route in favor of a version-matched full-library build with dependencies prebuilt/cached. Confirm public `hadd`/TFileMerger reachability before PR.

Research repo visibility restored to Private and verified after runs.
