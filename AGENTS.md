# SendMIDI互換・高速軽量MIDI送信ライブラリ＆CLI 仕様書（ドラフト）

**版**: 0.1.0
**日付**: 2025-10-01
**ライセンス**: MIT（ライブラリ/CLI）

---

## ゴール

- JUCEに依存しない、MITライセンスの**埋め込み用MIDI送信コア**を提供する。
- SendMIDIと**互換のCLI**を同梱（完全互換モード＋拡張モード）。
- macOS / Windows 両OSで**低レイテンシ**かつ**安定**した送信を実現する。
- ライブラリは GUI / CLI / サービス等、他プロジェクトから**直接リンク可能**。
- 将来の MIDI 2.0 / Windows MIDI Services 拡張を見据えたレイヤ設計とする。

## 非ゴール（初期版）

- ネイティブMIDI 2.0 / UMP。
- VST / AU など DAW プラグイン。
- Windows における独自仮想MIDIドライバの同梱（OS標準外のため）。

## 成果物

- **libmidix**（MIT）: 薄い C API（C++ / Swift / C# からも利用しやすい）
- **midix-cli**（MIT）: SendMIDI 互換の CLI サンプル（`sendmidi` 相当の文法をサポート）
- **bindings**: 最小の Swift / C# バインディング（ヘッダ + P/Invoke / bridging header）
- **examples / tests**: スモークテスト、レイテンシ測定、SysEx 大容量送信テスト

## サポートOSと依存

- macOS: CoreMIDI / CoreAudio（標準フレームワークのみ）
- Windows: WinMM（mmsystem）を標準実装。将来的に Windows MIDI Services への差し替え可。
- 追加依存なし（C標準ライブラリ + OS SDK のみ）。

## パフォーマンス要件

- 即時送信の平均レイテンシを**サブミリ秒〜数ミリ秒**に抑制（仮想ポート経由の実測）。
- スケジュール送信のジッタは**±1ms以内**を目標（高負荷時も破綻しない設計）。
- 連続ノート連打・CC スパム・長大 SysEx（≥256KB）に対する耐性。

## アーキテクチャ

- **core**: スレッドセーフな送信 API、エラーハンドリング、タイムスタンプスケジューラ。
- **hal**: OS 抽象化（`midix_hal_mac` と `midix_hal_win`）。
  - mac: MIDIClientCreate / MIDIOutputPortCreate / MIDISend / MIDISendSysex / MIDISourceCreate / MIDIReceived。
  - win: midiOutOpen / midiOutShortMsg / midiOutLongMsg / midiOutPrepareHeader / midiOutUnprepareHeader。
- **scheduler**: 高精度タイマ（ホストタイム / QPC）＋軽量ワーカースレッド。
  - 先読みキュー（タイムスタンプ昇順のロックフリーリング）。
  - スリープ→アクティブ待機のハイブリッド（デッドライン直前でスピン）。
- **cli**: パーサ（SendMIDI 文法互換）、状態（デバイス名 / チャンネル / 基数 / 中音Cオクターブ）。
- **util**: ノート名↔ノート番号、10進/16進、RAW HEX、.syx 入出力、ログ。

## スレッドモデル

- `midix_context` はひとつの送信ワーカースレッドを持つ（複数ポート同時対応可）。
- API 呼び出しはノンブロッキングでキューに投入、ワーカーが送信。
- SysEx は OS の非同期 API に乗せ、完了コールバックでバッファ寿命を管理。
- 終了時はワーカーの drain → join → デバイス解放。

## メモリと安全性

- SysEx 送信バッファは完了まで生存（ライブラリ側でコピー or ユーザ提供を pin）。
- 大容量 SysEx は分割とバックプレッシャを実装。
- すべての公開関数は戻り値で結果を明示（0=OK, <0=エラー）。

## エラー設計（例）

