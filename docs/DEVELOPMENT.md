# 開発者向けメモ

ビルド・書き込みの手順は [README.md](../README.md)（単体）と [LAUNCHER.md](LAUNCHER.md)（wararyoLauncher）を参照。ここでは PC シミュレータ、テスト、ソース構成、
設計上の注意、検証状況をまとめる。

仕様は SPECIFICATION.md v2.4（デザインキット `M5StopWatchi_CountDown_Design_Kit_v2.4.zip` 内）に準拠している。
デザインキット（約 84 MB、画像と ZIP）はリポジトリに含めていない（`.gitignore`）。

## ビルド構成

- ESP-IDF 5.5（PlatformIO `platform = espressif32 @ 6.12.0`、`framework = espidf`。wararyoLauncher と同じ構成）
- M5Unified 0.2.16 / M5GFX 0.2.28 / ArduinoJson 7.2.1（ヘッダのみ。Arduino フレームワークは使わない）
- [M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm) v1.1.0（Launcher 連携。`m5stopwatch-launcher` 環境だけが `lib_deps` で取得し、`CD_MULTIFIRM_GUEST` を定義する）

| ファイル | 役割 |
|---|---|
| `platformio.ini` | 環境 `m5stopwatch`（既定、単体用）/ `m5stopwatch-launcher`（wararyoLauncher のゲスト用）/ `native`（シミュレータ） |
| `CMakeLists.txt`, `src/CMakeLists.txt` | ESP-IDF のプロジェクトとコンポーネント（ソース一覧・Web UI の埋め込み）。**ソースを追加したら `src/CMakeLists.txt` にも足す** |
| `sdkconfig.defaults` / `sdkconfig.defaults.esp32s3` | ESP-IDF の設定（共通 / 機種別：電源管理、PSRAM、HTTP ヘッダ長など） |
| `partitions.csv` | 単体用のパーティション表（Launcher のゲストスロットでは使われない） |
| `tools/upload_guard.py` | `m5stopwatch-launcher` の書込保護（MultiFirm 配布物のコピー） |

PlatformIO は環境ごとに `sdkconfig.<環境名>` を生成する（リポジトリには含めない）。`sdkconfig.defaults*` を
変えたら `sdkconfig.<環境名>` を消してからビルドし直す。

シリアルログは `pio device monitor`（115200 bps、USB Serial/JTAG）で見られる。

## PC シミュレータ（native + SDL2）

StopWatch の画面と操作を PC 上のウィンドウ（466×466）で動かせる。

