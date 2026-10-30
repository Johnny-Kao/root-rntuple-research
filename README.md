# ROOT RNTuple anchor length invariant — private validation

Status: **Pending CI runner allocation**. No memory-safety finding is confirmed.

- Scope: `RMiniFileReader::GetNTupleProperAtOffset` in `tree/ntuple/src/RMiniFile.cxx`.
- Static invariant: compressed input length must not exceed the allocated input buffer capacity. ROOT's writer does ensure compressed data are not larger than the original, but the reader does not revalidate this at the read boundary.
- Reproducer: generate a legitimate RNTuple and change one on-disk TKey length in a bounded copy (without publishing the altered file).
- Controls: normal file must read successfully; malformed file is opened via normal `RNTupleReader::Open`; instrument reader with ASan; require an exact heap-buffer-overflow report before calling it confirmed.
- Isolation: GitHub-hosted Actions only. Do not test on user's Mac. No upstream issues, PRs, or public artifacts.
- Limitations: prebuilt ROOT PyPI wheel is a released snapshot, not the current `master`; if it reproduces, verify the relevant source is unchanged on `master`.
- Gate: upon a confirmed memory-safety issue, preserve evidence privately and follow ROOT security policy before publication.

Source reading baseline: `root-project/root@56eee6877aefb25a2749bea8436833e8692796ba`.

Related historical context:
- [ROOT PR #18612](https://github.com/root-project/root/pull/18612) — anchor buffer refactor.
- [ROOT PR #19434](https://github.com/root-project/root/pull/19434) — separate on-disk and uncompressed length arguments.
- [ROOT PR #20986](https://github.com/root-project/root/pull/20986) — corrupted file read error handling.
- [ROOT PR #23066](https://github.com/root-project/root/pull/23066) — input length limits in S3 backend.

Actions run: https://github.com/Johnny-Kao/root-rntuple-anchor-invariant-lab/actions/runs/41101207559
