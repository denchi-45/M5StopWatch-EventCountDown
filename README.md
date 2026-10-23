# M5StopWatchi_CountDown

楽しみなイベントまでの残り時間を、赤黒の SF 警報装置風の画面で数える M5StopWatch 用ファームウェア。
仕様は `M5StopWatchi_CountDown_Design_Kit/M5StopWatchi_CountDown_Design_Kit_v2.4.zip` 内の **SPECIFICATION.md v2.4** に準拠
（展開済みフォルダ直下の SPECIFICATION.md は v2.1 の旧版。ZIP 内の v2.4 を優先した）。

- 対象: M5StopWatch（466×466 円形 AMOLED）。M5Unified 対応の長方形機種へも移植可能（Core2 でビルド確認済み）
- Arduino Framework + PlatformIO + M5Unified 0.2.16 + M5GFX 0.2.28 + ArduinoJson 7.2.1
- wararyoLauncher（M5StopWatch-MultiFirm v1.1.0）の**ゲスト**として動作

## 操作

| 画面 | 入力 | 動作 |
|---|---|---|
| カウントダウン | KEY A 短押し / KEY B 短押し | 前 / 次の有効イベント（循環、2 秒後に NVS 保存） |
| カウントダウン | 画面中央を 1.5 秒長押し | 設定メニュー |
| カウントダウン（タッチなし機種） | KEY B 1.5 秒長押し | 設定メニュー |
| どこでも | KEY A+B 1.6 秒長押し | Launcher に戻る（MultiFirm ゲスト時。アプリ側の操作は発火しない） |
| 電源投入時 | KEY B を押しながら起動 | アプリを起動せず Launcher へ |
| 設定画面 | タップ | 項目選択・実行、＋/− |
| 設定画面 | A / B 短押し | 前 / 次の項目（編集中は値の減 / 増） |
| 設定画面 | B 1.5 秒長押し | 開く・実行・編集開始 / 編集値の確定 |
| 設定画面 | A 1.5 秒長押し | 戻る（未保存は破棄） / 編集値の取り消し |

減光中の最初の短押し・タップは画面を起こすだけで、操作としては扱わない。

## ビルド

```powershell
pio run                      # = pio run -e m5stopwatch（MultiFirm ゲスト用）
```

成果物は `.pio/build/m5stopwatch/firmware.bin`。

> **`pio run -t upload` は使えません（意図的）。** `tools/upload_guard.py`（MultiFirm 配布物のコピー）が
> `upload / uploadfs / erase` を拒否し、bin が 0x1F0000 を超えるとビルドエラーにする。通常の upload は
> Launcher・共存パーティション表・他アプリを上書きするため。

## Launcher 導入済み本体への書き込み（ゲストスロット）

M5StopWatch-MultiFirm の PC ツールを使う（準備は `../M5StopWatch-MultiFirm/tools/README.md`）。

```powershell
cd ..\M5StopWatch-MultiFirm
# 1) 予定の確認（dry-run、書き込まない）
tools\multifirm.ps1 install --slot 2 ..\M5StopWatch_CountDown\.pio\build\m5stopwatch\firmware.bin --name "Countdown" --port COM11
# 2) 内容を確認してから実行
tools\multifirm.ps1 install --slot 2 ..\M5StopWatch_CountDown\.pio\build\m5stopwatch\firmware.bin --name "Countdown" --port COM11 --execute
```

スロット番号（1〜3）・ポートは環境に合わせる。オフライン検査は
`python tools\multifirm.py inspect <firmware.bin>` で行える（2026-10-23 実施: 「受け付け可能」）。

### Launcher への戻り方

1. カウントダウン/設定のどの画面でも **KEY A + KEY B を 1.6 秒押し続ける**（途中で画面に触れると取り消し）。
2. 起動しない・固まった等のときは **KEY B を押しながら電源を入れる**（M5 初期化前に Launcher へ戻る）。

### Launcher を使わない単体機

```powershell
pio run -e m5stopwatch-standalone -t upload
```

`custom_allow_full_upload = yes` の環境。**フラッシュ全体の配置を上書きする**ため、Launcher 導入機には使わない。

## 初回セットアップ（スマホ / PC）

1. 画面長押し → `WEB CONFIG`。本体が SoftAP を起動し、SSID（`M5-Countdown-XXXX`、MAC 末尾）、
   パスワード、URL を表示する。
2. パスワードは**本体固有の乱数（約 59 bit、`XXXX-XXXX-XXXX`）**で初回に生成して NVS に保存。配布方法は
   「本体画面に表示されたものを手で入力する」のみ（Web や シリアルには出さない）。
3. スマホで接続し `http://192.168.4.1`（実 IP は画面表示）を開く。EVENTS / DEVICE / TIME / WI-FI と EXIT SETUP。
4. 実際の操作（入力・タップ）が 5 分ないと AP は自動停止（状態ポーリングでは延長しない。残り 30 秒で警告）。

時刻はインターネットなしでも TIME → `SYNC FROM PHONE / PC` で設定できる。家庭用 Wi-Fi を WI-FI に登録すると、
接続（IP 取得）のたびに NTP で自動補正し、接続中は 6 時間ごとに再補正する。

## ソース構成

```text
src/main.cpp                    setup/loop（checkStartupEscape → M5.begin → App）
src/app/App.{h,cpp}             状態変更の一元管理（検証→永続化→反映）、描画、減光、通知、NTP 連携
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
  NetworkService / NtpSync      STA/AP 管理、NTP はワーカータスク + 世代付きキュー
  WebConfigServer               Web UI と JSON API
src/hal/                        DeviceCapabilities（機種判定）、RtcHal、LauncherBridge（MultiFirm）
web/index.html                  Web UI（firmware に埋め込み。LittleFS は使わない）
tests/                          ホスト用ユニットテスト
tools/upload_guard.py           MultiFirm の書込保護
tools/preview/                  画面を PNG に描画するホスト用ハーネス
```