- MIDIX_ERR_NO_DEVICE: デバイス未選択 / 未発見。
- MIDIX_ERR_PORT_FAILURE: ポート作成失敗 / 切断。
- MIDIX_ERR_INVALID_MSG: MIDI メッセージ不正。
- MIDIX_ERR_SCHED_OVERFLOW: 送信キュー飽和。
- MIDIX_ERR_SYSEX_INCOMPLETE: SysEx 終端不正（F0 / F7 欠落）。
- MIDIX_ERR_TIMEOUT: 送信完了待ちタイムアウト（SysEx）。

## 公開API（C, 例）

```c
// include/midix.h (MIT)
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct midix_ctx midix_ctx;
typedef uint64_t midix_time_ns; // モノトニック時刻（ns）

midix_ctx* midix_create(const char* client_name);
void       midix_destroy(midix_ctx*);

int  midix_open_output_by_name(midix_ctx*, const char* name_substring);  // 部分一致OK
void midix_close_output(midix_ctx*);

int  midix_create_virtual_output(midix_ctx*, const char* port_name);     // macOSのみ実装（Windowsは外部ドライバ）
void midix_drop_virtual_output(midix_ctx*);

midix_time_ns midix_now();                       // モノトニック（host/QPC）
int  midix_send_bytes(midix_ctx*, const uint8_t* data, size_t len, midix_time_ns ts); // ts=0 で即時
int  midix_send_sysex(midix_ctx*, const uint8_t* data, size_t len, midix_time_ns ts);

int  midix_note_on (midix_ctx*, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);
int  midix_note_off(midix_ctx*, int ch, uint8_t note, uint8_t vel, midix_time_ns ts);
int  midix_cc     (midix_ctx*, int ch, uint8_t ccno, uint8_t val, midix_time_ns ts);
int  midix_pb     (midix_ctx*, int ch, uint16_t val14, midix_time_ns ts);
int  midix_cp     (midix_ctx*, int ch, uint8_t val, midix_time_ns ts);
int  midix_pc     (midix_ctx*, int ch, uint8_t program, midix_time_ns ts);

int  midix_panic(midix_ctx*);                    // 全CH AllNotesOff / SustainOff
int  midix_flush(midix_ctx*);                    // キューをすべて送出完了まで待つ（任意）

#ifdef __cplusplus
}
#endif
```

## CLI 仕様（SendMIDI互換）

- 実行ファイル: `midix`（必要に応じて `sendmidi` 互換エイリアスも提供）。
- 入力形態: 一括コマンド / テキストファイル / 標準入力ストリーム（常駐）。
- 対応コマンド（主なもの）:
  - `dev <name>`: 出力ポート選択（部分一致・大文字小文字無視）。
  - `virt [name]`: 仮想MIDI出力の作成（macOS）。
  - `list`: 出力ポート一覧。
  - `panic`: 全ノートオフ等。
  - `file <path>`: コマンドファイル読み込み（`#` はコメント）。
  - `dec` / `hex`: 既定基数切替（末尾 `M` / `H` で強制指定）。
  - `ch <1-16>`: 既定チャンネル。
  - `omc <number>`: 中音Cのオクターブ定義。
  - `on` / `off` / `pp` / `cc` / `cc14` / `pc` / `cp` / `pb` / `rpn` / `nrpn`。
  - リアルタイム系: `clock <bpm>` / `mc` / `start` / `stop` / `cont`。
  - システム系: `as` / `rst` / `tc` / `spp` / `ss` / `tun`。
  - SysEx: `syx <bytes...>`（F0/F7省略入力可） / `syf <.syx file>`。
  - `raw <bytes...>`: 生バイト列。
  - `--`: 標準入力から閉じるまで読み続ける常駐モード。
- 互換外の拡張（例）:
  - `@<ms>` / `+<ms>`: 相対/絶対スケジュール。
  - `--drain`: 終了前に送信完了を待つ。
  - `--strict`: エラーで即非0終了。
  - `--json`: `list` の機械可読出力。
