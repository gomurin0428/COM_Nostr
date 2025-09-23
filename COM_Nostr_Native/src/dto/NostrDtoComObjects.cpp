#include "pch.h"

#include "NostrDtoComObjects.h"

#include <utility>

namespace com::nostr::native
{
    using ATL::CComBSTR;
    using ATL::CComPtr;
    using ATL::CComVariant;
    using ::VARIANT_BOOL;

    namespace
    {
        HRESULT CopyVariantToOutput(const CComVariant& source, VARIANT* destination)
        {
            if (!destination)
            {
                return E_POINTER;
            }

            return CopyVariant(source, destination);
        }

        HRESULT AssignVariant(CComVariant& target, const VARIANT& value)
        {
            return CopyVariant(value, &target);
        }

        HRESULT ConvertDispatchArrayToVector(SAFEARRAY* value, std::vector<CComPtr<IDispatch>>& target)
        {
            target.clear();
            if (!value)
            {
                return S_OK;
            }

            LONG lower = 0;
            LONG upper = -1;
            HRESULT hr = SafeArrayGetLBound(value, 1, &lower);
            if (FAILED(hr))
            {
                return hr;
            }

            hr = SafeArrayGetUBound(value, 1, &upper);
            if (FAILED(hr))
            {
                return hr;
            }

            if (upper < lower)
            {
                return S_OK;
            }

            LONG count = upper - lower + 1;
            target.reserve(static_cast<size_t>(count));

            for (LONG offset = 0; offset < count; ++offset)
            {
                LONG index = lower + offset;
                IDispatch* element = nullptr;
                hr = SafeArrayGetElement(value, &index, &element);
                if (FAILED(hr))
                {
                    return hr;
                }

                target.emplace_back();
                target.back().Attach(element);
            }

            return S_OK;
        }

        HRESULT CreateDispatchSafeArray(const std::vector<CComPtr<IDispatch>>& source, SAFEARRAY** result)
        {
            if (!result)
            {
                return E_POINTER;
            }

            *result = nullptr;
            SAFEARRAYBOUND bound{};
            bound.lLbound = 0;
            bound.cElements = static_cast<ULONG>(source.size());

            SAFEARRAY* array = SafeArrayCreate(VT_DISPATCH, 1, &bound);
            if (!array)
            {
                return E_OUTOFMEMORY;
            }

            HRESULT hr = S_OK;
            for (LONG index = 0; index < static_cast<LONG>(source.size()); ++index)
            {
                IDispatch* pointer = source[static_cast<size_t>(index)];
                hr = SafeArrayPutElement(array, &index, pointer);
                if (FAILED(hr))
                {
                    SafeArrayDestroy(array);
                    return hr;
                }
            }

            *result = array;
            return S_OK;
        }
    }

    CNostrEvent::CNostrEvent() noexcept = default;

    HRESULT CNostrEvent::FinalConstruct() noexcept
    {
        return S_OK;
    }

    void CNostrEvent::FinalRelease() noexcept
    {
        tags_.clear();
    }

    STDMETHODIMP CNostrEvent::get_Id(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return id_.CopyTo(value);
    }

