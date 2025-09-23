# Native Sequence Diagrams (2025-09-23)

## Relay 接続と購読開始 (NIP-11/NIP-15)
`NostrClient.Initialize` 後に `ConnectRelay` → `OpenSubscription` する流れを C++/ATL で再現するためのシーケンス図。WinHTTP を介したメタ取得と WebSocket 接続、EOSE ハンドリング、コールバック直列化を表している。

```mermaid
sequenceDiagram
    participant Client as COM Client
    participant NC as NostrClient
    participant NR as NostrRelaySession
    participant HTTP as NativeHttpClient
    participant WS as WinHttpWebSocket
    participant Relay as Nostr Relay

    Client->>NC: Initialize(options)
    NC->>HTTP: Build runtime options / Create factory
    Client->>NC: ConnectRelay(descriptor, authCb)
    NC->>HTTP: GET NIP-11 metadata
    HTTP-->>NC: Metadata JSON
    NC->>NR: Create session (descriptor, dispatcher)
    NR->>WS: WinHTTP WebSocket Connect
    WS-->>NR: Connected
    Client->>NC: OpenSubscription(relayUrl, filters, eventCb, options)
    NC->>NR: RegisterSubscription()
    NR->>WS: SEND REQ(subscriptionId, filters)
    Relay-->>WS: Stored EVENT batches
    WS-->>NR: EVENT(subscriptionId, event)
    NR->>NC: Queue EVENT
    NC->>ComDispatcher: Post OnEvent
    ComDispatcher-->>Client: OnEvent
    Relay-->>WS: EOSE(subscriptionId)
    WS-->>NR: EOSE(subscriptionId)
    NR->>NC: Update status to Active
    NC->>ComDispatcher: Post OnEndOfStoredEvents
    ComDispatcher-->>Client: OnEndOfStoredEvents
```

## EVENT 送信と AUTH 応答 (NIP-20/NIP-42)
署名者 (`INostrSigner`) を COM 経由で呼び出しつつ EVENT/AUTH を送信し、`OK` / `NOTICE` / `auth-required` を処理する流れ。

```mermaid
sequenceDiagram
    participant Client as COM Client
    participant NC as NostrClient
    participant NR as NostrRelaySession
    participant Signer as INostrSigner
    participant WS as WinHttpWebSocket
    participant Relay as Nostr Relay

    Client->>NC: PublishEvent(relayUrl, event)
    NC->>NR: GetSessionOrThrow
    NC->>NC: NormalizeEvent(event)
    NC->>Signer: Sign(draft)
    Signer-->>NC: sig (64-byte hex)
    NC->>NC: Compute event id (libsecp256k1)
    NC->>NR: PublishEvent(dto, timeout, retries)
    NR->>WS: SEND EVENT
    Relay-->>WS: OK(eventId, success=true, "")
    WS-->>NR: OK(...)
    NR->>NC: Update LastOkResult
    NC->>ComDispatcher: Post OnNotice if message present

    Relay-->>WS: NOTICE("auth-required: challenge")
    WS-->>NR: NOTICE(...)
    NR->>NC: Cache challenge, invoke auth callback
    NC->>ComDispatcher: Post OnAuthRequired
    ComDispatcher-->>Client: OnAuthRequired(challenge)
    Client->>NC: RespondAuth(relayUrl, authEvent)
    NC->>Signer: GetPublicKey()
    NC->>Signer: Sign(draft22242)
    Signer-->>NC: sig
    NC->>NR: Authenticate(dto, timeout, retries)
    NR->>WS: SEND AUTH
    Relay-->>WS: OK(eventId, success=true, "auth ok")
    WS-->>NR: OK(...)
    NR->>NC: Clear challenge, mark success
    NC->>ComDispatcher: Post OnAuthSucceeded
    ComDispatcher-->>Client: OnAuthSucceeded(relayUrl)
```
