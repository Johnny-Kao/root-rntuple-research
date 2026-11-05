# H1 — upstream master contract and history investigation (2026-11-05)

## Status and limitations
- Upstream master checked out at `9365bf37a4b7c5a0386d4e214f8f09da6115b8fa`, not an inferred prior snapshot.
- [GitHub Actions source gate #42052202294](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42052202294) emitted `H1_MASTER_SOURCE_CONTRACT_CONFLICT_CONFIRMED` with four expected source markers.
- Actual master binary runtime was **not executed**. An exact ROOTNTuple-target Ninja dry run contained **3,607 tasks**, breaching the deliberately enforced 1,200-task gate (`H1_MASTER_BUILD_GATE=TOO_LARGE_NO_DYNAMIC_EXECUTION`). CI failed with exit 22 by design, not due to an application-level regression.
- Last dynamic validation: v6.40.00 source, patched against binary v6.40.00: [#42032809017](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42032809017), 10/10 tests, 260 rows each, exact readback.
- Repository restored Private after run.

## Source-level root-cause evidence
- Master `tree/ntuple/src/RNTupleMerger.cxx`: `RChangeCompressionFunc` computes `checksumSize` using `fWriteOpts.GetEnablePageChecksums()`, but constructs the new `RSealedPage` with `fSealedPage.GetHasChecksum()` (the input-page flag).
- `RNTupleMergeStrategyDefault::MergeCommonColumns` allocates `uncompressedSize + checksumSize` where `checksumSize = pageInfo.HasChecksum() * RPageStorage::kNBytesPageChecksum` (input-page flag).
- Thus for recompressed pages, destination checksum choice and output-page flag/buffer capacity may disagree.
- A higher-level contract checks merger option output **compression** against the sink's compression; no equivalent check forbidding mixed input/output checksum settings was found.
- The page storage format represents checksum presence per page. Fast-copy merger can preserve source-page flags even when output write options differ; that is distinct from the inconsistent newly generated page in recompression.
- `RNTupleWriteOptions.hxx` documents `EnablePageChecksums=true` as 'checksums will be calculated and written for every page', which deserves nuanced interpretation for fast-copy merging.
- `hadd`/TFileMerger RNTuple merge reaches this code path via `RNTupleMerger`, so this is not solely a test helper, but dynamic product-path impact on master remains unproven.

## History
- [38792a6](https://github.com/root-project/root/commit/38792a6293a6ff31355f8b8679780773ecb9328c), committed 2025-12-04 (author timestamp 2025-11-10): introduced L2 recompression that used destination checksum for required bytes, retained source checksum flag in page, and allocated by source checksum. This is the earliest identified source-level origin of today's L2 inconsistency, not proof of absence on older branches.
- [356df0f](https://github.com/root-project/root/commit/356df0f28f919238c71ece0469c0986c0ad333ac), committed 2025-12-04 (author timestamp 2025-11-10): changed Reseal to honor destination checksum, but left buffer size assertion using source checksum.
- Refactors in 2026 removed the Reseal function in the default master path; the modern fix scope may differ from v6.40.00.

## Existing-test coverage caveat
- Master `tree/ntuple/test/ntuple_merger.cxx`, tests `ChangeCompression` and `ChangeCompressionMixed`: `destinationNoChecksum` is constructed when `writeOpts.SetEnablePageChecksums(true)` is in force; the subsequent `writeOpts.SetEnablePageChecksums(false)` happens after destination creation. This test name is misleading and does not exercise the off-destination variation as intended.
- `tree/ntuple/test/ntuple_checksum.cxx` Merge test is about checksum *corruption detection*, not differing valid checksum modes.

## Decision
Credible format/size correctness bug in generated recompressed output on master, supported by specific code and historic v6.40 dynamic repro. Do not state that latest master runtime is confirmed failing until a coherent master build and exact run exist. If upstream engagement becomes appropriate, use a narrowly scoped issue or PR with a direct user-facing reproducer and source-level contracts, not a bulk refactor.
