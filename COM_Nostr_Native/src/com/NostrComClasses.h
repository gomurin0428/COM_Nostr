#pragma once

#include <atlbase.h>
#include <atlcom.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "resource.h"
#include "COMNostrNative_i.h"
#include "runtime/ClientRuntimeOptions.h"
#include "runtime/ComCallbackDispatcher.h"
#include "runtime/NativeClientResources.h"
#include "NostrJsonSerializer.h"

namespace com::nostr::native
{
class CNostrSubscription;

struct RelaySessionData
{
    struct SubscriptionOptionsData
    {
        bool keepAlive = true;
        std::optional<double> autoRequeryWindowSeconds;
        std::optional<uint32_t> maxQueueLength;
        QueueOverflowStrategy overflowStrategy = QueueOverflowStrategy_DropOldest;
    };

    struct SubscriptionEntry
    {
        std::wstring id;
        ATL::CComPtr<INostrEventCallback> callback;
        SubscriptionStatus status = SubscriptionStatus_Pending;
        CNostrSubscription* owner = nullptr;
        std::vector<NostrJsonSerializer::FilterData> filters;
        SubscriptionOptionsData options;
        std::vector<ATL::CComPtr<IDispatch>> originalFilters;
    };

    RelaySessionData() = default;
    RelaySessionData(const RelaySessionData&) = delete;
    RelaySessionData& operator=(const RelaySessionData&) = delete;

    std::wstring url;
    bool readEnabled = true;
    bool writeEnabled = true;
    bool preferred = false;
    std::wstring metadataJson;
    std::vector<int> supportedNips;

    std::unique_ptr<INativeWebSocket> webSocket;
    ATL::CComPtr<INostrAuthCallback> authCallback;

    ComCallbackDispatcher* dispatcher = nullptr;
    std::shared_ptr<NostrJsonSerializer> serializer;
    ClientRuntimeOptions runtimeOptions;

    std::atomic<bool> stopRequested{ false };
    std::thread receiveThread;

    std::mutex subscriptionMutex;
    std::unordered_map<std::wstring, std::shared_ptr<SubscriptionEntry>> subscriptions;

    std::mutex lastOkMutex;
    bool hasLastOk = false;
    bool lastOkSuccess = false;
    std::wstring lastOkEventId;
    std::wstring lastOkMessageText;

    std::atomic<RelaySessionState> state{ RelaySessionState_Disconnected };
};

class ATL_NO_VTABLE CNostrClient
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , public ATL::CComCoClass<CNostrClient, &CLSID_NostrClient>
    , public ATL::IDispatchImpl<INostrClient, &IID_INostrClient, &LIBID_COMNostrNativeLib, 1, 0>
{
public:
    CNostrClient() = default;

    DECLARE_REGISTRY_RESOURCEID(IDR_COMNOSTRNATIVE)
    DECLARE_NOT_AGGREGATABLE(CNostrClient)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CNostrClient)
        COM_INTERFACE_ENTRY(INostrClient)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    HRESULT FinalConstruct() noexcept;
    void FinalRelease() noexcept;

    // INostrClient
    STDMETHOD(Initialize)(IDispatch* options) override;
    STDMETHOD(SetSigner)(INostrSigner* signer) override;
    STDMETHOD(ConnectRelay)(IDispatch* descriptor, INostrAuthCallback* authCallback, INostrRelaySession** session) override;
    STDMETHOD(DisconnectRelay)(BSTR relayUrl) override;
    STDMETHOD(HasRelay)(BSTR relayUrl, VARIANT_BOOL* hasRelay) override;
    STDMETHOD(OpenSubscription)(BSTR relayUrl, SAFEARRAY* filters, INostrEventCallback* callback, IDispatch* options, INostrSubscription** subscription) override;
    STDMETHOD(PublishEvent)(BSTR relayUrl, IDispatch* eventPayload) override;
    STDMETHOD(RespondAuth)(BSTR relayUrl, IDispatch* authEvent) override;
    STDMETHOD(RefreshRelayInfo)(BSTR relayUrl) override;
    STDMETHOD(ListRelays)(SAFEARRAY** relayUrls) override;

private:
    HRESULT NormalizeClientOptions(IDispatch* optionsDispatch,
                                   ClientRuntimeOptions& optionsOut,
                                   std::wstring& webSocketProgId) const;