## テスト

```sh
sh tests/run_tests.sh           # g++ (C++17)。Windows では WSL で実行
sh tools/preview/build.sh       # .pio/preview/out/*.png に全画面を描画
```

ユニットテストは 257 チェック（日時・閏年・月末、NTP 時刻の 2036 年桁あふれ、カウントダウン/進捗 0/50/100%、UTF-8 名、10 件循環と無効スキップ、
選択フォールバック、到達通知と抑止、RTC の UTC/JST 往復と二重 +9h なし、RTC 書込失敗、RTC MODE 切替時の信頼性チェック、NTP/Wi-Fi タイマー、
ボタン/タッチのジェスチャと A+B 抑止、JSON v3 保存と v2 移行、段階外の明るさ・減光時間の ＋/−）。

## 検証結果（2026-10-23）

### 検証済み（実機なし）

| 項目 | 結果 |
|---|---|
| `pio run -e m5stopwatch` | 成功、警告なし。firmware.bin **1,406,288 bytes**（ゲスト上限 2,031,616 の 69%）、RAM 静的 48.7 KB |
| `pio run -e m5stack-core2`（長方形・ESP32） | 成功（1,443,465 bytes） |
| MultiFirm `inspect` | 「受け付け可能」（ゲスト上限内、付加 SHA 有効） |
| upload guard | `pio run -t upload` が拒否されることを確認 |
| ユニットテスト（WSL g++ 13） | 257 passed / 0 failed |
| 画面描画 | 実際の画面クラスを M5GFX（Linux ビルド）で 466×466 / 320×240 / 240×135 に描画して目視確認。AddressSanitizer でエラーなし |
| Web UI | JS 構文チェック、ローカル日時⇔UTC 変換（JST/UTC・存在しない日付の拒否）を Node で確認 |
| Launcher の RTC 形式 | Launcher のソース/テスト（`M5Hal::writeRtc`、1 Feb 00:00 JST → RTC 31 Jan 15:00）から **UTC 保存**と判断。既定の `UTC` 解釈と一致 |

### 未検証（実機が必要）

- 実機での表示（AMOLED の輝度・発光の見え方、円の縁の余白、描画時間）、タッチ座標・長押し、ボタン
- MultiFirm スロットへの導入、A+B 1.6 秒での Launcher 復帰、KEY B 起動脱出、Launcher 往復で時刻が 9 時間ずれないこと
- RX8130CE への書込・読み返し、電源 OFF/ON 後の復元
- SoftAP 接続、Web からの CRUD・並び替え・選択、5 分無操作停止、ブラウザ時刻送信
- STA 接続・NTP 自動補正・切断/再接続・タイムアウト/DNS 失敗時の継続（ロジックはユニットテストのみ）
- 到達時の音・振動（StopWatch の振動モータは M5Unified の `setVibration` で駆動する想定）
- 長方形機種の実機表示、Core2 以外の機種

## 設計上の注意・既知の制限

- **NVS:** 名前空間 `m5cd_v2` のみ使用。読めないイベントデータは内容ごとのキー `evbak_<hash>` に退避し
  （既存の退避は上書きしない）、退避に失敗したときはイベントの変更を拒否して元データを守る。`WiFi.persistent(false)` で Wi-Fi 資格情報を共有 NVS
  （Launcher と共用）に書かない。初回導入やパーティション変更で NVS が消える場合がある（MultiFirm の仕様）。
- **時刻:** 内部・保存値はすべて UTC 秒（int64）。表示 TZ と各イベントの TZ は v1 では `Asia/Tokyo` と `UTC` のみ。
  RTC の妥当性は「年 ≥ 2024・暦として正しい・最後に確立した時刻より 1 日以上前でない」で判定する。
- **RTC MODE の切替**は RTC の値の「解釈」を変えるだけで、RTC 自体は書き換えない（仕様 2.14）。Web では
  60 秒有効のプレビュートークンが必要。新しい解釈の RTC が起動時と同じ妥当性チェックに通らない場合（クリア済み・古い値）は、
  モードだけ保存して動作中の時計はそのまま残す。
- **2038 年問題:** Arduino-ESP32 2.x の `time_t` は 32 bit のため、壁時計は libc ではなく `RtcHal` が
  「基準 UTC + esp_timer の経過」で保持する（2099 年まで有効）。NTP は 2036 年の桁あふれ（era 1）を処理する。
- **通知:** 選択中の有効イベントが通常の時間経過で到達したときだけ、記録を NVS に保存できた場合に最大 1 秒鳴らす/振動。
  保存に失敗した場合は 10 秒ごとに再試行し、60 秒以内に保存できればその時点で通知する。
  起動時・時刻補正・非選択イベントでは鳴らさない。音は初期 OFF。
- **Web の保護:** WPA2（本体表示の鍵）、AP インターフェイス以外からの要求拒否、Host ヘッダ検査、
  変更系 API はセッショントークン（`X-CD-Session`）必須、本文 8 KiB 上限。
- **日本語:** `lgfxJapanGothicP_24` を使用（外部フォントは同梱しない）。フォントにない文字（絵文字など）は □ に置換、
  長い名前は … で省略。240×135 のような極小画面では日本語が縮小で潰れる（数字表示を優先）。
- **長方形機種の設定画面**は丸型 466 レイアウトを縮小表示する簡易対応。
- 描画は 1 秒ごと（と操作時）に全画面スプライトを転送する方式。静的な背景はスプライトにキャッシュする。
