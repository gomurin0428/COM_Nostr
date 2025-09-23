# Troubleshooting

- NostrClient は 1 度しか Initialize できません。2 回目以降は `E_NOSTR_ALREADY_INITIALIZED` が返るため、新しいインスタンスを生成してください。`Dispose()` 後に任意の API を呼び出すと `E_NOSTR_OBJECT_DISPOSED` が返るので、再利用せず破棄してください。
- GUI 以外のホスト (PowerShell やサービス) で `SynchronizationContext` が捕捉できない場合、内部 STA スレッド上でコールバックが直列実行されます。コールバック内で長時間ブロックすると他の通知が詰まるため、必要なら `Task.Run` 等でオフロードしてください。
- QueueOverflowStrategy を Throw に設定した状態で購読コールバックが長時間ブロックすると、Subscription queue overflow. で CLOSED になる。MaxQueueLength を十分に確保するか、DropOldest に切り替えてイベントを間引いてください。
- Docker 上の strfry リレーを再起動するテスト (RestartAsync) を実行する際は、既存コンテナとポート競合しないことを確認してください。停止済みでも --rm オプションが動作しない環境では手動で docker stop が必要です。
- RestartAsync は毎回コンテナ名を再生成するため、テストが異常終了した場合は docker ps -a で孤立した strfry-test-* を停止・削除してから再実行してください。
- `COM_Nostr_Native` の MIDL が `MIDL2025: ... near "IDispatch"` で停止する場合は、IDL 内の各 `coclass` で `[default] interface IDispatch;` のように `interface` キーワード付きで既定インターフェイスを宣言しているか確認し、修正後に `msbuild COM_Nostr_Native.vcxproj /t:Clean;Build /p:Configuration=Debug /p:Platform=x64` を実行する。
- `COM_Nostr_NativePS` ビルドで "MIDL will not generate DLLDATA.C" が出た場合は、既存の生成済み `*_p.c`/`dlldata.c` を探す代わりに `Stub.cpp` が配置されているかを確認し、`msbuild COM_Nostr_NativePS.vcxproj /t:Clean;Build /p:Configuration=Debug /p:Platform=x64` を実行してスタブ DLL を再生成する。

- Setup_COM_Nostr で生成した MSI を非管理者セッションで実行すると `DllRegisterServer failed with error 0x80004005` などの自己登録失敗が発生する。管理者権限で実行し、事前に .NET 8 Desktop Runtime (x64) をインストールしてください。

- PowerShell 7 (pwsh) は .NET 9 系ランタイムを事前読み込みするため、`New-Object -ComObject COM_Nostr.NostrClient` で 0x800080A5 (CO_E_CLRNOTAVAILABLE) が発生することがある。その場合は Windows PowerShell 5.1 (`powershell.exe`) など .NET Framework ホストか、Excel/VBA 等のネイティブクライアント経由で参照する。
- `tlbexp.exe` (.NET Framework 4.x) は net8.0-windows 向けにビルドした `COM_Nostr.dll` を読み込む際に GAC から `System.Runtime, Version=8.0.0.0` を解決できず TX0000 エラーで停止する。公式ドキュメントにもあるとおり、.NET Core/.NET 5+ ではアセンブリから TLB を生成する機能が提供されないため、TLB が必要な場合は MIDL で IDL から生成するか、既存の TLB を用意して `<ComHostTypeLibrary>` で埋め込む運用に切り替える。

- 32bit PowerShell (例: `C:\\Windows\\SysWOW64\\WindowsPowerShell\\v1.0\\powershell.exe`) から `COM_Nostr.comhost.dll` を登録すると WOW6432Node 側にのみ CLSID が書き込まれ、64bit クライアント (Excel 64bit 等) では `REGDB_E_CLASSNOTREG` となる。登録・動作確認はいずれも 64bit ホストで行うこと。

- CLSID `{7d3091fe-ca18-49ba-835c-012991076660}` を直接指定する場合も、生成元プロセスが 64bit であることを確認する。PowerShell 5.1 (64bit) では `[type]::GetTypeFromCLSID(...)` を使い、取得後は `Marshal.ReleaseComObject` で確実に解放する。


