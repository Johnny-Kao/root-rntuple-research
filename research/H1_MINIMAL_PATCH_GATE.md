# H1 minimal fix — checksum ownership during recompression

Target: `root-project/root` commit `916c66b5933cfab7112e96c7839c3be1a4bfb095`.

## Defect
In `RNTupleMerger.cxx`, the recompressed page's recorded size is calculated from the destination checksum option, but `RSealedPage::GetHasChecksum()` remains the source value. The output allocation also uses the source checksum setting. For mixed source/destination checksum settings, the three values disagree.

## Proposed fix (only during recompression)
1. Preserve source-page checksum verification before decompressing.
2. Allocate the destination buffer using destination checksum settings.
3. Set the recompressed sealed page's checksum flag to destination settings.
4. Leave L1 fast-copy unchanged; it preserves its source-page representation.

Patch: `patches/h1-output-checksum-consistency.patch`.

## Regression
Existing `validation/recheck_checksum_invariants.cxx` provides independent generated files and full 260-value readback; H1 cases are 0,1,2,3,4,5,6,7,10,11. In ROOT 6.40.00 baseline, cases 2,3,4,5,7 crash or abort, while controls pass. **A successful patch validation requires rerunning this exact matrix against a ROOT library built with the patch** and verifying all cases return zero, followed by supported public entry-point testing.

The fast prebuilt ROOT wheel cannot validate a source-level patch unless the patched library is rebuilt/relinked. A source patch applying cleanly is a syntax/integration preparation check, not a runtime regression PASS.

## Submission gate
- No upstream PR until patched runtime result and official merge-path reachability are confirmed.
- A checksum mismatch alone must not be called silent numeric corruption; no completed merge with incorrect values was observed.
- Security impact remains unclassified pending instrumented memory-safety results.
