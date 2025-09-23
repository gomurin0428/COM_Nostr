#pragma once

#include <atlbase.h>
#include <atlcom.h>

#include "ComValueHelpers.h"
#include "COMNostrNative_i.h"

#include <vector>

namespace com::nostr::native
{
    using ATL::CComBSTR;
    using ATL::CComPtr;
    using ATL::CComVariant;
    using ::VARIANT_BOOL;
    class ATL_NO_VTABLE CNostrEvent
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CNostrEvent, &CLSID_NostrEvent>
        , public ATL::IDispatchImpl<INostrEvent, &IID_INostrEvent, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CNostrEvent() noexcept;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNostrEvent)
            COM_INTERFACE_ENTRY(INostrEvent)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_Id)(BSTR* value);
        STDMETHOD(put_Id)(BSTR value);
        STDMETHOD(get_PublicKey)(BSTR* value);
        STDMETHOD(put_PublicKey)(BSTR value);
        STDMETHOD(get_CreatedAt)(DOUBLE* value);
        STDMETHOD(put_CreatedAt)(DOUBLE value);
        STDMETHOD(get_Kind)(LONG* value);
        STDMETHOD(put_Kind)(LONG value);
        STDMETHOD(get_Tags)(SAFEARRAY** value);
        STDMETHOD(put_Tags)(SAFEARRAY* value);
        STDMETHOD(get_Content)(BSTR* value);
        STDMETHOD(put_Content)(BSTR value);
        STDMETHOD(get_Signature)(BSTR* value);
        STDMETHOD(put_Signature)(BSTR value);

        const std::vector<std::vector<CComBSTR>>& GetTags() const noexcept { return tags_; }

    private:
        CComBSTR id_;
        CComBSTR publicKey_;
        DOUBLE createdAt_ = 0.0;
        LONG kind_ = 0;
        std::vector<std::vector<CComBSTR>> tags_;
        CComBSTR content_;
        CComBSTR signature_;
    };

    class ATL_NO_VTABLE CNostrTagQuery
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CNostrTagQuery, &CLSID_NostrTagQuery>
        , public ATL::IDispatchImpl<INostrTagQuery, &IID_INostrTagQuery, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CNostrTagQuery() noexcept = default;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNostrTagQuery)
            COM_INTERFACE_ENTRY(INostrTagQuery)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_Label)(BSTR* value);
        STDMETHOD(put_Label)(BSTR value);
        STDMETHOD(get_Values)(SAFEARRAY** value);
        STDMETHOD(put_Values)(SAFEARRAY* value);

        const std::vector<CComBSTR>& GetValues() const noexcept { return values_; }

    private:
        CComBSTR label_;
        std::vector<CComBSTR> values_;
    };

    class ATL_NO_VTABLE CNostrFilter
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CNostrFilter, &CLSID_NostrFilter>
        , public ATL::IDispatchImpl<INostrFilter, &IID_INostrFilter, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CNostrFilter() noexcept = default;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNostrFilter)
            COM_INTERFACE_ENTRY(INostrFilter)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_Ids)(SAFEARRAY** value);
        STDMETHOD(put_Ids)(SAFEARRAY* value);
        STDMETHOD(get_Authors)(SAFEARRAY** value);
        STDMETHOD(put_Authors)(SAFEARRAY* value);
        STDMETHOD(get_Kinds)(SAFEARRAY** value);
        STDMETHOD(put_Kinds)(SAFEARRAY* value);
        STDMETHOD(get_Tags)(SAFEARRAY** value);
        STDMETHOD(put_Tags)(SAFEARRAY* value);
        STDMETHOD(get_Since)(VARIANT* value);
        STDMETHOD(put_Since)(VARIANT value);
        STDMETHOD(get_Until)(VARIANT* value);
        STDMETHOD(put_Until)(VARIANT value);
        STDMETHOD(get_Limit)(VARIANT* value);
        STDMETHOD(put_Limit)(VARIANT value);

        const std::vector<CComPtr<IDispatch>>& GetTagsCollection() const noexcept { return tags_; }

    private:
        std::vector<CComBSTR> ids_;
        std::vector<CComBSTR> authors_;
        std::vector<long> kinds_;
        std::vector<CComPtr<IDispatch>> tags_;
        CComVariant since_;
        CComVariant until_;
        CComVariant limit_;
    };

    class ATL_NO_VTABLE CRelayDescriptor
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CRelayDescriptor, &CLSID_RelayDescriptor>
        , public ATL::IDispatchImpl<IRelayDescriptor, &IID_IRelayDescriptor, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CRelayDescriptor() noexcept;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CRelayDescriptor)
            COM_INTERFACE_ENTRY(IRelayDescriptor)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_Url)(BSTR* value);
        STDMETHOD(put_Url)(BSTR value);
        STDMETHOD(get_ReadEnabled)(VARIANT_BOOL* value);
        STDMETHOD(put_ReadEnabled)(VARIANT_BOOL value);
        STDMETHOD(get_WriteEnabled)(VARIANT_BOOL* value);
        STDMETHOD(put_WriteEnabled)(VARIANT_BOOL value);
        STDMETHOD(get_Preferred)(VARIANT_BOOL* value);
        STDMETHOD(put_Preferred)(VARIANT_BOOL value);
        STDMETHOD(get_Metadata)(VARIANT* value);
        STDMETHOD(put_Metadata)(VARIANT value);

    private:
        CComBSTR url_;
        VARIANT_BOOL readEnabled_ = VARIANT_TRUE;
        VARIANT_BOOL writeEnabled_ = VARIANT_TRUE;
        VARIANT_BOOL preferred_ = VARIANT_FALSE;
        CComVariant metadata_;
    };

    class ATL_NO_VTABLE CSubscriptionOptions
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CSubscriptionOptions, &CLSID_SubscriptionOptions>
        , public ATL::IDispatchImpl<ISubscriptionOptions, &IID_ISubscriptionOptions, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CSubscriptionOptions() noexcept;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CSubscriptionOptions)
            COM_INTERFACE_ENTRY(ISubscriptionOptions)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_KeepAlive)(VARIANT_BOOL* value);
        STDMETHOD(put_KeepAlive)(VARIANT_BOOL value);
        STDMETHOD(get_AutoRequeryWindowSeconds)(VARIANT* value);
        STDMETHOD(put_AutoRequeryWindowSeconds)(VARIANT value);
        STDMETHOD(get_MaxQueueLength)(VARIANT* value);
        STDMETHOD(put_MaxQueueLength)(VARIANT value);
        STDMETHOD(get_QueueOverflowStrategy)(VARIANT* value);
        STDMETHOD(put_QueueOverflowStrategy)(VARIANT value);

    private:
        VARIANT_BOOL keepAlive_ = VARIANT_TRUE;
        CComVariant autoRequeryWindowSeconds_;
        CComVariant maxQueueLength_;
        CComVariant queueOverflowStrategy_;
    };

    class ATL_NO_VTABLE CAuthChallenge
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CAuthChallenge, &CLSID_AuthChallenge>
        , public ATL::IDispatchImpl<IAuthChallenge, &IID_IAuthChallenge, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CAuthChallenge() noexcept = default;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CAuthChallenge)
            COM_INTERFACE_ENTRY(IAuthChallenge)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_RelayUrl)(BSTR* value);
        STDMETHOD(put_RelayUrl)(BSTR value);
        STDMETHOD(get_Challenge)(BSTR* value);
        STDMETHOD(put_Challenge)(BSTR value);
        STDMETHOD(get_ExpiresAt)(VARIANT* value);
        STDMETHOD(put_ExpiresAt)(VARIANT value);

    private:
        CComBSTR relayUrl_;
        CComBSTR challenge_;
        CComVariant expiresAt_;
    };

    class ATL_NO_VTABLE CNostrEventDraft
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CNostrEventDraft, &CLSID_NostrEventDraft>
        , public ATL::IDispatchImpl<INostrEventDraft, &IID_INostrEventDraft, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CNostrEventDraft() noexcept;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNostrEventDraft)
            COM_INTERFACE_ENTRY(INostrEventDraft)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_PublicKey)(BSTR* value);
        STDMETHOD(put_PublicKey)(BSTR value);
        STDMETHOD(get_CreatedAt)(DOUBLE* value);
        STDMETHOD(put_CreatedAt)(DOUBLE value);
        STDMETHOD(get_Kind)(LONG* value);
        STDMETHOD(put_Kind)(LONG value);
        STDMETHOD(get_Tags)(SAFEARRAY** value);
        STDMETHOD(put_Tags)(SAFEARRAY* value);
        STDMETHOD(get_Content)(BSTR* value);
        STDMETHOD(put_Content)(BSTR value);

    private:
        CComBSTR publicKey_;
        DOUBLE createdAt_ = 0.0;
        LONG kind_ = 0;
        std::vector<std::vector<CComBSTR>> tags_;
        CComBSTR content_;
    };

    class ATL_NO_VTABLE CClientOptions
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CClientOptions, &CLSID_ClientOptions>
        , public ATL::IDispatchImpl<IClientOptions, &IID_IClientOptions, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CClientOptions() noexcept = default;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CClientOptions)
            COM_INTERFACE_ENTRY(IClientOptions)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_WebSocketFactoryProgId)(BSTR* value);
        STDMETHOD(put_WebSocketFactoryProgId)(BSTR value);
        STDMETHOD(get_UserAgent)(BSTR* value);
        STDMETHOD(put_UserAgent)(BSTR value);
        STDMETHOD(get_ConnectTimeoutSeconds)(VARIANT* value);
        STDMETHOD(put_ConnectTimeoutSeconds)(VARIANT value);
        STDMETHOD(get_SendTimeoutSeconds)(VARIANT* value);
        STDMETHOD(put_SendTimeoutSeconds)(VARIANT value);
        STDMETHOD(get_ReceiveTimeoutSeconds)(VARIANT* value);
        STDMETHOD(put_ReceiveTimeoutSeconds)(VARIANT value);

    private:
        CComBSTR webSocketFactoryProgId_;
        CComBSTR userAgent_;
        CComVariant connectTimeoutSeconds_;
        CComVariant sendTimeoutSeconds_;
        CComVariant receiveTimeoutSeconds_;
    };

    class ATL_NO_VTABLE CNostrOkResult
        : public ATL::CComObjectRootEx<ATL::CComSingleThreadModel>
        , public ATL::CComCoClass<CNostrOkResult, &CLSID_NostrOkResult>
        , public ATL::IDispatchImpl<INostrOkResult, &IID_INostrOkResult, &LIBID_COMNostrNativeLib, 1, 0>
    {
    public:
        CNostrOkResult() noexcept = default;

        DECLARE_NO_REGISTRY()
        DECLARE_PROTECT_FINAL_CONSTRUCT()

        BEGIN_COM_MAP(CNostrOkResult)
            COM_INTERFACE_ENTRY(INostrOkResult)
            COM_INTERFACE_ENTRY(IDispatch)
        END_COM_MAP()

        HRESULT FinalConstruct() noexcept;
        void FinalRelease() noexcept;

        STDMETHOD(get_Success)(VARIANT_BOOL* value);
        STDMETHOD(put_Success)(VARIANT_BOOL value);
        STDMETHOD(get_EventId)(BSTR* value);
        STDMETHOD(put_EventId)(BSTR value);
        STDMETHOD(get_Message)(BSTR* value);
        STDMETHOD(put_Message)(BSTR value);

    private:
        VARIANT_BOOL success_ = VARIANT_FALSE;
        CComBSTR eventId_;
        CComBSTR message_;
    };
}
