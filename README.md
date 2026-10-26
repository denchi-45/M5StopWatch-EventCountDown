# EventCountDown

楽しみなイベントまでの残り時間を、赤黒の SF 警報装置風の画面で数える **M5StopWatch** 用ファームウェアです。

- 最大 10 件のイベントを登録し、ボタンで切り替えて表示（日数 + HH:MM:SS の 7 セグ表示と 52 区画の進捗リング）
- イベントの登録・時刻合わせ・Wi-Fi 設定は、本体の Wi-Fi アクセスポイントにスマホ / PC から接続して Web 画面で行う
- 家庭用 Wi-Fi を登録すると NTP で自動的に時刻を補正
- [wararyoLauncher](https://github.com/wararyo/M5StopWatch-wararyoLauncher)（[M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm)）の
  **ゲストアプリ**として動作。Launcher を入れていない本体に単体で書き込むこともできる

> [!WARNING]
> 現在の ESP-IDF 版は**実機での動作確認がまだ済んでいません**（ビルド・ユニットテスト・PC シミュレータでの描画確認のみ）。
> 詳細は [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md#検証結果2026-10-24esp-idf-移行後) を参照してください。

## 必要なもの

| 種類 | 内容 |
|---|---|
| 本体 | M5StopWatch（ESP32-S3、466×466 円形 AMOLED、16 MB Flash） |
| ケーブル | データ通信できる USB-C ケーブル |
| ビルド | [PlatformIO](https://platformio.org/)（VS Code 拡張または PlatformIO Core CLI）。ESP-IDF 5.5 とライブラリは初回ビルド時に自動で取得される（Releases の bin を使う場合は不要） |
| 書き込み（Launcher 導入機） | Python 3.11 + esptool 4.12.0（[M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm) の PC ツールが使う） |

## ダウンロード（ビルドしない場合）

Launcher 導入済みの本体に入れるだけなら、ビルドせずに [Releases](https://github.com/denchi-45/M5StopWatch_Coundown/releases)
の `firmware.bin`（ゲストスロット用）を使えます。

1. [Releases](https://github.com/denchi-45/M5StopWatch_Coundown/releases) から使いたい版を開き、Assets の `firmware.bin` をダウンロードします。
2. リリースノートの SHA-256 と一致するか確認します。

   ```powershell
   Get-FileHash "$HOME\Downloads\firmware.bin" -Algorithm SHA256   # Windows
   ```

   ```sh
   shasum -a 256 ~/Downloads/firmware.bin                          # macOS / Linux
   ```

3. 下の「[A. Launcher 導入済みの本体](#a-launcher-導入済みの本体ゲストスロットに入れる)」の手順で書き込みます
   （`$bin` / `bin` をダウンロードしたファイルのパスにします）。

Releases の bin はゲストスロット専用です。Launcher を使わない本体（B）には、下の手順でソースからビルドして書き込んでください。

## ビルド

```sh
git clone https://github.com/denchi-45/M5StopWatch_Coundown.git
cd M5StopWatch_Coundown
pio run                      # = pio run -e m5stopwatch（MultiFirm ゲスト用）
```

成果物は `.pio/build/m5stopwatch/firmware.bin` です（初回は ESP-IDF などのダウンロードで時間がかかります）。

| 環境 | 用途 |
|---|---|
| `m5stopwatch`（既定） | Launcher のゲストスロット用。`-t upload` は使えない（下記） |
| `m5stopwatch-standalone` | Launcher を入れていない本体に直接書き込む用 |
| `native` | PC シミュレータ（[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md#pc-シミュレータnative--sdl2)） |

> [!NOTE]
> 既定の環境では **`pio run -t upload` / `erase` / `uploadfs` は意図的にエラーになります。** 通常の書き込みは
> Launcher・共存用パーティション表・他のアプリを上書きしてしまうためです（`tools/upload_guard.py`）。
> また firmware.bin がゲストスロットの上限（0x1F0000 = 2,031,616 bytes）を超えるとビルドエラーになります。

## 書き込み

本体に wararyoLauncher が入っているかどうかで手順が変わります。

### A. Launcher 導入済みの本体（ゲストスロットに入れる）

本体はあらかじめ MultiFirm v1 の配置になっている（wararyoLauncher が入っている）必要があります。
Launcher の導入方法は [wararyoLauncher](https://github.com/wararyo/M5StopWatch-wararyoLauncher) と
[MultiFirm のツール説明](https://github.com/wararyo/M5StopWatch-MultiFirm/blob/master/tools/README.md) を参照してください。

**1. MultiFirm の PC ツールを準備する（初回のみ）**

このリポジトリと同じ階層に clone します（Releases の bin だけを使う場合は任意の場所で構いません）。

```powershell
cd ..
git clone https://github.com/wararyo/M5StopWatch-MultiFirm.git
cd M5StopWatch-MultiFirm
py -3.11 -m venv tools\.venv
tools\.venv\Scripts\python.exe -m pip install -r tools\requirements.txt
```

macOS / Linux:

```sh
python3.11 -m venv tools/.venv
tools/.venv/bin/python -m pip install -r tools/requirements.txt
```

**2. シリアルポートを確認する**

| OS | 例 | 探し方 |
|---|---|---|
| Windows | `COM11` | デバイスマネージャーの「ポート (COM と LPT)」 |
| macOS | `/dev/cu.usbmodem1101` | `ls /dev/cu.usbmodem*` |
| Linux | `/dev/ttyACM0` | `ls /dev/ttyACM*`（権限がなければ `dialout` グループに追加） |

**3. ゲストスロットに書き込む**

EventCountDown のフォルダで実行します（`COM11` とスロット番号 `2` は環境に合わせて変更。スロットは 1〜3）。
Releases からダウンロードした bin を使う場合は、`$bin` / `bin` をそのファイルのパス（例: `$HOME\Downloads\firmware.bin`）に、
`$mf` / `mf` を M5StopWatch-MultiFirm を clone した場所に合わせてください。

```powershell
$mf = "..\M5StopWatch-MultiFirm\tools\multifirm.ps1"
$bin = ".pio\build\m5stopwatch\firmware.bin"
# 1) 計画を表示（実機に接続しない）
& $mf install --slot 2 $bin --name "EventCountDown" --port COM11
# 2) 実機を読み取って確認（書き込まない）
& $mf install --slot 2 $bin --name "EventCountDown" --port COM11 --check-device
# 3) 書き込む
& $mf install --slot 2 $bin --name "EventCountDown" --port COM11 --execute
```

macOS / Linux:

```sh
mf=../M5StopWatch-MultiFirm/tools/multifirm.sh
bin=.pio/build/m5stopwatch/firmware.bin
$mf install --slot 2 $bin --name "EventCountDown" --port /dev/ttyACM0
$mf install --slot 2 $bin --name "EventCountDown" --port /dev/ttyACM0 --check-device
$mf install --slot 2 $bin --name "EventCountDown" --port /dev/ttyACM0 --execute
```

- 指定したスロットに入っていたアプリは上書きされます。他のスロット・設定（NVS）・起動先は変わりません。
- 書き込み後は Launcher が起動するので、一覧から **EventCountDown** を選んでください。
- 実機なしでの検査: `python ..\M5StopWatch-MultiFirm\tools\multifirm.py inspect .pio\build\m5stopwatch\firmware.bin`
- ゲストから戻れなくなったときは `multifirm.ps1 recover --port COM11 --execute`（詳細は MultiFirm のツール説明）。

### B. Launcher を使わない本体（単体で書き込む）

> [!CAUTION]
> **フラッシュ全体の配置（ブートローダ・パーティション表）を上書きします。** Launcher 導入機には使わないでください。
> 元に戻したい場合に備えて、先に 16 MB 全体のバックアップを取っておくことをおすすめします。
>
> ```sh
> pio pkg exec -p "platformio/tool-esptoolpy@~2.40900.0" -- esptool.py --chip esp32s3 --port COM11 read_flash 0 0x1000000 backup.bin
> ```

```sh
pio run -e m5stopwatch-standalone -t upload                  # ポートは自動検出
pio run -e m5stopwatch-standalone -t upload --upload-port COM11   # ポートを指定する場合
```

### シリアルログ

```sh
pio device monitor -b 115200 -p COM11
```

## 初回セットアップ（スマホ / PC）

1. カウントダウン画面の中央を 1.5 秒長押し（タッチなし機種では KEY B を 1.5 秒長押し）して設定メニューを開き、
   `WEB CONFIG` を選びます。
   本体が Wi-Fi アクセスポイントを起動し、SSID（`EventCountDown-XXXX`）・パスワード・URL を画面に表示します。
2. パスワードは本体ごとに初回に生成される乱数（`XXXX-XXXX-XXXX`）です。本体の画面にだけ表示されます。
3. スマホ / PC でその SSID に接続し、画面に表示された URL（通常 `http://192.168.4.1`）を開きます。
   EVENTS / DEVICE / TIME / WI-FI の各ページで設定し、最後に EXIT SETUP。
4. 5 分間操作がないとアクセスポイントは自動で停止します（残り 30 秒で警告）。

時刻はインターネットがなくても TIME → `SYNC FROM PHONE / PC` で、接続中の端末の時計から設定できます。
家庭用 Wi-Fi を WI-FI に登録すると、接続のたびに NTP で自動補正し、接続中は 6 時間ごとに再補正します。

## 操作

| 画面 | 入力 | 動作 |
|---|---|---|
| カウントダウン | KEY A 短押し / KEY B 短押し | 前 / 次の有効イベント（循環、2 秒後に保存） |
| カウントダウン | 画面中央を 1.5 秒長押し | 設定メニュー |
| カウントダウン（タッチなし機種） | KEY B 1.5 秒長押し | 設定メニュー |
| どこでも | KEY A+B 1.6 秒長押し | Launcher に戻る（ゲスト時。アプリ側の操作は発火しない） |
| 電源投入時 | KEY B を押しながら起動 | アプリを起動せず Launcher へ |
| 設定画面 | タップ | 項目選択・実行、＋/− |
| 設定画面 | A / B 短押し | 前 / 次の項目（編集中は値の減 / 増） |
| 設定画面 | B 1.5 秒長押し | 開く・実行・編集開始 / 編集値の確定 |
| 設定画面 | A 1.5 秒長押し | 戻る（未保存は破棄） / 編集値の取り消し |

減光中の最初の短押し・タップは画面を起こすだけで、操作としては扱いません。

### Launcher への戻り方

1. どの画面でも **KEY A + KEY B を 1.6 秒押し続ける**（途中で画面に触れると取り消し）。
2. 起動しない・固まったなどのときは **KEY B を押しながら電源を入れる**（M5 の初期化前に Launcher へ戻る）。

## 既知の制限

- 表示・イベントのタイムゾーンは `Asia/Tokyo` と `UTC` のみ。
- 対応機種は M5StopWatch のみ（長方形画面の配置も持っているが、ESP32 機種は依存ライブラリの都合でビルドできない）。
- Launcher への初回導入やパーティション変更で、保存した設定・イベントが消える場合があります（MultiFirm の仕様）。

その他の設計上の注意、ソース構成、PC シミュレータ、テストの実行方法は [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) にあります。

## ライセンス

[MIT License](LICENSE)

依存ライブラリ（M5Unified / M5GFX / ArduinoJson / M5StopWatch-MultiFirm / ESP-IDF）はそれぞれのライセンスに従います。
`tools/upload_guard.py` は M5StopWatch-MultiFirm の配布物のコピーです。
