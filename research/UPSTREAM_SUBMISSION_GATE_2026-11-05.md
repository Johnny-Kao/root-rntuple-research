# ROOT RNTuple — 上游提交前的五項審查（2026-11-05）

**目前決策：HOLD — 不開 Issue、不開 PR、不合併候選修正。**
六個失敗案例不代表六個獨立 Bug。目前先區分 H1（Page recompression / checksum 不一致，Case 2/3/4/5/7）與 H2（Same-page merge 的 source checksum 前提，Case 8），另外避免和已被其他人報告的 schema silent corruption [#23683](https://github.com/root-project/root/issues/23683)、相關 [#23684](https://github.com/root-project/root/pull/23684) 重複。

證據基準：
- [ROOT 6.40.00 的 12 組實測 CI #41866296529](https://github.com/Johnny-Kao/root-rntuple-research/actions/runs/41866296529)：6 PASS、6 異常，其他成功案例 260/260 數值吻合。
- [完整 Case matrix / 原始碼鑑識](RNTUPLE_MERGE_CHECKSUM_FINDINGS_2026-11-05.md)。
- 上游 master 靜態比對：[`root-project/root@916c66b`](https://github.com/root-project/root/commit/916c66b5933cfab7112e96c7839c3be1a4bfb095)；未執行 master binary。

## Gate 1 — 正式產品路徑，而非純實驗

**部分確認，未通過完整 Gate。**

- [PR #14398](https://github.com/root-project/root/pull/14398) 於 2024 年將 `RNTupleMerger` 整合到 `TFileMerger`，因此 `hadd` 能使用。
- 上游 [`RNTupleMerger.cxx`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RNTupleMerger.cxx) 的 `RNTuple::Merge` 明確標記「Entry point for TFileMerger」；預設建立帶有 checksum / same-page merge ON 的 `RNTupleWriteOptions`，並可從 `hadd` 指定目標 compression。來源 Ntuple 的 checksum 可以是 OFF（正式 Writer API 支援此選項）。
- [`main/src/hadd.cxx`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/main/src/hadd.cxx) 支援 `-f505` 等壓縮指定。
- **限制：** 現行 Repro 使用 `ROOT::Experimental::Internal::RNTupleMerger`，並非實際 `hadd`；上游 [Merging.md](https://root.cern/doc/master/md_tree_2ntuple_2doc_2Merging.html) 也明確將 Merger 稱為 experimental。尚不能宣稱一般 hadd 100% 可重現。
- **通過條件：** 至少一個乾淨、公開支援的 `hadd` 命令使用兩個合法 Writer 產物，在當前 master 重現同類可觀察錯誤；保留完整 command/exit code/版本/簡化輸入。

## Gate 2 — 是否造成「計算結果靜默錯誤」

**未確認；目前不能作為提交理由。**

- 6 個 PASS 案例全部按 row oracle 驗證 260/260，未發現靜默數值改變。
- 6 個 FAIL 案例發生 Abort / SIGSEGV，並非「命令成功而值被寫錯」。
- 官方 [Merging.md](https://root.cern/doc/master/md_tree_2ntuple_2doc_2Merging.html) 的 Requirement R2 要求輸出值依來源順序相接；此目標是我們應該驗證的規格。
- **通過條件：** 找到合併回報成功、完成讀回、而值或 schema / page metadata 產生錯誤的具體正常情境；或明確證明 checksum 元資料被錯誤寫入且造成不易察覺的後續運算問題。不要用 crash 代替 silent corruption 證據。

## Gate 3 — 安全疑慮與必要處置

**安全風險線索存在，嚴重性未分類。**

- Case 3/7：glibc `free(): invalid next size (fast)`，表示釋放時 heap allocator 偵測到一致性問題，但不能由此直接定位首次錯誤寫入。
- Case 4/5：SIGSEGV，call stack 至 `RSealedPage::ChecksumIfEnabled()`。
- Case 2：輸出讀取時 `dataLen > nbytes`。
- 上游靜態 invariant：[`RNTupleMerger.cxx`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RNTupleMerger.cxx) 在 L2 重壓縮時 buffer size 用 source checksum，Page size 用 destination checksum，Page HasChecksum 仍用 source。
- **缺失證據：** ASan/UBSan instrumented ROOT library、精確 OOB 地址與 caller、master 重現、外部不可信來源可否跨界觸發。只將測試程式加 sanitizer 但使用普通 PyPI ROOT binary 不足以歸因。
- **處置：** 若最新 master 在公開產品路徑重現 memory corruption，先判斷是否需依 ROOT 的安全回報流程私下通報；不要逕行公開額外觸發細節或先送普通安全修補 PR。

## Gate 4 — 歷史 PR / Reviewer 已做過的設計決策

**已檢查，必須尊重既有設計。**

- [#23940](https://github.com/root-project/root/pull/23940)：修的是 MiniFile Anchor decompression 的長度 invariant，不是 RNTuple Page 合併。Reviewer 先要求確保 buffer 有足夠空間、錯誤資訊正確、測試可維護；後來合併並安排 backport。此次不可視為那個漏洞的第二次提交。
- [#20112](https://github.com/root-project/root/pull/20112)：於 2025-12-04 合併 L2 recompression（只重新壓縮、不 reseal）；當時 Reviewer 明確討論應用來源 `HasChecksum` 還是目的端 `fWriteOpts`，並指出 L1 Fast Copy 可能例外。當時選擇以目的 Sink 選項為重要參考，但目前 L2 實作仍含 source/destination 不一致。**不能只把 L2 的 HasChecksum 改為 destination 而忽略 buffer allocation 與 L1 路徑。**
- [#15961](https://github.com/root-project/root/pull/15961)：為了幾個百分點的空間效益導入 same-page deduplication；checksum 被用作快速分辨相同 Page。
- [#17352](https://github.com/root-project/root/pull/17352)：明確將 same-page merging 與 checksum 改成兩個選項，但仍要求啟用 same-page 時具備 checksum；Reviewer 也特別要求 Assertion。不能一刀取消 dedup 或直接刪除 Assertion。
- [#23576](https://github.com/root-project/root/pull/23576)：2026 年重構 merger strategy，宣稱不改行為。改修之前必須基於**重構後的 master**，不能只對 6.40 舊程式套 Patch。
- [#20191](https://github.com/root-project/root/pull/20191)：記錄 L1/L2 行為及輸出值要等於來源串接；文件仍稱實驗性。
- **補充維護價值：上游既有 `ChangeCompression` 與 `ChangeCompressionMixed` 測試中的 `destinationNoChecksum`** 先被建立，而 `writeOpts.SetEnablePageChecksums(false)` 在後面才呼叫。[`RPageSink::RPageSink`](https://github.com/root-project/root/blob/916c66b5933cfab7112e96c7839c3be1a4bfb095/tree/ntuple/src/RPageStorage.cxx) 於建構時 `fOptions(options.Clone())`，所以其實沒有測到關閉 checksum 的 destination；而且測試來源 Writer 多為 checksum ON。這是可解釋 CI 盲點的實際 Test Coverage Gap。先加正反案例，不要直接擴張 PR。
- 搜尋相關 GitHub Issues/PRs，目前未查到同題已完成修正，但無法保證完全無重複；已知 [#23683/#23684](https://github.com/root-project/root/pull/23684) 涉及 **不同的 silent schema 讀寫錯誤**，不可混合。

## Gate 5 — Maintainer 的價值、負擔與 PR 範圍

**目前尚未通過提交門檻；潛在價值高但需要縮小。**

| 候選 | 動態證據 | 正式使用路徑 | Silent Wrong Result | Security | 維護價值 | 暫定 |
|---|---|---|---|---|---|---|
| **H1 L2 recompression / checksum**（Case 2/3/4/5/7） | 6.40 異常確認 | 有 `hadd` 入口；精確重現待測 | 無證據 | 有 heap/SIGSEGV 線索，待 ASan | 高；可能集中一個根因、既有測試漏掉 | **P1 研究，HOLD** |
| **H2 same-page missing input checksum**（Case 8） | 6.40 Assertion 確認 | 正式 Writer 支援 OFF，hadd 可能觸發 | 無證據 | 目前主要是崩潰／可用性 | 中至高；但 Assertion 是既有設計，需保留安全前提 | **P2 獨立研究，HOLD** |
| **H3 type checksum optional** | 尚無獨立 Repro | 不明 | 已知其他類似 schema 問題已在處理 | 不明 | 低；容易重複、範圍漂移 | **PARK** |

**提交形態（僅 Gate 通過後）：**
1. **一個根因一個提交**：H1 不是 5 個 Issue，H2 應獨立，H3 暫不提。
2. 每件提交包括：一個普通命令／最小 fixture、上游 master 的 fail→pass regression、既有 Reviewer 決策如何維持、最小 diff、預期的使用者錯誤類型。
3. 優先 **Regression Test + 有限修正**；不把 L1/L2 重新設計、Refactor、性能優化或 schema 改造打包到同一 PR。
4. 若設計意圖仍有歧義，先提出**簡短 Issue 或同既有 Maintainer 討論**，問是否應 fallback、正常拒絕或重新計算 Page checksum；避免沒有共識的行為修改。
5. 不因「有一個漂亮的修正」就提交；先證明實際受影響、修復價值大於 Review/Backport/Regression 成本。

## 目前尚缺的最少測試（不自動啟動耗資源的新 CI）

一個受控 staging matrix，並限量：
- A. 在 master 現有 build 上驗證 **public `hadd`**：來源正常、checksum OFF/ON、目標不同 compression；另做 same-page ON 對照。控制組必須在相同環境成功。
- B. 若 A 失敗，針對最精簡的 H1 Case 3/4/5/7 執行 **instrumented ROOT library ASan**；辨別 memory safety vs 只是 checksum metadata 不一致。
- C. 對成功返回的路徑完整比對輸出值、欄位型別、Page checksum / compression metadata；若仍無異常，不聲稱 Silent Data Corruption。
- D. 核對上游最新 Issue/PR 與當前實際 HEAD；確認沒有已經在 release branch 修正或已被其他人承接。

**本審查只建立 Gate 與既有源碼證據：未跑新 GitHub Actions、未寫 ROOT upstream、未開 Issue/PR。**
