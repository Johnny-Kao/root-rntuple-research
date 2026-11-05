## 2026-11-06 — definitive official ROOT 6.42.00-rc1 production `hadd` validation

**Reproduced without patching ROOT.** [GitHub Actions #42065941739](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42065941739), Ubuntu 24.04. Used official CERN 6.42.00-rc1 binary `root_v6.42.00-rc1.Linux-ubuntu24.04-x86_64-gcc13.3.tar.gz`, 397,692,613 bytes, downloaded in eight byte-range parts, checked exact byte lengths and passed `gzip -t`. Both test input RNTuples were created by the matching ROOT version's public RNTupleWriter; merged using the real `hadd -f505` CLI. No ROOT implementation changes.

Results (all include 130 entries in each input):
- Checksum ON + ON, both compression 505: success, 260/260 exact readback.
- Checksum ON + ON, compression 0 + 505: success, 260/260 exact readback.
- Checksum OFF + ON, compression 505 + 505: SIGABRT exit 134.
- Checksum OFF + ON, compression 0 + 505: SIGABRT exit 134.
- Checksum OFF + OFF, compression 505 + 505: SIGABRT exit 134.

Crash signature: `Fatal: sealedPageIt->GetHasChecksum() violated at line 1606 of /github/home/ROOT-CI/src/tree/ntuple/src/RPageStorage.cxx`. Observed incomplete 413-byte merged file, no RNTuple object; reader refused to open it.

**Interpretation:** same-page dedup's checksum requirement rejects valid checksum-free source pages through a public production entry point, without needing recompression. This is separate from H1 regenerated-page checksum/size inconsistency and should not be conflated with it. Confidence of user-facing reproducible bug on 6.42 RC1: very high. It is a deterministic availability/batch integrity failure; neither silent data corruption nor CVE-grade security impact is established.

This run was **not exact master**. CI job itself is marked failed because failure reproduction returns nonzero, not because setup failed. Research repo reverted and verified **Private** after the run.

---

## 2026-11-06 retry: 6.42.00-rc1 blocked before runner allocation

- Updated `.github/workflows/h1-production-hadd-642rc1.yml` for eight parallel HTTP-range downloads of CERN's official unmodified 6.42.00-rc1 binary, with part-length and gzip integrity checks. The previous single-stream download had timed out.
- Attempted public-run validation: [Actions #42064027160](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42064027160).
- **No runner was allocated**. GitHub annotated: `The job was not started because recent account payments have failed or your spending limit needs to be increased.`
- Cross-checked independent [hadd CLI #42064027113](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42064027113) and [master contract #42064026855](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42064026855): same runner-allocation failure. Therefore this is account-level Actions enforcement, **not a code/test regression**.
- **No 6.42.00-rc1 or exact master runtime result**. Do not claim fixed or reproduced on the newer version.
- Research repo restored **Private** and independently verified.
- Once Actions availability is restored, rerun the saved official-binary workflow without modifying ROOT implementation. For exact master, source-build cost remains an unresolved 3,607 Ninja-task gate.

---

# Public hadd production-path validation — 2026-11-05

## Objective

Determine whether *unmodified* ROOT production/user-facing merge calls can fail with valid RNTuples using differing checksum settings, and separate the previously researched H1 L2 recompression issue from higher-level output sink rules. **No ROOT source modifications were used.**

## Reproduction environment

- GitHub Actions Ubuntu 24.04, official prebuilt ROOT 6.40.00 via PyPI ROOT==0.1a12.
- Actual public `hadd -f505 output.root sourceA.root sourceB.root` CLI; stack includes `hadd` -> `TFileMerger::MergeRecursive` -> `RNTuple::Merge` -> `RNTupleMerger::MergeSourceClusters` -> `RPagePersistentSink::CommitSealedPageV`.
- Legitimate source files generated with public RNTupleWriter and checked with public RNTupleReader.
- Test harness `validation/h1_hadd_cli.cxx`; workflow `.github/workflows/h1-production-hadd.yml`.
- Definitive matrix logs: [Actions #42056005937](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42056005937). Earlier [#42055741191](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42055741191) also provided clean control and stack.

## Production matrix (all commands request output compression 505)

| Input A | Input B | Observed |
|---|---|---|
| checksum ON, compression 505 | checksum ON, compression 505 | PASS; 260/260 exact readback |
| checksum ON, compression 0 | checksum ON, compression 505 | PASS; 260/260 exact readback |
| checksum OFF, compression 0 | checksum ON, compression 505 | Abort 134; output file only 413 bytes and cannot open RNTuple |
| checksum OFF, compression 505 | checksum ON, compression 505 | Abort 134 |
| checksum OFF, compression 505 | checksum OFF, compression 505 | Abort 134 |

Fatal expression is `sealedPageIt->GetHasChecksum()` in `RPageStorage.cxx:1123`. Destination has same-page deduplication enabled; it demands page checksums. Input writers may validly disable page checksums. The public merger does not negotiate/rewrite this incompatibility before committing fast-merged pages. This public CLI issue does not require a change in compression and is **distinct from, and can mask, H1 L2 recompression's data-size/checksum-flag inconsistency**.

## Contract and impact

- This is a highly credible, proven user-facing bug: valid inputs can crash `hadd` in ordinary checksum modes.
- Proven consequence: deterministic batch/merge job termination (SIGABRT, exit 134) and invalid partial output (one observed 413-byte file). In automation, this can block data-processing pipelines, and stale/incomplete outputs could be picked up by downstream automation if exit codes or file validity are ignored.
- The logs show the output was *not silently accepted*; a reader rejected the RNTuple. **Do not claim silent wrong numeric results, remote attackability, arbitrary memory overwrite, or CVE severity.**
- Publication/hardening proposal should distinguish: (A) same-page merging valid input negotiation at production `hadd` entry, (B) regenerated/recompressed page checksum correctness H1. They should probably be separate upstream issues/PRs.
- `RNTupleWriteOptions::SetEnablePageChecksums(false)` is a supported input option. Output dedup implies output checksum, but does not mean every source must have enabled it. A user-facing contract check could reject incompatible input with a controlled error; better can reconcile source Page checksum by disabling same-page dedup or generating destination-compliant pages.
- GitHub baseline v6.40.00 patched internals: [#42032809017](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42032809017): 10/10 cases exact readback, but this does **not** guarantee public `hadd` is fixed by the H1 patch.

## Master and 6.42 release caveat

- Exact upstream master commit `9365bf37a4b7c5a0386d4e214f8f09da6115b8fa` source contract inspected; static discrepancy persists. [#42052202294](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42052202294) stopped before native build at 3,607 Ninja target tasks (predefined 1,200 task cost gate). **No exact-master runtime reproduction**.
- Official 6.42.00-rc1 (2026-10-29) binary attempt [#42056518386](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/42056518386): attempted 397,692,613-byte CERN binary download; two curl 240-second timeouts (57 MB and 130 MB transferred); cancelled. **No 6.42 runtime result**.
- Production research repository returned to Private and verified.

## Assessment (evidence-based, not statistical probabilities)

- **9/10 confidence** that there is a genuine public `hadd` merge bug with valid checksum-disabled inputs in the tested 6.40.00 version.
- **8/10 confidence** that the separate H1 regenerated-page checksum inconsistency is a real bug in the tested 6.40.00 version, given controlled fixed-vs-unfixed replay. Exact-master live reproducibility remains unproven.
- Risk impact based on evidence: data-processing availability/integrity of output artifact; not silently incorrect computational values nor demonstrated exploitability.
