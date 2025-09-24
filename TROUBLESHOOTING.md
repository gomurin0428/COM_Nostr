# Troubleshooting

- NostrClient は 1 度しか Initialize できません。2 回目以降は `E_NOSTR_ALREADY_INITIALIZED` が返るため、新しいインスタンスを生成してください。`Dispose()` 後に任意の API を呼び出すと `E_NOSTR_OBJECT_DISPOSED` が返るので、再利用せず破棄してください。
- GUI 以外のホスト (PowerShell やサービス) で `SynchronizationContext` が捕捉できない場合、内部 STA スレッド上でコールバックが直列実行されます。コールバック内で長時間ブロックすると他の通知が詰まるため、必要なら `Task.Run` 等でオフロードしてください。
- WinHTTP ベースの WebSocket 実装は permessage-deflate 等の拡張を受け付けず、圧縮必須のリレーでは `ERROR_WINHTTP_INVALID_SERVER_RESPONSE` を返す。Native 版では `HResults.WebSocketFailure` へ変換し、NOTICE へ「compression unsupported」などの説明を流すことで利用者が別リレーを選べるよう周知する。
- strfry など Nostr リレーへ接続する際は `Sec-WebSocket-Protocol: nostr` を必ず送信する。ヘッダーが欠落すると WinHTTP が `ERROR_WINHTTP_INVALID_SERVER_RESPONSE` でハンドシェイクを中断するため、`WinHttpWebSocket` の Connect 失敗時はヘッダー設定と docker コンテナの起動状態を確認する。
- `WinHttpWebSocketReceive` は接続維持中でも 5 秒ごとに `ERROR_WINHTTP_TIMEOUT` を返すことがあり、その際に受信ループを終了させると以降の `Receive` が常時タイムアウトする。ネイティブ実装ではタイムアウトを継続扱いにして再試行し、EOSE 待ちの間もキューとイベントを保持する。
- `tests/native/NostrNativeTests/WebSocketHandshakeTests.cpp` は `docker run --rm -d ... dockurr/strfry` を呼び出してリレーを起動し、HTTP ポーリング (`waitForHttp`) を最大 20 回リトライする。Docker Desktop のデーモンが停止している場合、各リトライで WinHTTP の接続タイムアウト (現状 5 秒) を待つため、`vstest.console` が約 1～2 分無言のまま「ハング」しているように見える。テスト前に `docker ps` が成功することを確認するか、Docker が使えない環境では該当テストを除外するフィルターを設定する。
- QueueOverflowStrategy を Throw に設定した状態で購読コールバックが長時間ブロックすると、Subscription queue overflow. で CLOSED になる。MaxQueueLength を十分に確保するか、DropOldest に切り替えてイベントを間引いてください。
- Docker 上の strfry リレーを再起動するテスト (RestartAsync) を実行する際は、既存コンテナとポート競合しないことを確認してください。停止済みでも --rm オプションが動作しない環境では手動で docker stop が必要です。
- RestartAsync は毎回コンテナ名を再生成するため、テストが異常終了した場合は docker ps -a で孤立した strfry-test-* を停止・削除してから再実行してください。
- `build/native-deps.ps1` は Visual Studio 2022 の C++ ビルドツールと CMake がインストールされていることを前提とする。PATH に `cmake` が無い場合でも標準インストール先 (`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`) を自動的に追加するが、別ディレクトリへインストールしている場合は PATH を明示的に調整する。`cmake` や `ninja` が見つからない場合は「x64 Native Tools Command Prompt for VS 2022」などの開発者コマンドプロンプトから実行するか、`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64` の出力でインストール有無を確認する。IXWebSocket のビルドは初期設定で TLS/Zlib を無効化しているため、`wss://` が必要な検証を行う場合は OpenSSL/MbedTLS を用意した上で `build/native-deps.ps1` に対応する CMake オプションを追加すること。
- Release 構成で `msbuild COM_Nostr_Native.vcxproj /p:Configuration=Release /p:Platform=x64` を実行する前に `build/native-deps.ps1 -Configuration Release` を走らせておかないと、`packages/native/ixwebsocket/x64/Release/lib/ixwebsocket.lib` が存在せず `LNK1104` で失敗する。Debug だけ生成済みの場合は Release の依存フォルダーが空のままなので、スクリプトを構成別に実行してからビルドを再試行する。
- PowerShell 7 (`pwsh`) で `pwsh -Command & { ... }` のようにコマンド引数へ直接スクリプトブロックを渡すと `ScriptBlock should only be specified as a value of the Command parameter.` というパースエラーになる。`-Command "<script>"` の形式で 1 つの文字列としてスクリプト全体を渡すか、`$script = '...'; pwsh -Command $script` として実行すれば意図どおりに動作する。
- Desktop Commander の `start_process` 経由で `pwsh` を呼び出すと内部で Windows PowerShell (`powershell.exe`) が起動し、環境によっては 0x8009001d (KEYSET_DOES_NOT_EXIST) で初期化に失敗することがある。長時間処理を走らせたい場合は `shell` コマンドで `timeout_ms` を指定するか、`start_process` で `cmd /c` を挟んでから `pwsh` を起動する。
- WinHTTP API をリンクし忘れると `LNK2019: unresolved external symbol WinHttpWebSocket*` などのリンク エラーが出る。`COM_Nostr_Native.vcxproj` / `NostrNativeTests.vcxproj` の `<AdditionalDependencies>` に `winhttp.lib` が入っているか確認する。
- `COM_Nostr_Native` の MIDL が `MIDL2025: ... near "IDispatch"` で停止する場合は、IDL 内の各 `coclass` で `[default] interface IDispatch;` のように `interface` キーワード付きで既定インターフェイスを宣言しているか確認し、修正後に `msbuild COM_Nostr_Native.vcxproj /t:Clean;Build /p:Configuration=Debug /p:Platform=x64` を実行する。
- `COM_Nostr_NativePS` ビルドで "MIDL will not generate DLLDATA.C" が出た場合は、既存の生成済み `*_p.c`/`dlldata.c` を探す代わりに `Stub.cpp` が配置されているかを確認し、`msbuild COM_Nostr_NativePS.vcxproj /t:Clean;Build /p:Configuration=Debug /p:Platform=x64` を実行してスタブ DLL を再生成する。

