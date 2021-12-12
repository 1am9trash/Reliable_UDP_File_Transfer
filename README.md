Reliable UDP File Transfer
---

- **目的**：使用UDP傳輸檔案，嘗試在惡劣的網路環境下仍保持良好可靠的傳輸行為
- Makefile
  - `make`：可編譯執行檔
  - `make clean`：可清除執行檔
  - 以下網路設置部分，須在bsd based的作業系統（如mac OS）上才能使用，若使用linux，請使用`tc`控制
  - `make build_net_env`：設定模擬網路環境
  - `make close_net_env`：關閉模擬網路環境設置
  - 可在Makefile中調整`dnctl`指令，改變模擬的網路環境，我原始設定為`dnctl pipe 1 plr 0.1 delay 200 bw 200Kbit/s`，亦即掉包率10%、延遲200 ms、頻寬200 kb/s
- 產生傳送的檔案
  - `dd if=/dev/urandom of=a_big_file bs=1000000 count=1`
  - bs為檔案大小、of為檔案名稱
- 比較傳輸後檔案是否相同
  - `diff <file1> <file2>`
- [simple tranfer](simple_transfer)
  - 簡單的transfer，每次傳送一個封包，便等待收到ack再傳下一個封包，若超時，則簡單判定封包丟失，重傳封包
	- 可達成reliable的檔案傳輸，但在網路不穩時，效能極差
- [selective repeat transfer](selective_repeat_transfer)
  - 依照selective repeat的算法傳輸檔案，並使用類似tcp slow start的方式依據網路回饋調整傳輸檔案速度
  - 不同網路環境傳送1MB資料的表現
    - 無限制：0.078s、重傳率0%
    - 10%掉包率：4.571s、重傳率13%
    - 20%掉包率、delay 200ms、bandwidth 200kb/s：11m45.750s、重傳率379%