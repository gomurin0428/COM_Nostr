# よく使うコマンド
- `msbuild COM_Nostr.sln /m /p:Configuration=Debug /p:Platform=x64` : .NET/ネイティブ含む全体ビルド。
- `msbuild COM_Nostr_Native/COM_Nostr_Native.vcxproj /p:Configuration=Debug /p:Platform=x64` : ネイティブ COM 本体のみビルド (Release も同様)。
- `msbuild tests/native/NostrNativeTests/NostrNativeTests.vcxproj /p:Configuration=Debug /p:Platform=x64` : ネイティブ MSTest のビルド。
- `vstest.console tests/native/NostrNativeTests/x64/Debug/NostrNativeTests.dll` : ネイティブ統合テスト実行 (strfry Docker コンテナを起動してから)。
- `pwsh ./build/native-deps.ps1` : libsecp256k1 を Debug/Release x64 でビルドして packages/native に展開。