- 文法: 空白区切り、引用符 "..."、ノート名（C#4 / Db3）と 10進/16進、M/H サフィックス。
- 標準エラー: 送信失敗・ポート切断・SysEx不正・キュー飽和を明示。`--verbose` で詳細。

## Windows 固有仕様

- 送信: midiOutShortMsg（ショート）/ midiOutLongMsg（SysEx）。
- コールバック: CALLBACK_FUNCTION で MOM_DONE を受領後に UnprepareHeader。
- 高精度時間: QueryPerformanceCounter/Frequency 基準。
- スケジューラ: CreateWaitableTimer＋微小スピン、必要に応じて timeBeginPeriod(1)（オプトイン）。
- 仮想ポート: OS標準なし。loopMIDI / virtualMIDI 等の外部ドライバがあれば接続可（同梱しない）。
- 将来拡張: Windows MIDI Services（MIDI 2.0 / UMP）への HAL 差し替えオプション。

## macOS 固有仕様

- 送信: MIDISend（ショート）/ MIDISendSysex（長文 SysEx）。
- 仮想出力: MIDISourceCreate で名前付き仮想ポートを作成し MIDIReceived で配信。
- 高精度時間: AudioGetCurrentHostTime / AudioConvertNanosToHostTime。

## テスト計画

- 列挙・接続: 物理デバイス / 仮想ポート混在で `dev` 部分一致の挙動を検証。
- レイテンシ: IAC（mac）や loopMIDI（win）＋ DAW で受信タイムスタンプ比較。
- ロバストネス: 10〜100k イベントのバースト、1MB 超 SysEx 分割送信。
- 互換試験: SendMIDI の代表コマンドを同一入力で再生し、受信ログ一致を比較。
- 連携試験: ReceiveMIDI 互換のパイプ入力（`receivemidi | midix`）でフォワード。

## ディレクトリ構成（案）

/libmidix
  include/midix.h
  src/common/scheduler.c
  src/common/util.c
  src/common/log.c
  src/mac/midix_hal_mac.mm
  src/win/midix_hal_win.c
/cli
  midix_cli.cpp
  parser/lexer.cpp
  parser/parser.cpp
  parser/ast.hpp
/bindings/swift
/bindings/csharp
/examples
/tests
/cmake
LICENSE
README.md
.github/workflows/ci.yml

## ビルドと配布

- CMake 最小要件（macOS は -framework CoreMIDI CoreAudio CoreFoundation をリンク）。
- 生成物: 静的 / 共有ライブラリ（.a/.dylib, .lib/.dll）、CLI バイナリ。
- パッケージ配布: Homebrew Tap / winget は将来対応。

## ロギングとデバッグ

- 既定は WARN 以上、MIDIX_LOG_LEVEL=DEBUG で詳細。
- 送信キュー長・遅延・ドロップ数を周期出力（診断用）。

## セキュリティと安定性

- 未検証ポインタを OS API に渡さない（長文 SysEx はコピー or 参照カウント）。
- 終了時は drain を既定、強制終了フラグで中断も可。
- 例外 / SEH は使用せず、C スタイルの戻り値で統一。

## ライセンス

- ライブラリ / CLI とも MIT。外部ドライバ（例: virtualMIDI）使用時は各ライセンスを遵守。

## 互換性と参考

- SendMIDI README とコマンド体系: https://github.com/gbevin/SendMIDI
- Apple CoreMIDI ドキュメント: https://developer.apple.com/documentation/coremidi
- Microsoft WinMM（midiOut*）: https://learn.microsoft.com/windows/win32/api/mmeapi/
- Windows MIDI Services / MIDI 2.0: https://aka.ms/midiservices
- loopMIDI（Windows 仮想MIDI）: https://www.tobias-erichsen.de/software/loopmidi.html

---

この仕様は**番号なし**で増補しやすい構成。次ステップとして、libmidix のスケルトンと midix-cli の最小実装（dev/list/on/off/cc/syx/syf/--）を起こし、CI と連動させる。
