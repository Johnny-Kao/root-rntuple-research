# ROOT RNTuple：Merge / Page Checksum Invariant 研究紀錄

**整理日期：** 2026-11-05（JST）  
**目前結論：** ROOT 6.40.00：12 組獨立測試，6 PASS、6 異常。這些異常不等於六個獨立漏洞。  
**動態測試狀態：** 已重現 Release Binary 下的 Abort / Segmentation fault；**尚未以 ASan 確認精確的無效讀寫邊界**；尚未在當前 ROOT \`master\` 動態驗證。  
**目標：** 釐清正常生成、格式有效的 RNTuple 在合併、重新壓縮與 Checksum 轉換時是否滿足資料正確性與記憶體安全 invariant。

## 1. 可追溯證據

- [GitHub Actions #41866296529 — 完整 12 組](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41866296529) — ROOT 6.40.00、Ubuntu 24.04、Python 3.11 PyPI ROOT binary、native C++ Merger。Job exit code 1 是**研究矩陣刻意對非零案例計數**，不是編譯失敗。
- [初次測試 #41863791886](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41863791886) — 2 PASS 後 Case 2 Abort；舊 CI Shell 在非零 status 時提早停止，**不是第二次 ROOT 原因不同的失敗**。
- [測試來源 \`validation/recheck_checksum_invariants.cxx\`](../validation/recheck_checksum_invariants.cxx) — 各個 case 由獨立 Process 建立來源及合併檔案。
- [Workflow \`.github/workflows/merge-correctness.yml\`](../.github/workflows/merge-correctness.yml) — CI 修正為 12 案全部執行完後才計數。
- **檔案格式控制：** 測試使用公開的 \`RNTupleWriter::Recreate\` 產生兩個正常 ROOT 檔案；各 130 筆，欄位 \`id: int32\`、\`value: double\`，每筆有確定性預期值。合併前先完整讀回來源；合併後檢查 260 筆值，不是對畸形檔案進行 fuzzing。
- **API 限制：** 合併使用 \`ROOT::Experimental::Internal::RNTupleMerger\` + \`RPageSinkFile\`，尚未證實與一般 \`hadd\` / \`TFileMerger\` 的預設選項完全等價。影響範圍須另外確認。

**版本範圍：** 6.40.00 的崩潰已由 CI 證實；下方當前上游 \`master\` 只做程式碼靜態比對，觀察基準 commit [916c66b](https://github.com/root-project/root/commit/916c66b5933cfab7112e96c7839c3be1a4bfb095)，**不能當作 \`master\` 動態重現**。

## 2. 完整測試矩陣

表示法：\`ON/0\` = Page Checksum 開、Compression 設定值 0（不壓縮）；\`OFF/505\` = Page Checksum 關、Compression 設定值 505。表中「目的」是目的端 WriteOptions，不保證輸出每個 Fast-copied Page 都轉換該選項。除另註外 Same-page merging 關閉。

| Case | 來源 A | 來源 B | 目的 | Same-page | 結果 | Run 中觀察 |
|---|---|---|---|---|---|---|
| 0 | ON/0 | ON/505 | ON/505 | OFF | **PASS** | 260/260 值相同 |
| 1 | OFF/0 | OFF/505 | OFF/505 | OFF | **PASS** | 260/260 值相同 |
| 2 | OFF/0 | ON/505 | ON/505 | OFF | **FAIL 134** | \`dataLen > nbytes\`，在合併產物讀取路徑 |
| 3 | ON/505 | OFF/0 | ON/505 | OFF | **FAIL 134** | \`free(): invalid next size (fast)\` |
| 4 | ON/0 | ON/505 | OFF/505 | OFF | **FAIL 139** | \`Segmentation violation\`，stack：\`RSealedPage::ChecksumIfEnabled()\` |
| 5 | ON/505 | OFF/0 | OFF/0 | OFF | **FAIL 139** | \`Segmentation violation\`，stack：\`RSealedPage::ChecksumIfEnabled()\` |
| 6 | OFF/0 | ON/505 | OFF/505 | OFF | **PASS** | 260/260 值相同 |
| 7 | OFF/0 | OFF/505 | ON/505 | OFF | **FAIL 134** | \`free(): invalid next size (fast)\` |
| 8 | OFF/0 | OFF/505 | ON/505 | ON | **FAIL 134** | \`sealedPageIt->GetHasChecksum()\` Assertion |
| 9 | ON/0 | ON/505 | ON/505 | ON | **PASS** | 260/260 值相同 |
| 10 | ON/505 | ON/505 | OFF/505 | OFF | **PASS** | Fast-copy 路徑，260/260 值相同 |
| 11 | OFF/505 | OFF/505 | ON/505 | OFF | **PASS** | Fast-copy 路徑，260/260 值相同 |

**已觀察：** 12/12 process 有 exit status；6/12 沒有異常且完整驗值；另外 6/12 因 Abort 或 SIGSEGV 未完成。沒有觀察到「程式成功退出，但 260 筆中有數值錯誤」的案例。Fail 案例不代表已證實輸出錯誤值：多數尚未進入完整的輸出讀回階段。

## 3. H1：重壓縮的 Page Checksum 三個來源不一致（主要候選）

原始碼：[\`v6-40-00: RNTupleMerger.cxx\`](https://github.com/root-project/root/blob/v6-40-00/tree/ntuple/src/RNTupleMerger.cxx) 的 \`MergeCommonColumns\` 和 \`RChangeCompressionFunc\`；[目前 master 對應實作](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RNTupleMerger.cxx)。

設 \`S = pageInfo.HasChecksum()\`（來源 Page）、\`D = fDestination.GetWriteOptions().GetEnablePageChecksums()\`（目的選項）、\`U = 源欄位未壓縮長度\`、\`Z = 重新壓縮後位元組數\`。有三個關鍵操作：

1. **緩衝區配置**（\`MergeCommonColumns\`）：\`bufSize = U + 8*S\`。在 \`v6-40-00\` 約 915–920 行；\`master\` 約 1439–1454 行。
2. **新 SealedPage 標示大小**（\`RChangeCompressionFunc\`）：\`fBufferSize = Z + 8*D\`。
3. **新 SealedPage 的 HasChecksum 標記**（同函式）：\`fHasChecksum = S\`（直接沿用來源標記）。

且 [\`RSealedPage::GetDataSize()\`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/inc/ROOT/RPageStorage.hxx) 使用 \`fBufferSize - 8*fHasChecksum\`；[\`ChecksumIfEnabled()\`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RPageStorage.cxx) 根據 \`fHasChecksum\` 決定是否寫入尾端 checksum。

因此 **S != D 時，配置尺寸、記錄尺寸與語義標記不能同時保證一致**：

- \`S=0,D=1\`：配置未按目的 Checksum 留出額外 8 bytes；新物件卻宣稱多 8 bytes，\`HasChecksum\` 仍是 false。新 Page 的 \`GetDataSize()\` 會把額外的 8 bytes 當作有效資料。與 Case 2 的讀取長度 Assertion 和 Case 3/7 的 allocator 異常**相容**，但每個觸發點尚需 sanitizer 精確歸因。
- \`S=1,D=0\`：新 Page 的長度不再預留 checksum，但 \`HasChecksum\` 仍 true；計算 \`GetDataSize()\` 時會從 \`Z\` 減 8，Checksum 寫入路徑仍執行。可造成壓縮內容和尾端 checksum 互相干擾，並可能在短 Page／異常長度下產生無效存取；Case 4/5 的 stack 確實位於 \`ChecksumIfEnabled()\`，但**未經 ASan 不能定性為 Heap Buffer Overflow**。

**注意：** Case 6、10、11 仍 PASS，表示錯誤依賴 compression 路徑、source/dest Checksum 組合與頁面內容，並非只要 \`S != D\` 就必然崩潰。特別是 Fast-copy 不重新配置或重新標示 checksum；通過的 260 筆數值只代表資料正確，**不能據此推定輸出所有 Page 都遵守目的 Checksum 選項**。

### H1 對應日誌

- Case 2：\`Fatal: dataLen > nbytes violated at line 112 ... RNTupleZip.hxx\`；stack 含 \`RPageSource::UnsealPage\`、\`RPageSourceFile::LoadPageImpl\`，顯示合併產物**讀取**發生長度不一致。
- Case 3、7：\`free(): invalid next size (fast)\`（glibc heap allocator 一致性檢查失敗），非正式 ASan 报告；**高度值得單獨驗證**。
- Case 4、5：SIGSEGV，stack 首幀 \`RPageStorage::RSealedPage::ChecksumIfEnabled\`，發生於 Merge 的 Page 操作階段。

## 4. H2：Same-page merging 的來源 Page Checksum 假設

在 [\`RPagePersistentSink::CommitSealedPageV\`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RPageStorage.cxx) 中：

- 是否進入 deduplication 路徑由**目的端** \`GetEnableSamePageMerging()\` 決定。
- 進入後直接 \`R__ASSERT(sealedPageIt->GetHasChecksum())\`。
- \`RNTupleWriteOptions\` 檢查的是目的端同時開啟 Checksum 與 Same-page merging，並**不會將合法、沒有 Checksum 的來源 Page 自動轉換**。

因此 Case 8 用一般 Writer 建立 OFF checksum 的來源，合併至目的端 Same-page merging ON 時，立刻命中 Assertion。Case 9 同樣開啟 deduplication，但來源 Page 有 Checksum，所以 PASS。

這是獨立的 **source-page precondition**，可和 H1 的 recompression 錯誤區分（H1 個案皆關閉 Same-page merging）。但需要確認產品合併 API 的保證：應拒絕不相容來源、Fallback 跳過 deduplication，或應先重新生成 Checksum；**尚未選定修法**。

## 5. 原因與影響的證據等級

| 判斷 | 證據 | 狀態 |
|---|---|---|
| 正常 Writer 輸出、不同設定的來源檔案能單獨讀回 | 測試程式在 Merge 前逐筆比對 | **確認** |
| ROOT 6.40.00 六個組合異常終止 | 完整 CI 日誌 | **確認** |
| Case 4/5 崩潰位於 \`ChecksumIfEnabled()\` | 符號化 stack | **確認** |
| Case 3/7 顯示 glibc heap metadata 異常 | \`free(): invalid next size\` | **確認觀察，破壞位置未知** |
| 程式碼中配置長度／輸出長度／Checksum 標記來源不一致 | 6.40.00 + master 靜態對照 | **確認** |
| 同樣的六個例子在當前 master 動態重現 | 尚無 master build-and-run | **未驗證** |
| 精確 Heap OOB read/write、利用可行性 | 尚無 instrumented library ASan/UBSan 證據 | **未驗證** |
| Silent Data Corruption | 成功案例逐筆相同；失敗案例多為崩潰 | **尚未觀察** |
| 一般 \`hadd\`／真實 pipeline 受到同樣影響 | 目前是 Internal Merger API | **未驗證** |

**不要把所有六個失敗當成六個 CVE；不要把 \`free(): invalid next size\` 或 SIGSEGV 自動等同於已證實可利用的記憶體漏洞。**

## 6. 下一步的最小驗證 Gate（不在此紀錄中宣稱已執行）

1. **先用 ROOT 目前 master 建置並重跑**：至少 Case 0、1 作基準 + Case 2、3、4、5、7、8；若被修復，先找實際修正 commit。
2. **使用真正編譯了 ASan/UBSan 的 ROOT library**：僅將外部小型測試程式加 \`-fsanitize\` 而連接未 instrument 的預編譯 ROOT library，不能可靠定位 ROOT 內部的越界位置。若資源有限，先針對 Case 3、4、5、7 執行，並設定超時／產物大小上限。
3. **兩組正反對照**：固定格式與資料，僅切換 \`S\`、\`D\`；另外固定 checksum 僅切換是否重壓縮，釐清 H1；H2 單獨比較 \`SamePage=0/1\`。
4. **驗證實際產品路徑**：用 \`hadd\` 或 \`TFileMerger\` 的公開用法合併同樣合法來源，確認設定可達性、錯誤呈現方式與一般使用者影響。
5. **檢查上游既有 issue/PR**：未在目前關鍵字搜尋發現明確同題項，但此非完整去重證明；對照後再考慮精簡、負責任的 maintainer 回報。
6. **完成資料正確性分類**：每個成功 case 再讀回 Page metadata（Checksum/Compression）及值；若僅存在輸出 metadata 不符約定，與 Crash / Heap 不安全分開處理。

**Repo 政策：** 研究用非 Fork Repo；不直接修改 ROOT upstream；不在使用者 Mac 進行測試；GitHub Actions 只在可明確驗證的範圍執行。

## 7. 與已修復舊漏洞的關係

[ROOT #23940](https://github.com/root-project/root/pull/23940) 是 **Anchor decompression / mini-file reader 長度 invariant**（已修復並給予通報 credit）。本次是 **合併與 Page 重新壓縮**，進入不同的程式碼／資料流，不應算作 #23940 的再次發現，也不能因為兩者都與長度有關就斷言是同一漏洞。

**摘要：** 目前有一組強的 **H1 Checksum/recompression invariant** 問題與一組 **H2 Same-page precondition** 問題；ROOT 6.40.00 動態證據充分，當前 master 仍待動態確認，尚未提交 upstream issue/PR。