    mutable ATL::CComAutoCriticalSection stateLock_;
    bool initialized_ = false;
    bool disposed_ = false;
    ClientRuntimeOptions options_{};
    std::wstring webSocketFactoryProgId_;
    ATL::CComPtr<INostrSigner> signer_;
    std::unique_ptr<NativeClientResources> resources_;
    std::shared_ptr<NostrJsonSerializer> serializer_;
    std::unique_ptr<ComCallbackDispatcher> dispatcher_;
    std::unordered_map<std::wstring, std::shared_ptr<RelaySessionData>> relaySessions_;
};

class ATL_NO_VTABLE CNostrRelaySession
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , public ATL::CComCoClass<CNostrRelaySession, &CLSID_NostrRelaySession>
    , public ATL::IDispatchImpl<INostrRelaySession, &IID_INostrRelaySession, &LIBID_COMNostrNativeLib, 1, 0>
{
public:
    CNostrRelaySession() = default;

    DECLARE_REGISTRY_RESOURCEID(IDR_COMNOSTRNATIVE)
    DECLARE_NOT_AGGREGATABLE(CNostrRelaySession)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CNostrRelaySession)
        COM_INTERFACE_ENTRY(INostrRelaySession)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    HRESULT Initialize(std::shared_ptr<RelaySessionData> state);
    HRESULT FinalConstruct() noexcept;
    void FinalRelease() noexcept;

    friend class CNostrClient;

    // INostrRelaySession
    STDMETHOD(get_Url)(BSTR* value) override;
    STDMETHOD(get_State)(RelaySessionState* value) override;
    STDMETHOD(get_LastOkResult)(IDispatch** value) override;
    STDMETHOD(get_SupportedNips)(SAFEARRAY** value) override;
    STDMETHOD(get_WriteEnabled)(VARIANT_BOOL* value) override;
    STDMETHOD(get_ReadEnabled)(VARIANT_BOOL* value) override;
    STDMETHOD(Reconnect)() override;
    STDMETHOD(Close)() override;
    STDMETHOD(GetDescriptor)(IDispatch** value) override;
    STDMETHOD(UpdatePolicy)(IDispatch* descriptor) override;

private:
    std::weak_ptr<RelaySessionData> state_;
};
class ATL_NO_VTABLE CNostrSubscription
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , public ATL::CComCoClass<CNostrSubscription, &CLSID_NostrSubscription>
    , public ATL::IDispatchImpl<INostrSubscription, &IID_INostrSubscription, &LIBID_COMNostrNativeLib, 1, 0>
{
public:
    CNostrSubscription() = default;

    DECLARE_REGISTRY_RESOURCEID(IDR_COMNOSTRNATIVE)
    DECLARE_NOT_AGGREGATABLE(CNostrSubscription)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CNostrSubscription)
        COM_INTERFACE_ENTRY(INostrSubscription)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    HRESULT FinalConstruct() noexcept;
    void FinalRelease() noexcept;

    HRESULT Initialize(std::shared_ptr<RelaySessionData> state,
                       std::shared_ptr<RelaySessionData::SubscriptionEntry> entry);

    // INostrSubscription
    STDMETHOD(get_Id)(BSTR* value) override;
    STDMETHOD(get_Status)(SubscriptionStatus* value) override;
    STDMETHOD(get_Filters)(SAFEARRAY** value) override;
    STDMETHOD(UpdateFilters)(SAFEARRAY* filters) override;
    STDMETHOD(Close)() override;

private:
    std::weak_ptr<RelaySessionData> state_;
    std::weak_ptr<RelaySessionData::SubscriptionEntry> entry_;
};
class ATL_NO_VTABLE CNostrSigner
    : public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>
    , public ATL::CComCoClass<CNostrSigner, &CLSID_NostrSigner>
    , public ATL::IDispatchImpl<INostrSigner, &DIID_INostrSigner, &LIBID_COMNostrNativeLib, 1, 0>
{
public:
    CNostrSigner() = default;

    DECLARE_REGISTRY_RESOURCEID(IDR_COMNOSTRNATIVE)
    DECLARE_NOT_AGGREGATABLE(CNostrSigner)
    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CNostrSigner)
        COM_INTERFACE_ENTRY(INostrSigner)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    HRESULT FinalConstruct() noexcept;
    void FinalRelease() noexcept;
};

} // namespace com::nostr::native
