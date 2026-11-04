# ROOT RNTuple merge compatibility verification

This public research branch verifies RNTuple compatibility when merging files with different page-checksum and compression settings.

- Test input data are generated in GitHub Actions.
- The test compares output rows with input rows.
- No modified ROOT upstream source, sensitive data, credentials or binaries are committed here.
- Test conclusions require a completed build and job log, not just a workflow status.

Public CI recheck requested on 2026-11-05 JST.
