## Summary

When the RNTuple merger recompresses a page, it allocates the output buffer based on whether the **source page** has a checksum. However, the size of the resulting sealed page is calculated from the **destination write options**, while the page keeps the source's `HasChecksum` flag.

If those settings differ, the page's declared size no longer matches its checksum flag and allocated storage. In my tests this caused an assertion failure or a segmentation fault when merging otherwise readable RNTuples.

This patch uses the source page's checksum flag consistently for L2 recompression, matching the existing L1 fast-merge behavior, and removes the unused write-options field from the helper.

## Reproducer and tests

I added a `TFileMerger` regression test using sources with different compression and page-checksum settings, checked in both source orders.

On ROOT 6.43.01 (source `5281bc2342c5`), I also compared the same 12-case matrix before and after the patch:

- Original: 6/12 pass; the remaining cases abort or crash.
- Patched: 12/12 pass.
- Public `TFileMerger` path: original aborts; patched succeeds in both source orders, with all 260 values verified per run.

Research run: https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/43455000906

The new native test compiles against ROOT's current test headers. Full upstream CI and instrumented sanitizer builds are not part of this research run.
