# プロジェクト概要
- COM_Nostr は Windows クライアントから Nostr リレーと対話する COM コンポーネント (.NET 8 実装) と、その C++/ATL ネイティブ移植 `COM_Nostr_Native` を含むソリューション。COM_Nostrは廃止予定。
- COM インターフェイス契約 (INostrClient/INostrRelaySession/INostrSubscription/INostrEventCallback/INostrAuthCallback/INostrSigner) や HRESULT は README.md と COM_Nostr_Native/COMNostrNative.idl で共有され、NIP-01/11/15/20/42/65 を最小構成で実装。
- ランタイムは WinHTTP (HTTP/WebSocket) と libsecp256k1、nlohmann::json を使用し、strfry リレーを Docker で起動する統合テストで検証する。