準備（Windows、初回のみ）: [MSYS2](https://www.msys2.org/) の UCRT64 に g++ と SDL2 を入れる。

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2"
```

Linux / macOS では g++ と SDL2 の開発パッケージ（例: `libsdl2-dev`、`brew install sdl2`）を入れる（未確認）。

```powershell
pio run -e native                        # ビルド
pio run -e native -t exec                # 実行
.pio\build\native\program.exe --demo     # サンプルイベントを入れて実行（保存済みイベントを置き換える）
```

| 実機 | シミュレータ |
|---|---|
| タッチ | マウス左ボタン（中央を 1.5 秒押し続けると設定） |
| KEY A | ← または A キー |
| KEY B | ↓ または B キー（A+B の同時押しも可） |

- `--demo` は 4 件のサンプル（うち 1 件は起動 90 秒後に到達、1 件は無効）を書き込む。最終カウントダウンや到達表示の確認用。
- NVS と RTC は作業フォルダの `sim_data/` に保存される（消すと初期状態）。RTC は最初 PC の UTC 時刻を持つ。
- Wi-Fi・Web 設定・NTP・Launcher 連携は動かない（`sim/` の差し替え部品）。WEB CONFIG 画面は表示だけ確認できる。
- `tools/native_env.py` が `C:\msys64\ucrt64\bin` を PATH の先頭に足す（Git for Windows の mingw64 などとの DLL 衝突を避けるため）。
  別の場所なら環境変数 `MSYS2_UCRT64_BIN` で指定する。

## テスト

Linux / WSL / macOS の g++（C++17）で実行する。ArduinoJson と M5GFX は PlatformIO が `.pio/libdeps/` に取得したものを使うので、
先に一度 `pio run`（または `pio pkg install`）を実行しておく。

```sh
sh tests/run_tests.sh           # ホスト用ユニットテスト
sh tools/preview/build.sh       # .pio/preview/out/*.png に全画面を描画し、部分描画が全面描画と一致するか検査
```

ユニットテストは 257 チェック（日時・閏年・月末、NTP 時刻の 2036 年桁あふれ、カウントダウン/進捗 0/50/100%、UTF-8 名、10 件循環と無効スキップ、
選択フォールバック、到達通知と抑止、RTC の UTC/JST 往復と二重 +9h なし、RTC 書込失敗、RTC MODE 切替時の信頼性チェック、NTP/Wi-Fi タイマー、
ボタン/タッチのジェスチャと A+B 抑止、JSON v3 保存と v2 移行、段階外の明るさ・減光時間の ＋/−）。

## ソース構成

```text
src/main.cpp                    app_main（checkStartupEscape → NVS → M5.begin → 電源管理 → UI ループ）
src/app/App.{h,cpp}             状態変更の一元管理（検証→永続化→反映）、描画（全面/部分）、待ち時間、減光、通知、NTP 連携
src/core/                       ハードウェア非依存（ホストでユニットテスト）
  TimeMath.h                    UTC⇔暦、JST/UTC、ISO 解析（TZ/tzset 不使用）
  CountdownEngine               残り時間・進捗（int64 秒、進捗は double）・画面状態
  EventRepository               最大 10 件、選択/循環/フォールバック、到達通知の記録
  ClockService                  時計の唯一の所有者。RTC 読み書き（UTC / LOCAL JST）、検証、世代番号
  SyncScheduler                 NTP（10 s / 60-300-900 s / 6 h）と STA 接続（15 s）のタイマー
  DateEditor / DeviceSettings / Utf8
src/input/InputRouter           短押し/長押し/タップ/長押しタッチ、A+B 抑止
src/ui/                         すべてベクタ描画（画像の貼り付けなし）
  Painter / LayoutPolicy        設計座標（丸型 466、長方形 320×240）→ 実パネル
  HudFont                       角張った右傾斜のストロークフォント（見出し・ボタン）
  SevenSegmentRenderer          7 本の六角形セグメント + 芯 + 2 段の発光 + 消灯セグメント
  ProgressRing                  52 区画リング（12 時から時計回り、ベベル・金属縁）/ 水平バー
  HudWidgets                    角落とし二重枠、斜線帯、ロケット、赤グリッド
  CountdownScreen / SettingsScreen（メニュー、一覧、日時設定、Web 設定、オプション、時刻状態、RTC モード）
src/services/
  SettingsStore                 NVS 名前空間 m5cd_v2。書込後に読み返して検証
  JsonCodec                     schemaVersion 3（UTC 秒）、v2 からの移行
  NetworkService / NtpSync      STA/AP 管理（esp_wifi、無線は必要なときだけ起動）、NTP はワーカータスク + 世代付きキュー（lwIP）
  WebConfigServer               Web UI と JSON API（esp_http_server。ハンドラは UI ロックを取って App を呼ぶ）
src/hal/                        DeviceCapabilities（機種判定）、RtcHal、LauncherBridge（MultiFirm）、
                                KvStore（NVS）、PowerHal（DFS・light sleep・入力割り込み）、UiTask（UI ロック）
web/index.html                  Web UI（firmware に埋め込み。LittleFS は使わない）
tests/                          ホスト用ユニットテスト
tools/upload_guard.py           MultiFirm の書込保護
tools/native_env.py             シミュレータのビルド用 PATH 調整（Windows）
tools/preview/                  画面を PNG に描画するホスト用ハーネス
sim/                            PC シミュレータ用の差し替え（KvStore・RTC・ネットワーク、エントリポイント）
```

## 省電力と描画（wararyoLauncher に倣う）

| 項目 | 内容 |
|---|---|
| CPU | DFS 240/80 MHz + tickless idle（`CONFIG_PM_ENABLE`）。描画など処理中だけ 240 MHz、待機中は 80 MHz |
| 待ち方 | 毎ループ 4 ms のポーリングをやめ、次の期限（次の秒、トースト・振動・保存・減光など）まで待つ。KEY A / KEY B / タッチ INT（GPIO 2 / 1 / 13）の割り込みで即座に起きる。押下中と操作後 150 ms は 10 ms 間隔で入力を追う。無線が動いている間は最長 100 ms、それ以外は最長 1 秒 |
| light sleep | 減光中・電池駆動（USB 給電なし）・無線オフ・音や振動なしのときだけ許可（`hal/PowerHal`）。パネルは自分のメモリで表示を続け、毎秒の更新で起きる。CS（GPIO 39）は sleep 中プルアップ |
| USB 給電の確認 | I2C 読み出しのため、毎ループではなく 2 秒ごと |
| 緑の LED | M5PM1 が電源投入時に点けるもの。Launcher と同じく充電中だけ点灯し、それ以外は消す（M5PM1 PWR_CFG bit 4、USB 給電の確認と同じ 2 秒ごと） |
| 描画 | M5StopWatch のパネルは M5GFX が PSRAM のフレームバッファを持ち、`endWrite()` で変わった範囲だけ転送する。そこへ直接描き、自前の全画面ダブルバッファ（434 KB）と毎秒の全画面コピー・全画面転送をやめた |
| 部分描画 | 時間経過だけの秒の更新では、画面が宣言した `liveRect()`（カウントダウンは HH:MM:SS の枠）だけを小さなキャンバスに描いて送る（466×466 → 約 310×70、転送量は約 1/10）。日数・状態・イベント・進捗（`contentKey()`）や背景（`backgroundKey()`）が変わったとき、操作・トーストのときは全面描画 |
| 時刻を表示しない画面 | メニュー・日時設定・オプションは秒ごとに描かない（`ticks()`） |
| Wi-Fi | 必要なとき（家庭用 Wi-Fi の登録あり、または Web 設定中）だけ無線を起動し、どちらも不要なら停止 |

実機での消費電流・描画時間はまだ測っていない（下記「未検証」）。

## 検証結果

### 実機で検証済み（2026-10-26、単体 `m5stopwatch`）

- ESP-IDF 版の起動、AMOLED の表示、タッチ・長押し・ボタン
- 設定・イベントの NVS 保存、RX8130CE への書込・読み返し、電源 OFF/ON 後の復元
  （別のファームウェアから入れ替えた本体では NVS の消去が必要なことがある。README「別のファームウェアから入れ替えた場合」）
- SoftAP 接続と Web からの設定・イベント編集、STA 接続・NTP 自動補正

### README の書き込み手順（2026-10-27、実機）

- Releases の `EventCountDown-v0.1.0-standalone.bin`：SHA-256 がリリースノートと一致、esptool v5.6.0 の `read-flash`（16 MB、約 80 秒）と
  `write-flash 0x0` が成功し、起動ログで `EventCountDown` 0.1.0 の起動を確認。イメージの `0x9000`〜`0x10000` は 0xFF なので、
  書き込み後の NVS は空になる（設定・イベントは消える）
- NVS 消去：esptool v5 の `erase-region 0x9000 0x5000`、PlatformIO の esptool 4.9 の `erase_region` とも成功
- ソースから：`pio run -t upload`（COM ポートの自動検出、既定の `m5stopwatch` だけをビルド）と `--upload-port` 指定が成功
- バックアップ：PlatformIO の esptool 4.9 の `read_flash`（16 MB）も成功するが約 24 分かかる（91.8 kbit/s）

### 検証済み（実機なし、2026-10-24 の ESP-IDF 移行後）

| 項目 | 結果 |
|---|---|
| `pio run -e m5stopwatch-launcher`（当時の既定環境） | 成功（ESP-IDF 5.5.0）。firmware.bin **1,547,456 bytes**（ゲスト上限 2,031,616 の 76%）、静的 RAM 43.4 KB。自作コードの警告なし |
| MultiFirm `inspect` | 「受け付け可能」（ゲスト上限内、付加 SHA 有効、project_name `EventCountDown`） |
| upload guard | ゲスト用環境で `pio run -t upload` が拒否されることを確認 |
| `pio run -e m5stopwatch`（単体、2026-10-26 の環境分割後） | 成功。firmware.bin 1,539,888 bytes、MultiFirm のコードを含まないことを確認。ゲスト用は 1,547,904 bytes で `inspect` 受け付け可能（version `0.1.0`） |
| `pio run -e native`（シミュレータ） | ビルド成功（起動しての操作確認は未実施） |
| ユニットテスト（WSL g++ 13） | 257 passed / 0 failed |
| 部分描画 | プレビュー用ハーネスで、秒ごとの部分描画を全面描画とピクセル比較。丸型（時・日の境目、最終 1 時間、到達）と長方形（日の境目）の 360 フレームで不一致 0 |
| 画面描画 | 全 18 画面を描画。`drawWideLine()` の移植前後で PNG がバイト単位で一致 |

### 未検証（実機が必要）

- 消費電流（減光中の light sleep、DFS）、描画時間、入力割り込みでの復帰、light sleep 中の画面表示の乱れの有無
- MultiFirm スロットへの導入、A+B 1.6 秒での Launcher 復帰、KEY B 起動脱出、Launcher 往復で時刻が 9 時間ずれないこと
- Arduino 版で保存した NVS データの読み込み
- Web 設定の 5 分無操作停止、STA の切断/再接続・タイムアウト/DNS 失敗時の継続（ロジックはユニットテストのみ）
- 到達時の音・振動

## 設計上の注意・既知の制限

- **NVS:** 名前空間 `m5cd_v2` のみ使用。読めないイベントデータは内容ごとのキー `evbak_<hash>` に退避し
  （既存の退避は上書きしない）、退避に失敗したときはイベントの変更を拒否して元データを守る。Wi-Fi は `WIFI_STORAGE_RAM`（`nvs_enable = 0`）で、資格情報を共有 NVS
  （Launcher と共用）に書かない。Arduino 版と同じ型（blob / str / i64）で保存するので、Arduino 版で保存した設定・イベントはそのまま読める設計（実機では未確認）。初回導入やパーティション変更で NVS が消える場合がある（MultiFirm の仕様）。
- **時刻:** 内部・保存値はすべて UTC 秒（int64）。表示 TZ と各イベントの TZ は v1 では `Asia/Tokyo` と `UTC` のみ。
  RTC の妥当性は「年 ≥ 2024・暦として正しい・最後に確立した時刻より 1 日以上前でない」で判定する。
- **RTC MODE の切替**は RTC の値の「解釈」を変えるだけで、RTC 自体は書き換えない（仕様 2.14）。Web では
  60 秒有効のプレビュートークンが必要。新しい解釈の RTC が起動時と同じ妥当性チェックに通らない場合（クリア済み・古い値）は、
  モードだけ保存して動作中の時計はそのまま残す。
- **壁時計:** libc ではなく `RtcHal` が「基準 UTC + esp_timer の経過」で保持する（2099 年まで有効。esp_timer は
  light sleep 中も進む）。NTP は 2036 年の桁あふれ（era 1）を処理する。
- **通知:** 選択中の有効イベントが通常の時間経過で到達したときだけ、記録を NVS に保存できた場合に最大 1 秒鳴らす/振動。
  保存に失敗した場合は 10 秒ごとに再試行し、60 秒以内に保存できればその時点で通知する。
  起動時・時刻補正・非選択イベントでは鳴らさない。音は初期 OFF。
- **Web の保護:** WPA2（本体表示の鍵）、AP インターフェイス以外からの要求拒否、Host ヘッダ検査、
  変更系 API はセッショントークン（`X-CD-Session`）必須、本文 8 KiB 上限。
- **日本語:** `lgfxJapanGothicP_24` を使用（外部フォントは同梱しない）。フォントにない文字（絵文字など）は □ に置換、
  長い名前は … で省略。240×135 のような極小画面では日本語が縮小で潰れる（数字表示を優先）。
- **長方形機種の設定画面**は丸型 466 レイアウトを縮小表示する簡易対応。
- **ESP32（Core2 など）はビルドできない:** M5Unified 0.2.16 が ESP-IDF 5.5 の ESP32 で新しいタッチドライバのヘッダを読みながら
  旧型 `touch_pad_t` を使うため、コンパイルエラーになる（ライブラリ側の問題）。Launcher と同じ版に揃えるため M5Unified は上げず、
  `env:m5stack-core2` は外した。M5Unified が対応したら `sdkconfig.defaults.esp32` を用意して環境を戻せる。
- **M5GFX の `drawWideLine()`:** 呼び出し側のクリップ矩形を自分の外接矩形で置き換えて最後に解除する（さらに幅・高さの位置に
  右下の座標を渡す）。部分描画が全面描画と一致しなくなるため、`Painter::line()` は同じ走査・同じ被覆率の移植版（クリップを守る）を使う。
  全画面の描画結果は元の関数と同一（全 18 画面のプレビュー PNG がバイト単位で一致）。
- **Web 設定のスレッド:** esp_http_server は自分のタスクでハンドラを呼ぶ。ハンドラは UI ロック（`hal/UiTask`）を取ってから App を
  使い、終わると UI タスクを起こして再描画させる。停止時は待機中のハンドラが 503 で抜けるので `httpd_stop()` が詰まらない。
