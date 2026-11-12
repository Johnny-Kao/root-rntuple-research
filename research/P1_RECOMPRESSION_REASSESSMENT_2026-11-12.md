# ROOT RNTuple P1 / H1 — post-#24085 re-assessment (2026-11-12)

**Scope:** research only. No upstream issue, PR, comment, or push. The source inspection below used `root-project/root` master on 2026-11-12. **No new compiled ROOT master run or ASan run was performed here.** The original 6.40.00 runtime evidence must not be presented as proof of master behavior.

## Prior evidence

- Research matrix: `research/RNTUPLE_MERGE_CHECKSUM_FINDINGS_2026-11-05.md`.
- 6.40.00: Cases 3/7 `free(): invalid next size (fast)`; Cases 4/5 SIGSEGV with stack at `RSealedPage::ChecksumIfEnabled()`; Case 2 read assertion `dataLen > nbytes`. These are observed failures, not independently verified ASan heap OOB or successful silent corruption.
- Inputs are normal Writer-generated RNTuples, read back correctly before merging. The 2026-11-05 matrix uses Internal Merger API; public hadd reachability still needs runtime confirmation.

## Post-#24085 source inspection

ROOT PR #24085 (merged 2026-11-12) adds a guard in `RNTupleMergeStrategyDefault::MergeCommonColumns` for a **different** precondition: if a source page lacks checksum while same-page merging is enabled, warn and disable same-page merging. Its patch does not update the L2 recompression allocation, size, or checksum-marker logic.

Current master `tree/ntuple/src/RNTupleMerger.cxx` still has the following L2 chain:

1. `needsRecompressing = colRangeCompressionSettings != outCompression`. L1 bypasses recompression.
2. `checksumSize = pageInfo.HasChecksum() * RPageStorage::kNBytesPageChecksum`.
3. `bufSize = uncompressedSize + checksumSize`; allocate `buffer` on that **source-page** basis.
4. `RChangeCompressionFunc` instead computes `checksumSize = fWriteOpts.GetEnablePageChecksums() * sizeof(std::uint64_t)` on **destination options**.
5. Then constructs `RSealedPage` with `nBytesZipped + checksumSize` but preserves `fSealedPage.GetHasChecksum()` from the **source**; `ChecksumIfEnabled()` follows that source flag.

If source checksum S differs from destination option D, the allocated capacity, declared page length, and checksum-creation flag are based on mixed semantics.

- **S=0,D=1:** buffer capacity excludes 8 bytes; declared length includes 8, but the page flag says no checksum and no checksum is generated. Data length may be overreported; access beyond initialized/allocated storage is plausible depending on compressed size and consumers. Exact OOB remains unverified.
- **S=1,D=0:** buffer capacity includes 8, declared length omits 8, but the flag still says checksum is present and checksum generation remains enabled. The trailing 8 bytes can overlap the compressed payload, and tiny lengths could underflow in downstream size calculations. Exact failure mechanism requires instrumented tests.
- **S=D:** this particular mismatch disappears; another bug could still exist.
- **L1 fast copy:** source page bytes/flag normally pass through unchanged. Thus a destination-wide policy to force all pages to D is **not** implemented consistently even before a fix.

## Repair design alternatives — not implemented

**Candidate A (minimal, source-preserving L2):** consistently preserve each source page's checksum status during L2. Use the source `GetHasChecksum()` for the new size and checksum generation; allocation and marker already follow the source. Remove unused destination checksum dependency from the recompression helper after checking all call sites. This aligns L1 and L2's existing source-preservation model; it may not honor an assumed destination checksum conversion contract. Verify with official Merging.md and PR #20112 review discussion before choosing.

**Candidate B (destination-normalizing L2):** explicitly convert each recompressed page to D: allocate enough bytes for D, set size and `HasChecksum` to D, and write/remove checksum accordingly. More extensive; L1 would need its own policy or an explicit exception so output behavior is not inconsistent.

**Candidate C (reject incompatible source/destination):** guard at RNTupleMerger before unsafe recompression, return a diagnostic instead of crashing. This avoids memory risk but sacrifices valid mixed-setting merges and is likely not preferred without strong constraints.

Do not simply change `HasChecksum` to D or enlarge the buffer alone. All three invariants and the L1/L2 policy must be considered together.

## Verification gate (pending)

1. **Current master**: exact HEAD SHA, build instructions, ROOT version and checksum-mixed cases 0/1/2/3/4/5/6/7/10/11, separate processes, bounds on runtime and output artifacts. Case 8 should pass after #24085 independently.
2. **Public product path**: normal Writer-generated sources, `hadd` and/or `TFileMerger`; test same/different compression, both checksum directions, and both source orders. Confirm actual L1 vs L2 by debug/log/instrumentation, not inferred from destination options.
3. **Instrumented ROOT libraries**: ASan/UBSan on ROOT itself, not just test harness; prefer Cases 3/4/5/7. Save first invalid stack and distinguish buffer OOB, overwrite within buffer, or metadata inconsistency.
4. **Regression and semantics**: expected values and row order, page checksum flags, checksum integrity, compression metadata, repeated runs, fast-copy controls. Check tests setting destination checksum *before* constructing sink; earlier tests may set it after sink options have been cloned.
5. **Candidate A local fail→pass on staging only** if verified: minimal source-preserving change and focused regression. Compare Candidate B only if contract requires destination normalization.
6. If current master public path exposes memory corruption with potentially untrusted input, assess the project's private security disclosure route before any upstream disclosure. No upstream action from this research branch.

## Assessment

- **Confirmed:** the source/destination mismatch still exists in master source inspected 2026-11-12; #24085 does not fix H1.
- **Confirmed historically:** failures under 6.40.00 release binary.
- **Not confirmed:** latest-master reproducer, precise OOB/security impact, silent corruption, normal hadd reachability, and correct long-term checksum policy.
- **Decision:** P1 merits continued validation, not yet an upstream PR or a claim of fixed code. Research-only HOLD.