    STDMETHODIMP CNostrEvent::put_Id(BSTR value)
    {
        id_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_PublicKey(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return publicKey_.CopyTo(value);
    }

    STDMETHODIMP CNostrEvent::put_PublicKey(BSTR value)
    {
        publicKey_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_CreatedAt(DOUBLE* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = createdAt_;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::put_CreatedAt(DOUBLE value)
    {
        createdAt_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_Kind(LONG* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = kind_;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::put_Kind(LONG value)
    {
        kind_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_Tags(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromTagMatrix(tags_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::put_Tags(SAFEARRAY* value)
    {
        std::vector<std::vector<CComBSTR>> parsed;
        const HRESULT hr = SafeArrayToTagMatrix(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        tags_ = std::move(parsed);
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_Content(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return content_.CopyTo(value);
    }

    STDMETHODIMP CNostrEvent::put_Content(BSTR value)
    {
        content_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEvent::get_Signature(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return signature_.CopyTo(value);
    }

    STDMETHODIMP CNostrEvent::put_Signature(BSTR value)
    {
        signature_ = value;
        return S_OK;
    }

    HRESULT CNostrTagQuery::FinalConstruct() noexcept
    {
        return S_OK;
    }

    void CNostrTagQuery::FinalRelease() noexcept
    {
        values_.clear();
    }

    STDMETHODIMP CNostrTagQuery::get_Label(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return label_.CopyTo(value);
    }

    STDMETHODIMP CNostrTagQuery::put_Label(BSTR value)
    {
        label_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrTagQuery::get_Values(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromStrings(values_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrTagQuery::put_Values(SAFEARRAY* value)
    {
        std::vector<CComBSTR> parsed;
        const HRESULT hr = SafeArrayToStringVector(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        values_ = std::move(parsed);
        return S_OK;
    }

    HRESULT CNostrFilter::FinalConstruct() noexcept
    {
        since_.vt = VT_EMPTY;
        until_.vt = VT_EMPTY;
        limit_.vt = VT_EMPTY;
        return S_OK;
    }

    void CNostrFilter::FinalRelease() noexcept
    {
        ids_.clear();
        authors_.clear();
        kinds_.clear();
        tags_.clear();
        since_.Clear();
        until_.Clear();
        limit_.Clear();
    }

    STDMETHODIMP CNostrFilter::get_Ids(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromStrings(ids_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::put_Ids(SAFEARRAY* value)
    {
        std::vector<CComBSTR> parsed;
        const HRESULT hr = SafeArrayToStringVector(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        ids_ = std::move(parsed);
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::get_Authors(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromStrings(authors_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::put_Authors(SAFEARRAY* value)
    {
        std::vector<CComBSTR> parsed;
        const HRESULT hr = SafeArrayToStringVector(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        authors_ = std::move(parsed);
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::get_Kinds(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromLongs(kinds_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::put_Kinds(SAFEARRAY* value)
    {
        std::vector<long> parsed;
        const HRESULT hr = SafeArrayToLongVector(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        kinds_ = std::move(parsed);
        return S_OK;
    }

    STDMETHODIMP CNostrFilter::get_Tags(SAFEARRAY** value)
    {
        return CreateDispatchSafeArray(tags_, value);
    }

    STDMETHODIMP CNostrFilter::put_Tags(SAFEARRAY* value)
    {
        return ConvertDispatchArrayToVector(value, tags_);
    }

    STDMETHODIMP CNostrFilter::get_Since(VARIANT* value)
    {
        return CopyVariantToOutput(since_, value);
    }

    STDMETHODIMP CNostrFilter::put_Since(VARIANT value)
    {
        return AssignVariant(since_, value);
    }

    STDMETHODIMP CNostrFilter::get_Until(VARIANT* value)
    {
        return CopyVariantToOutput(until_, value);
    }

    STDMETHODIMP CNostrFilter::put_Until(VARIANT value)
    {
        return AssignVariant(until_, value);
    }

    STDMETHODIMP CNostrFilter::get_Limit(VARIANT* value)
    {
        return CopyVariantToOutput(limit_, value);
    }

    STDMETHODIMP CNostrFilter::put_Limit(VARIANT value)
    {
        return AssignVariant(limit_, value);
    }

    CRelayDescriptor::CRelayDescriptor() noexcept = default;

    HRESULT CRelayDescriptor::FinalConstruct() noexcept
    {
        metadata_.vt = VT_EMPTY;
        return S_OK;
    }

    void CRelayDescriptor::FinalRelease() noexcept
    {
        metadata_.Clear();
    }

    STDMETHODIMP CRelayDescriptor::get_Url(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return url_.CopyTo(value);
    }

    STDMETHODIMP CRelayDescriptor::put_Url(BSTR value)
    {
        url_ = value;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::get_ReadEnabled(VARIANT_BOOL* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = readEnabled_;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::put_ReadEnabled(VARIANT_BOOL value)
    {
        readEnabled_ = value == VARIANT_FALSE ? VARIANT_FALSE : VARIANT_TRUE;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::get_WriteEnabled(VARIANT_BOOL* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = writeEnabled_;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::put_WriteEnabled(VARIANT_BOOL value)
    {
        writeEnabled_ = value == VARIANT_FALSE ? VARIANT_FALSE : VARIANT_TRUE;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::get_Preferred(VARIANT_BOOL* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = preferred_;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::put_Preferred(VARIANT_BOOL value)
    {
        preferred_ = value == VARIANT_FALSE ? VARIANT_FALSE : VARIANT_TRUE;
        return S_OK;
    }

    STDMETHODIMP CRelayDescriptor::get_Metadata(VARIANT* value)
    {
        return CopyVariantToOutput(metadata_, value);
    }

    STDMETHODIMP CRelayDescriptor::put_Metadata(VARIANT value)
    {
        return AssignVariant(metadata_, value);
    }

    CSubscriptionOptions::CSubscriptionOptions() noexcept = default;

    HRESULT CSubscriptionOptions::FinalConstruct() noexcept
    {
        autoRequeryWindowSeconds_.vt = VT_EMPTY;
        maxQueueLength_.vt = VT_EMPTY;
        queueOverflowStrategy_.vt = VT_EMPTY;
        return S_OK;
    }

    void CSubscriptionOptions::FinalRelease() noexcept
    {
        autoRequeryWindowSeconds_.Clear();
        maxQueueLength_.Clear();
        queueOverflowStrategy_.Clear();
    }

    STDMETHODIMP CSubscriptionOptions::get_KeepAlive(VARIANT_BOOL* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = keepAlive_;
        return S_OK;
    }

    STDMETHODIMP CSubscriptionOptions::put_KeepAlive(VARIANT_BOOL value)
    {
        keepAlive_ = value == VARIANT_FALSE ? VARIANT_FALSE : VARIANT_TRUE;
        return S_OK;
    }

    STDMETHODIMP CSubscriptionOptions::get_AutoRequeryWindowSeconds(VARIANT* value)
    {
        return CopyVariantToOutput(autoRequeryWindowSeconds_, value);
    }

    STDMETHODIMP CSubscriptionOptions::put_AutoRequeryWindowSeconds(VARIANT value)
    {
        return AssignVariant(autoRequeryWindowSeconds_, value);
    }

    STDMETHODIMP CSubscriptionOptions::get_MaxQueueLength(VARIANT* value)
    {
        return CopyVariantToOutput(maxQueueLength_, value);
    }

    STDMETHODIMP CSubscriptionOptions::put_MaxQueueLength(VARIANT value)
    {
        return AssignVariant(maxQueueLength_, value);
    }

    STDMETHODIMP CSubscriptionOptions::get_QueueOverflowStrategy(VARIANT* value)
    {
        return CopyVariantToOutput(queueOverflowStrategy_, value);
    }

    STDMETHODIMP CSubscriptionOptions::put_QueueOverflowStrategy(VARIANT value)
    {
        return AssignVariant(queueOverflowStrategy_, value);
    }

    HRESULT CAuthChallenge::FinalConstruct() noexcept
    {
        expiresAt_.vt = VT_EMPTY;
        return S_OK;
    }

    void CAuthChallenge::FinalRelease() noexcept
    {
        expiresAt_.Clear();
    }

    STDMETHODIMP CAuthChallenge::get_RelayUrl(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return relayUrl_.CopyTo(value);
    }

    STDMETHODIMP CAuthChallenge::put_RelayUrl(BSTR value)
    {
        relayUrl_ = value;
        return S_OK;
    }

    STDMETHODIMP CAuthChallenge::get_Challenge(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return challenge_.CopyTo(value);
    }

    STDMETHODIMP CAuthChallenge::put_Challenge(BSTR value)
    {
        challenge_ = value;
        return S_OK;
    }

    STDMETHODIMP CAuthChallenge::get_ExpiresAt(VARIANT* value)
    {
        return CopyVariantToOutput(expiresAt_, value);
    }

    STDMETHODIMP CAuthChallenge::put_ExpiresAt(VARIANT value)
    {
        return AssignVariant(expiresAt_, value);
    }

    CNostrEventDraft::CNostrEventDraft() noexcept = default;

    HRESULT CNostrEventDraft::FinalConstruct() noexcept
    {
        return S_OK;
    }

    void CNostrEventDraft::FinalRelease() noexcept
    {
        tags_.clear();
    }

    STDMETHODIMP CNostrEventDraft::get_PublicKey(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return publicKey_.CopyTo(value);
    }

    STDMETHODIMP CNostrEventDraft::put_PublicKey(BSTR value)
    {
        publicKey_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::get_CreatedAt(DOUBLE* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = createdAt_;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::put_CreatedAt(DOUBLE value)
    {
        createdAt_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::get_Kind(LONG* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = kind_;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::put_Kind(LONG value)
    {
        kind_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::get_Tags(SAFEARRAY** value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        SAFEARRAY* array = nullptr;
        const HRESULT hr = CreateSafeArrayFromTagMatrix(tags_, &array);
        if (FAILED(hr))
        {
            return hr;
        }

        *value = array;
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::put_Tags(SAFEARRAY* value)
    {
        std::vector<std::vector<CComBSTR>> parsed;
        const HRESULT hr = SafeArrayToTagMatrix(value, parsed);
        if (FAILED(hr))
        {
            return hr;
        }

        tags_ = std::move(parsed);
        return S_OK;
    }

    STDMETHODIMP CNostrEventDraft::get_Content(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return content_.CopyTo(value);
    }

    STDMETHODIMP CNostrEventDraft::put_Content(BSTR value)
    {
        content_ = value;
        return S_OK;
    }

    HRESULT CClientOptions::FinalConstruct() noexcept
    {
        connectTimeoutSeconds_.vt = VT_EMPTY;
        sendTimeoutSeconds_.vt = VT_EMPTY;
        receiveTimeoutSeconds_.vt = VT_EMPTY;
        return S_OK;
    }

    void CClientOptions::FinalRelease() noexcept
    {
        connectTimeoutSeconds_.Clear();
        sendTimeoutSeconds_.Clear();
        receiveTimeoutSeconds_.Clear();
    }

    STDMETHODIMP CClientOptions::get_WebSocketFactoryProgId(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return webSocketFactoryProgId_.CopyTo(value);
    }

    STDMETHODIMP CClientOptions::put_WebSocketFactoryProgId(BSTR value)
    {
        webSocketFactoryProgId_ = value;
        return S_OK;
    }

    STDMETHODIMP CClientOptions::get_UserAgent(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return userAgent_.CopyTo(value);
    }

    STDMETHODIMP CClientOptions::put_UserAgent(BSTR value)
    {
        userAgent_ = value;
        return S_OK;
    }

    STDMETHODIMP CClientOptions::get_ConnectTimeoutSeconds(VARIANT* value)
    {
        return CopyVariantToOutput(connectTimeoutSeconds_, value);
    }

    STDMETHODIMP CClientOptions::put_ConnectTimeoutSeconds(VARIANT value)
    {
        return AssignVariant(connectTimeoutSeconds_, value);
    }

    STDMETHODIMP CClientOptions::get_SendTimeoutSeconds(VARIANT* value)
    {
        return CopyVariantToOutput(sendTimeoutSeconds_, value);
    }

    STDMETHODIMP CClientOptions::put_SendTimeoutSeconds(VARIANT value)
    {
        return AssignVariant(sendTimeoutSeconds_, value);
    }

    STDMETHODIMP CClientOptions::get_ReceiveTimeoutSeconds(VARIANT* value)
    {
        return CopyVariantToOutput(receiveTimeoutSeconds_, value);
    }

    STDMETHODIMP CClientOptions::put_ReceiveTimeoutSeconds(VARIANT value)
    {
        return AssignVariant(receiveTimeoutSeconds_, value);
    }

    HRESULT CNostrOkResult::FinalConstruct() noexcept
    {
        return S_OK;
    }

    void CNostrOkResult::FinalRelease() noexcept
    {
        // no-op
    }

    STDMETHODIMP CNostrOkResult::get_Success(VARIANT_BOOL* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        *value = success_;
        return S_OK;
    }

    STDMETHODIMP CNostrOkResult::put_Success(VARIANT_BOOL value)
    {
        success_ = value == VARIANT_FALSE ? VARIANT_FALSE : VARIANT_TRUE;
        return S_OK;
    }

    STDMETHODIMP CNostrOkResult::get_EventId(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return eventId_.CopyTo(value);
    }

    STDMETHODIMP CNostrOkResult::put_EventId(BSTR value)
    {
        eventId_ = value;
        return S_OK;
    }

    STDMETHODIMP CNostrOkResult::get_Message(BSTR* value)
    {
        if (!value)
        {
            return E_POINTER;
        }

        return message_.CopyTo(value);
    }

    STDMETHODIMP CNostrOkResult::put_Message(BSTR value)
    {
        message_ = value;
        return S_OK;
    }
}