- Setup_COM_Nostr で生成した MSI を非管理者セッションで実行すると `DllRegisterServer failed with error 0x80004005` などの自己登録失敗が発生する。管理者権限で実行し、事前に .NET 8 Desktop Runtime (x64) をインストールしてください。

- PowerShell 7 (pwsh) は .NET 9 系ランタイムを事前読み込みするため、`New-Object -ComObject COM_Nostr.NostrClient` で 0x800080A5 (CO_E_CLRNOTAVAILABLE) が発生することがある。その場合は Windows PowerShell 5.1 (`powershell.exe`) など .NET Framework ホストか、Excel/VBA 等のネイティブクライアント経由で参照する。
- `tlbexp.exe` (.NET Framework 4.x) は net8.0-windows 向けにビルドした `COM_Nostr.dll` を読み込む際に GAC から `System.Runtime, Version=8.0.0.0` を解決できず TX0000 エラーで停止する。公式ドキュメントにもあるとおり、.NET Core/.NET 5+ ではアセンブリから TLB を生成する機能が提供されないため、TLB が必要な場合は MIDL で IDL から生成するか、既存の TLB を用意して `<ComHostTypeLibrary>` で埋め込む運用に切り替える。
- UnitTest_COM_Nostr の `COMReference` はレジストリに `COM_Nostr` の Type Library が登録されていないと `MSB3284` で失敗する。COM コンポーネントを `regsvr32 COM_Nostr.comhost.dll` で登録し、`COM_Nostr.tlb` も `regtlibv12` などで登録した状態でビルド・テストを実行するか、開発環境に手動で TLB を配置して `ResolveComReference` が参照できるようにする。

- 32bit PowerShell (例: `C:\\Windows\\SysWOW64\\WindowsPowerShell\\v1.0\\powershell.exe`) から `COM_Nostr.comhost.dll` を登録すると WOW6432Node 側にのみ CLSID が書き込まれ、64bit クライアント (Excel 64bit 等) では `REGDB_E_CLASSNOTREG` となる。登録・動作確認はいずれも 64bit ホストで行うこと。

- CLSID `{7d3091fe-ca18-49ba-835c-012991076660}` を直接指定する場合も、生成元プロセスが 64bit であることを確認する。PowerShell 5.1 (64bit) では `[type]::GetTypeFromCLSID(...)` を使い、取得後は `Marshal.ReleaseComObject` で確実に解放する。


