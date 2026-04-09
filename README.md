# FTL Simulator

這是一個簡化版的 page-mapped FTL 模擬器，目前包含：

- Greedy GC
- hot/cold data 分離
- page-level write buffer

## Trace 來源

本專案使用的 trace 來源為 SNIA IOTTA：

- `http://iotta.snia.org/traces/block-io/4964`

下載時請選擇：

- `Systor '17 Paper Traces Part 1`

下載後使用的檔案名稱為：

- `systor17-01.tar`

這個 tar 檔內部包含多個原始 trace 檔，檔名格式如下：

- `<date>-LUN<id>.csv.gz`

原始 CSV 的欄位格式為：

```text
Timestamp,Response,IOType,LUN,Offset,Size
```

## 執行流程
## 1. 準備原始 trace
請將 `systor17-01.tar` 放在專案根目錄，接著在 MSYS2 UCRT64 或 Git Bash 中執行：

```bash
bash prepare_traces.sh
```

此腳本會從 `systor17-01.tar` 中取出並合併：

- `LUN3.csv`
- `LUN4.csv`
- `LUN6.csv`

這三個檔案會放在專案根目錄下。

## 2. 轉換 trace
執行：
```powershell
python raw2trace.py
```
轉換後會產生：
- `rawfile952G_simple_LUN3.txt`
- `rawfile952G_simple_LUN4.txt`
- `rawfile952G_simple_LUN6.txt`
這三個檔案可視為三組不同的測資，分別對應不同的 LUN
- 若要實際拿其中一個 trace 給模擬器執行，請把它複製到 `files/rawfile0_20G.txt`

## 3. 把 952G Trace 映射到 16G Logical Space

`rawfile952G_simple_LUN3.txt`、`rawfile952G_simple_LUN4.txt`、
`rawfile952G_simple_LUN6.txt` 其實已經是 simulator 可讀的 trace 格式：

```text
0 W 1511745290 1
0 R 1225887468 8
```

問題在於這些 sector 編號來自很大的 address space，但目前 simulator 的
logical size 只有 `16 GiB`。在 `main.c` 裡，只要 request 不符合：

```text
sector + length <= logical_sectors
```

這筆 request 就會被直接跳過，所以原始 `952G` trace 會有很多資料被丟掉。

如果想在不改 simulator 容量的前提下，讓這些 trace 可以重播，可以執行：

```powershell
python remap_traces.py
```

這支腳本會直接讀取三個 `rawfile952G_simple_LUN*.txt`，把每筆 request 的
sector 用 wrap 的方式折回目前 `config.txt` 設定的 logical space，並輸出：

- `remapped_traces/rawfile952G_simple_LUN3_wrap.txt`
- `remapped_traces/rawfile952G_simple_LUN4_wrap.txt`
- `remapped_traces/rawfile952G_simple_LUN6_wrap.txt`

它使用邏輯是：

```text
wrapped_sector = sector % (logical_sectors - length + 1)
```

## 4. Config 設定

模擬器會讀取：

- `files/config.txt`

目前使用的設定如下：

```text
PsizeByte 18897436672
LsizeByte 17179869184
blockSizeByte 2097152
pageSizeByte 16384
sectorSizeByte 512
```

### 為什麼這樣設定

這份 config 是根據目前這個模擬器的實作方式決定的：

- `LsizeByte = 17179869184`
  - 代表 logical size 為 `16 GiB`
  - 目前程式在 replay trace 前，會先把整個 logical space 填滿
  - 因此 config 必須和目前使用的 trace window 對齊

- `blockSizeByte = 2097152`
  - 代表 block size 為 `2 MiB`

- `pageSizeByte = 16384`
  - 代表 page size 為 `16 KiB`

- `sectorSizeByte = 512`
  - trace 轉換時是以 `512 bytes` 為一個 sector

- `PsizeByte = 18897436672`
  - physical size 稍大於 logical size，代表有 over-provisioning
  - 這個值有特別調整成可以被 `blockSizeByte` 整除
  - 否則會在 `init.c` 的 assertion 失敗

### 關於 952G 與 16G

原始 trace 來自較大的 logical address space，`raw2trace.py` 中的 `952G` 代表的是原始 trace 的 address range 來源，而不是目前這個模擬器真正模擬的 SSD 容量。

目前這個 simulator 實際跑的是：

- `16 GiB` logical size
- 約 `17.6 GiB` physical size

也就是說，現在是在使用原始大 trace 中的一個較小工作區間來做模擬。

## 5. 編譯

如果你是在 PowerShell 中直接使用 MSYS2 安裝的 GCC，可以執行：

```powershell
C:\msys64\ucrt64\bin\gcc.exe -Wall -Wextra -g -o ftl.exe main.c init.c write.c GC.c
```

如果你是在 MSYS2 UCRT64 內，則可執行：

```bash
gcc -Wall -Wextra -g -o ftl.exe main.c init.c write.c GC.c
```

## 6. 執行

請先確認以下檔案存在：

- `files/config.txt`
- `files/rawfile0_20G.txt`

接著執行：

```powershell
.\ftl.exe
```

執行後輸出會類似：

```text
Initializing ./files/config.txt ...
Fill Logical Block (17179869184 bytes) ...
Start ./files/rawfile0_20G.txt ...
host    write: ...
actual  write: ...
amplification: ...
```
