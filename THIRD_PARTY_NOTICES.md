# サードパーティのライセンス表示

EventCountDown 自体のコードは [MIT License](LICENSE)（Copyright (c) 2026 denchi-45）です。

ビルドしたファームウェア（Releases の `standalone.bin` / `launcher.bin` を含む）には、次のソフトウェアとフォントが組み込まれています。
各ライセンスの全文は [`licenses/`](licenses/) にあります。

| 名称 | 版 | 著作権者 | ライセンス | 全文 | 組み込まれる bin |
|---|---|---|---|---|---|
| [M5Unified](https://github.com/m5stack/M5Unified) | 0.2.16 | Copyright (c) 2021 M5Stack | MIT | [M5Unified-LICENSE.txt](licenses/M5Unified-LICENSE.txt) | 両方 |
| [M5GFX](https://github.com/m5stack/M5GFX) | 0.2.28 | Copyright (c) 2021 M5Stack | MIT | [M5GFX-LICENSE.txt](licenses/M5GFX-LICENSE.txt) | 両方 |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.2.1 | Copyright © 2014-2024, Benoit BLANCHON | MIT | [ArduinoJson-LICENSE.txt](licenses/ArduinoJson-LICENSE.txt) | 両方 |
| [ESP-IDF](https://github.com/espressif/esp-idf) | 5.5.0 | Espressif Systems (Shanghai) CO LTD ほか | Apache-2.0 | [ESP-IDF-LICENSE.txt](licenses/ESP-IDF-LICENSE.txt) | 両方 |
| [M5StopWatch-MultiFirm](https://github.com/wararyo/M5StopWatch-MultiFirm) | 1.1.0 | Copyright (c) 2026 wararyo | MIT | [M5StopWatch-MultiFirm-LICENSE.txt](licenses/M5StopWatch-MultiFirm-LICENSE.txt) | `launcher.bin` のみ |

## ESP-IDF に含まれるコンポーネント

ESP-IDF には FreeRTOS・lwIP・Mbed TLS・newlib など、それぞれ独自のライセンスを持つコンポーネントが含まれます。
一覧と各ライセンスは Espressif の
[Copyrights and Licenses（ESP-IDF v5.5）](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/COPYRIGHT.html)
を参照してください。

## フォント

M5GFX に同梱されている次のフォントを画面表示に使っています。

| フォント | 用途 | 元になったもの | ライセンス | 全文 |
|---|---|---|---|---|
| `lgfxJapanGothicP_24` | イベント名（日本語） | [IPAex ゴシック](https://moji.or.jp/ipafont/)（`ipaexg.ttf`）を M5GFX がビットマップ形式に変換したもの | IPA フォントライセンス v1.0 | [IPA_Font_License_Agreement_v1.0.txt](licenses/IPA_Font_License_Agreement_v1.0.txt) |
| `FreeSansBold9pt7b` / `FreeSansBold12pt7b` / `FreeSansBold18pt7b` | 英字の見出し・ラベル | GNU FreeFont を Adafruit GFX 形式に変換したもの（Copyright (c) 2012 Adafruit Industries） | BSD License | [Adafruit-GFX-Fonts-LICENSE.txt](licenses/Adafruit-GFX-Fonts-LICENSE.txt) |

`lgfxJapanGothicP_24` は IPA フォントライセンス v1.0 の「派生プログラム」にあたり、同ライセンスの条件の下で再配布しています。

- 派生プログラム（変換済みのフォントデータ）は M5GFX のソース
  [`src/lgfx/Fonts/IPA/`](https://github.com/m5stack/M5GFX/tree/0.2.28/src/lgfx/Fonts/IPA) で公開されており、変換手順も同じ場所の README にあります。
- オリジナルのフォントは [IPA フォントの配布ページ](https://moji.or.jp/ipafont/) から入手できます。
- このファームウェアは、フォントを画面表示のためにだけ使っています。

## このリポジトリに含まれるコピー

- [`tools/upload_guard.py`](tools/upload_guard.py): M5StopWatch-MultiFirm v1.1.0 の `examples/upload_guard.py` を、
  著作権表示を先頭に加えた以外は変更せずにコピーしたものです（MIT、Copyright (c) 2026 wararyo）。
