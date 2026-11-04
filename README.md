# ROOT RNTuple Research

Independent compatibility and correctness checks for ROOT RNTuple readers, writers, and file merging.

## Research status

- **Reader anchor validation:** The previously reported length-invariant problem has been fixed upstream in [ROOT #23940](https://github.com/root-project/root/pull/23940), merged on 2026-11-04. The historical reproducer in `tests/` remains for regression research.
- **Merge compatibility:** An independent test matrix on [`research/rntuple-invariant-recheck-20261105`](../../tree/research/rntuple-invariant-recheck-20261105) checks input/output values under different checksum and compression settings. A workflow status alone is not evidence that a test executed.
- **Other hypotheses:** Not established as independent ROOT defects until reproducible and compared against upstream fixes and known issues.

Tests run in GitHub Actions and generate their own data. No local Mac testing is required.

### Validation workflows

- [Reader regression workflow](.github/workflows/anchor-asan.yml)
- [Merge compatibility test branch](../../tree/research/rntuple-invariant-recheck-20261105)

The repository is independent research material, not an official ROOT project.
