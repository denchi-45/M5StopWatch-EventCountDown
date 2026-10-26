# wararyoLauncher に入れて使う

EventCountDown は、[wararyoLauncher](https://github.com/wararyo/M5StopWatch-wararyoLauncher)
（[M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm)）の**ゲストアプリ**としても動かせます。
Launcher の一覧から起動し、ボタン操作で Launcher に戻れます。

単体で使う場合は [README.md](../README.md) を参照してください。

> [!WARNING]
> Launcher 導入済みの本体に、README の手順（`pio run -t upload` や Releases の standalone 版）で書き込まないでください。
> フラッシュ全体の配置が上書きされ、Launcher・共存用パーティション表・他のアプリが消えます。

## 必要なもの

- MultiFirm v1 の配置になっている（wararyoLauncher が入っている）M5StopWatch。
  Launcher の導入方法は [wararyoLauncher](https://github.com/wararyo/M5StopWatch-wararyoLauncher) と
  [MultiFirm のツール説明](https://github.com/wararyo/M5StopWatch-MultiFirm/blob/master/tools/README.md) を参照してください。
- Python 3.11 と esptool 4.12.0（MultiFirm の PC ツールが使う）
- データ通信できる USB-C ケーブル

## 1. ゲスト用の bin を用意する

次のどちらかを使います。

- **ダウンロード:** [Releases](https://github.com/denchi-45/M5StopWatch_Coundown/releases) の
  `EventCountDown-<版>-launcher.bin`。リリースノートの SHA-256 と一致するか確認してください。
- **ビルド:** このリポジトリで

  ```sh
  pio run -e m5stopwatch-launcher
  ```

  成果物は `.pio/build/m5stopwatch-launcher/firmware.bin` です。

この環境では **`pio run -t upload` / `erase` / `uploadfs` は意図的にエラーになります**（`tools/upload_guard.py`）。
また bin がゲストスロットの上限（0x1F0000 = 2,031,616 bytes）を超えるとビルドエラーになります。

## 2. MultiFirm の PC ツールを準備する（初回のみ）

```powershell
git clone https://github.com/wararyo/M5StopWatch-MultiFirm.git
cd M5StopWatch-MultiFirm
py -3.11 -m venv tools\.venv
tools\.venv\Scripts\python.exe -m pip install -r tools\requirements.txt
```

macOS / Linux:

```sh
git clone https://github.com/wararyo/M5StopWatch-MultiFirm.git
cd M5StopWatch-MultiFirm
python3.11 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

## 3. ゲストスロットに書き込む

M5StopWatch-MultiFirm のフォルダで実行します。`$bin` / `bin` は手順 1 の bin のパス、`COM11` はシリアルポート
（README の「[シリアルポート](../README.md#シリアルポート)」）、スロット番号 `2` は空いているスロット（1〜3）に合わせてください。

```powershell
$bin = "$HOME\Downloads\EventCountDown-v0.1.0-launcher.bin"
# 1) 計画を表示（実機に接続しない）
tools\multifirm.ps1 install --slot 2 $bin --name "EventCountDown" --port COM11
# 2) 実機を読み取って確認（書き込まない）
tools\multifirm.ps1 install --slot 2 $bin --name "EventCountDown" --port COM11 --check-device
# 3) 書き込む
tools\multifirm.ps1 install --slot 2 $bin --name "EventCountDown" --port COM11 --execute
```

macOS / Linux:

```sh
bin="$HOME/Downloads/EventCountDown-v0.1.0-launcher.bin"
tools/multifirm.sh install --slot 2 "$bin" --name "EventCountDown" --port /dev/ttyACM0
tools/multifirm.sh install --slot 2 "$bin" --name "EventCountDown" --port /dev/ttyACM0 --check-device
tools/multifirm.sh install --slot 2 "$bin" --name "EventCountDown" --port /dev/ttyACM0 --execute
```

- 指定したスロットに入っていたアプリは上書きされます。他のスロット・設定（NVS）・起動先は変わりません。
- 書き込み後は Launcher が起動するので、一覧から **EventCountDown** を選んでください。
- 実機なしでの検査（上で設定した `$bin` / `bin` を使います）:
  PowerShell では `python tools/multifirm.py inspect $bin`、macOS / Linux では `python3 tools/multifirm.py inspect "$bin"`

## Launcher への戻り方

| 操作 | 動作 |
|---|---|
| どの画面でも **KEY A + KEY B を 1.6 秒押し続ける** | Launcher に戻る（途中で画面に触れると取り消し。アプリ側の操作は発火しない） |
| **KEY B を押しながら電源を入れる** | アプリを起動せず Launcher へ（起動しない・固まったときの脱出用） |

ゲストから戻れなくなったときは、MultiFirm のフォルダで次を実行します（詳細は MultiFirm のツール説明）。

```powershell
tools\multifirm.ps1 recover --port COM11 --execute
```

```sh
tools/multifirm.sh recover --port /dev/ttyACM0 --execute   # macOS / Linux
```

## 注意

- 設定とイベントは NVS（Launcher と共用のパーティション）の名前空間 `m5cd_v2` に保存されます。
  Launcher の初回導入やパーティション変更で消える場合があります（MultiFirm の仕様）。
- Wi-Fi の資格情報は共有 NVS に書き込みません（`WIFI_STORAGE_RAM`）。
