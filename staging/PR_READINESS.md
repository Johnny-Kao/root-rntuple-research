# P1 RNTuple L2 checksum consistency — submission readiness

Status: **Ready to open a Draft PR for upstream review**, without submitting one yet.

## Proposed change

- Upstream file: `tree/ntuple/src/RNTupleMerger.cxx` (minimal 6-line diff: remove one unused option reference and derive recompressed-page checksum bytes from the page's own checksum flag).
- Regression test: `tree/ntuple/test/ntuple_merger.cxx` (+41 lines, new `MergeThroughTFileMergerRecompressMixedChecksums` GoogleTest).
- Combined patch: `staging/P1_PR_READY.patch`.
- Proposed title and description: `staging/PR_BODY.md`.

The L2 recompression code previously allocated based on source `HasChecksum`, set its resulting buffer length from destination options, and retained source `HasChecksum`. The patch keeps these representations consistent with L1's source-preserving fast copy. No change is made to ROOT's generic checksum handling.

## Verified behavior

Reproducer binary ROOT **6.43.01** at source SHA `5281bc2342c5cba711cb507f4ef53c623ba4f942`. All runs are on GitHub-hosted Ubuntu and use the same source inputs before/after.

- Baseline: isolated matrix 6/12 PASS; cases 2,3,7,8 abort with exit 134; cases 4,5 SIGSEGV exit 139.
- Patched: matrix **12/12 PASS**, 260/260 expected values verified for each case.
- Public `TFileMerger`: baseline aborts with exit 134; patched returns **PASS in both source orders** (2 × 260 values verified).
- Native ROOT `ntuple_merger.cxx` new GoogleTest: passed **C++17 syntax/API compilation** with upstream test support headers.
- Both code and test patches apply cleanly to the pinned source, with `git diff --check` success.
- Evidence: https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/43455000906

Latest upstream head inspected: `6a167d36491cc92be9da91387312b0eed44b7095` (24 commits after tested SHA). The combined PR patch was applied to that HEAD and passed `git diff --check` in a dedicated check. The intervening RNTupleMerger changes involve cluster-group finalization, not this L2 checksum block.

- Latest-head patch applicability: https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/43455968332

## Duplicate and design review

- No exact existing upstream issue/PR found in searched discussions for this L2 source/destination checksum mismatch. Nearby work: merged #20112 introduces L2 behavior; #24085 guards same-page merging when pages have no checksums; neither fixes this independent L2 mismatch.
- Maintainer discussion on #20112 considers destination write options authoritative; that is a design question, not a confirmed contract for converted existing pages. The staged minimal fix preserves existing page checksum flags to match current L1 behavior. The upstream reviewer may choose a broader destination-normalization design instead; that would need consistent L1/L2 treatment and additional tests.

## Limitations — do not overclaim

- The newly-added upstream GoogleTest is syntax-compiled, **not executed as part of the complete ROOT native test suite**. Its equivalent end-to-end `TFileMerger` scenario was executed in the standalone driver.
- No ASan/UBSan-instrumented ROOT build was performed; exact first invalid memory write and security severity remain unproven.
- The regression matrix was not recompiled/re-executed against the latest `6a167d` source; **only patch applicability** was verified there. Upstream CI should confirm it at submit time.
- Passing these tests does not establish destination checksum normalization semantics for all existing-page modes; that is an explicit maintainer review question.

## Execution boundary

All files and testing changes remain in `Johnny-Kao/root-rntuple-research`. No upstream fork, issue, PR, comment, or push was made.
