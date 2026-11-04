# ROOT RNTuple invariant recheck — 2026-11-05 JST

**Status:** Source-audited hypotheses. **No new independent defect dynamically confirmed.** No upstream Issue/PR/contact; private research branch only.

## Scope and boundaries
- Upstream: `root-project/root`, both `v6-40-00` and `master` read from GitHub.
- Prior credited finding: [ROOT #23940](https://github.com/root-project/root/pull/23940), merged 2026-11-04. Excluded from any new-bug tally.
- Target: normal RNTuple data correctness when merging, especially pages with differing checksum/compression options.
- No execution on user's Mac. No controlled new test result should be asserted unless GitHub Actions actually started and emitted logs.

## H1 — Page checksum invariant in recompression (highest priority; not dynamically confirmed)
**Source:** `tree/ntuple/src/RNTupleMerger.cxx`, `RChangeCompressionFunc`, present in `v6-40-00` and current `master`.

- Recompression constructs a buffer size using **destination** `GetEnablePageChecksums()`:
  `checksumSize = dstWriteOptions.GetEnablePageChecksums() * 8`.
- It constructs the new `RSealedPage` using **source** `fSealedPage.GetHasChecksum()`, not destination flag.
- `RSealedPage::GetDataSize()` subtracts `8` according to `fHasChecksum`; `ChecksumIfEnabled()` writes according to that flag.
- When source and destination checksum settings differ, the physical buffer length and the semantic checksum flag can disagree.
- Need verify actual reachable cases, output deserialization, whether an error is explicit or numerical results change, and upstream-version compatibility. Avoid claims of leak, overflow, RCE, or silent data loss before dynamic proof.
- This is a distinct invariant from #23940's anchor compression lengths.

## H2 — Same-page merge requires checksummed input pages (prior assertion reproduced)
**Source:** `tree/ntuple/src/RPageStorage.cxx`, `RPagePersistentSink::CommitSealedPageV()`.

- Destination `RNTupleWriteOptions` has page checksums and same-page merging enabled by default.
- `CommitSealedPageV` enters the same-page path based on **destination options**, then asserts every **source sealed page** has checksum.
- Legitimate source RNTuples may have been written with `SetEnablePageChecksums(false)`.
- Earlier isolated ROOT 6.40.00 CI [#41655581430](https://github.com/Johnny-Kao/root-rntuple-anchor-invariant-lab/actions/runs/41655581430) reported:
  - checksum OFF→ON and OFF→OFF: `sealedPageIt->GetHasChecksum()` fatal assertion.
  - ON→OFF and ON→ON: 260/260 values correct.
- This is a **confirmed assertion under a tested configuration**, but expected merge contracts and any independent novelty are not yet established. No silent corruption demonstrated.

## H3 — Optional type checksum treated as compatible (lower confidence)
**Source:** `tree/ntuple/src/RNTupleMerger.cxx::CompareDescriptorStructure()` and `tree/ntuple/doc/Merging.md`.
- Type checksum mismatches are rejected **only if both checksums exist**. Documentation explicitly asks `is this sound?`.
- Other checks (type names, version, structures, column formats) still apply; therefore checksum absence alone is not proof of incompatible merge or numeric corruption.
- Need independently generated valid source files with equal type names/versions but differing dictionary semantics, controlling for known [#23683](https://github.com/root-project/root/issues/23683), [#22130](https://github.com/root-project/root/issues/22130), and other existing upstream fixes.

## Candidate downgraded / excluded
- `RMiniFile.cxx` TKey `GetSize() - fKeyLen` underflow: current #23940 `compSize > uncompLen` rejection catches the usual wrapped-size anchor case; not independently established.
- Anchor checksum offset minimum: `GetNTupleProper()` checks `fObjLen >= 78` before computing the anchor checksum. No new independent issue confirmed.
- Public [#23683](https://github.com/root-project/root/issues/23683): union/schema member mis-mapping; already claimed by another reporter. Do not duplicate.

## New test prepared, execution blocked
- Branch: `research/rntuple-invariant-recheck-20261105`
- Test: `validation/recheck_checksum_invariants.cxx` and `.github/workflows/merge-correctness.yml`.
- One planned CI matrix: 12 processes, paired native RNTuple creation and merger invocation, 260-row byte/value oracle, checksum and recompression cases, same-page merge toggles.
- [GitHub Actions #41857836362](https://github.com/Johnny-Kao/root-rntuple-anchor-invariant-lab/actions/runs/41857836362): **completed/failure without any job steps or readable logs**. Runner/test execution **not evidenced**. Do not interpret as a compilation, product, or failing-test result. Do not repeatedly retry without new infrastructure information.

## Follow-up gate
1. Confirm Runner allocation and actually execute the 12-case matrix on GitHub Actions.
2. Separate H1 from H2 by disabling same-page merging and changing only source/destination checksum options.
3. If a case fails, preserve exact source file settings, merge outcome, output read outcome and upstream version; validate against current master.
4. Only then classify independent correctness bug, security concern, or expected/unsupported API behavior.
