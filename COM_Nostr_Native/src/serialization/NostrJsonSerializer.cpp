#include "pch.h"

#include "NostrJsonSerializer.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <oleauto.h>

#ifndef LOCALE_INVARIANT
#define LOCALE_INVARIANT 0x007F
#endif

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace com::nostr::native
{
    using ATL::CComBSTR;
    using ATL::CComPtr;
    using ATL::CComVariant;
    using ATL::CComDispatchDriver;
    using ATL::CComObject;

    namespace
    {
        using json = nlohmann::json;

        std::string ToUtf8(const std::wstring& value)
        {
            return WideToUtf8(value);
        }

        std::wstring FromUtf8(const std::string& value)
        {
            return Utf8ToWide(value);
        }

        std::vector<uint8_t> ToBytes(const json& value)
        {
            const auto dumped = value.dump();
            return std::vector<uint8_t>(dumped.begin(), dumped.end());
        }

        std::optional<double> ParseWideNumeric(const std::wstring& text)
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            wchar_t* end = nullptr;
            errno = 0;
            const double value = std::wcstod(text.c_str(), &end);
            if (errno != 0 || end == text.c_str())
            {
                return std::nullopt;
            }

            if (std::isnan(value) || std::isinf(value))
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<double> ParseVariantTimeString(const std::wstring& text)
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            DATE date = 0;
            HRESULT hr = VarDateFromStr(text.c_str(), LOCALE_INVARIANT, 0, &date);
            if (FAILED(hr))
            {
                return std::nullopt;
            }

            SYSTEMTIME systemTime{};
            if (!VariantTimeToSystemTime(date, &systemTime))
            {
                return std::nullopt;
            }

            FILETIME fileTime{};
            if (!SystemTimeToFileTime(&systemTime, &fileTime))
            {
                return std::nullopt;
            }

            ULARGE_INTEGER uli{};
            uli.LowPart = fileTime.dwLowDateTime;
            uli.HighPart = fileTime.dwHighDateTime;

            constexpr ULONGLONG hundredNanosecondsPerSecond = 10000000ULL;
            constexpr ULONGLONG unixEpochOffset = 116444736000000000ULL;

            if (uli.QuadPart < unixEpochOffset)
            {
                return std::nullopt;
            }

            const long double seconds = static_cast<long double>(uli.QuadPart - unixEpochOffset) / static_cast<long double>(hundredNanosecondsPerSecond);
            return static_cast<double>(seconds);
        }

        std::optional<double> TryParseUnixTimestamp(const json& node)
        {
            if (node.is_number_float() || node.is_number_integer())
            {
                const double value = node.get<double>();
                if (std::isnan(value) || std::isinf(value))
                {
                    return std::nullopt;
                }

                return value;
            }

            if (node.is_string())
            {
                const auto utf8 = node.get<std::string>();
                const auto wide = FromUtf8(utf8);

                if (const auto numeric = ParseWideNumeric(wide))
                {
                    return numeric;
                }

                if (const auto parsedDate = ParseVariantTimeString(wide))
                {
                    return parsedDate;
                }
            }

            return std::nullopt;
        }

        std::optional<double> ReadAuthExpiration(const json& data)
        {
            static constexpr std::array<const char*, 3> candidateNames{
                "expires_at",
                "expiration",
                "expiresAt"
            };

            for (const auto* name : candidateNames)
            {
                if (data.contains(name))
                {
                    const auto candidate = TryParseUnixTimestamp(data.at(name));
                    if (candidate)
                    {
                        return candidate;
                    }
                }
            }

            return std::nullopt;
        }

        json BuildEventObject(const NostrJsonSerializer::EventData& event)
        {
            json tags = json::array();
            for (const auto& tag : event.tags)
            {
                json tagArray = json::array();
                for (const auto& entry : tag)
                {
                    tagArray.push_back(ToUtf8(entry));
                }
                tags.push_back(std::move(tagArray));
            }

            json obj = json::object();
            obj["id"] = ToUtf8(event.id);
            obj["pubkey"] = ToUtf8(event.publicKey);
            obj["created_at"] = event.createdAt;
            obj["kind"] = event.kind;
            obj["tags"] = std::move(tags);
            obj["content"] = ToUtf8(event.content);
            obj["sig"] = ToUtf8(event.signature);
            return obj;
        }

        json BuildFilterObject(const NostrJsonSerializer::FilterData& filter)
        {
            json obj = json::object();
            if (!filter.ids.empty())
            {
                json ids = json::array();
                for (const auto& value : filter.ids)
                {
                    ids.push_back(ToUtf8(value));
                }
                obj["ids"] = std::move(ids);
            }

            if (!filter.authors.empty())
            {
                json authors = json::array();
                for (const auto& value : filter.authors)
                {
                    authors.push_back(ToUtf8(value));
                }
                obj["authors"] = std::move(authors);
            }

            if (!filter.kinds.empty())
            {
                json kinds = json::array();
                for (const auto kind : filter.kinds)
                {
                    kinds.push_back(kind);
                }
                obj["kinds"] = std::move(kinds);
            }

            if (filter.since)
            {
                obj["since"] = *filter.since;
            }

            if (filter.until)
            {
                obj["until"] = *filter.until;
            }

            if (filter.limit)
            {
                obj["limit"] = *filter.limit;
            }

            if (!filter.tagQueries.empty())
            {
                for (const auto& [key, values] : filter.tagQueries)
                {
                    json tagArray = json::array();
                    for (const auto& value : values)
                    {
                        tagArray.push_back(ToUtf8(value));
                    }

                    obj[ToUtf8(key)] = std::move(tagArray);
                }
            }

            return obj;
        }

        std::wstring RequireString(const json& value, const char* property)
        {
            if (!value.contains(property))
            {
                throw std::runtime_error(std::string("JSON missing property: ") + property);
            }

            const auto& node = value.at(property);
            if (!node.is_string())
            {
                throw std::runtime_error(std::string("JSON property is not a string: ") + property);
            }

            return FromUtf8(node.get<std::string>());
        }

        double RequireNumber(const json& value, const char* property)
        {
            if (!value.contains(property))
            {
                throw std::runtime_error(std::string("JSON missing property: ") + property);
            }

            const auto& node = value.at(property);
            if (!(node.is_number_integer() || node.is_number_float()))
            {
                throw std::runtime_error(std::string("JSON property is not numeric: ") + property);
            }

            return node.get<double>();
        }

        int RequireInt(const json& value, const char* property)
        {
            const double temp = RequireNumber(value, property);
            if (temp > static_cast<double>((std::numeric_limits<int>::max)()) || temp < static_cast<double>((std::numeric_limits<int>::min)()))
            {
                throw std::runtime_error(std::string("Numeric value out of range for property: ") + property);
            }

            return static_cast<int>(temp);
        }

        std::vector<std::vector<std::wstring>> ReadTags(const json& value)
        {
            std::vector<std::vector<std::wstring>> result;
            if (!value.contains("tags"))
            {
                return result;
            }

            const auto& tagsNode = value.at("tags");
            if (!tagsNode.is_array())
            {
                throw std::runtime_error("EVENT tags must be an array.");
            }

            result.reserve(tagsNode.size());
            for (const auto& tagEntry : tagsNode)
            {
                if (!tagEntry.is_array())
                {
                    throw std::runtime_error("EVENT tag entry must be an array.");
                }

                std::vector<std::wstring> tagValues;
                tagValues.reserve(tagEntry.size());
                for (const auto& token : tagEntry)
                {
                    if (!token.is_string())
                    {
                        throw std::runtime_error("EVENT tag values must be strings.");
                    }

                    tagValues.push_back(FromUtf8(token.get<std::string>()));
                }

                result.push_back(std::move(tagValues));
            }

            return result;
        }

        NostrJsonSerializer::EventData ReadEventObject(const json& value)
        {
            if (!value.is_object())
            {
                throw std::runtime_error("EVENT body must be an object.");
            }

            NostrJsonSerializer::EventData event{};
            event.id = RequireString(value, "id");
            event.publicKey = RequireString(value, "pubkey");
            event.createdAt = RequireNumber(value, "created_at");
            event.kind = RequireInt(value, "kind");
            event.tags = ReadTags(value);
            if (value.contains("content"))
            {
                const auto& contentNode = value.at("content");
                if (!contentNode.is_string())
                {
                    throw std::runtime_error("EVENT content must be a string.");
                }

                event.content = FromUtf8(contentNode.get<std::string>());
            }
            event.signature = RequireString(value, "sig");
            return event;
        }

        std::optional<double> ReadOptionalNumber(const json& value, const char* property)
        {
            if (!value.contains(property))
            {
                return std::nullopt;
            }

            const auto& node = value.at(property);
            if (!(node.is_number_float() || node.is_number_integer()))
            {
                throw std::runtime_error(std::string("Filter property must be numeric: ") + property);
            }

            return node.get<double>();
        }

        std::optional<int> ReadOptionalInt(const json& value, const char* property)
        {
            if (!value.contains(property))
            {
                return std::nullopt;
            }

            const auto numeric = ReadOptionalNumber(value, property);
            if (!numeric)
            {
                return std::nullopt;
            }

            const double temp = *numeric;
            if (temp > static_cast<double>((std::numeric_limits<int>::max)()) || temp < static_cast<double>((std::numeric_limits<int>::min)()))
            {
                throw std::runtime_error(std::string("Numeric value out of range for property: ") + property);
            }

            return static_cast<int>(temp);
        }

        std::vector<std::wstring> ReadStringArray(const json& node)
        {
            if (!node.is_array())
            {
                throw std::runtime_error("JSON node is not an array of strings.");
            }

            std::vector<std::wstring> result;
            result.reserve(node.size());
            for (const auto& entry : node)
            {
                if (!entry.is_string())
                {
                    throw std::runtime_error("JSON array element is not a string.");
                }

                result.push_back(FromUtf8(entry.get<std::string>()));
            }

            return result;
        }

        NostrJsonSerializer::FilterData ReadFilterObject(const json& value)
        {
            if (!value.is_object())
            {
                throw std::runtime_error("Filter entry must be an object.");
            }

            NostrJsonSerializer::FilterData filter;

            if (value.contains("ids"))
            {
                filter.ids = ReadStringArray(value.at("ids"));
            }

            if (value.contains("authors"))
            {
                filter.authors = ReadStringArray(value.at("authors"));
            }

            if (value.contains("kinds"))
            {
                const auto& kindsNode = value.at("kinds");
                if (!kindsNode.is_array())
                {
                    throw std::runtime_error("Filter 'kinds' must be an array.");
                }

                for (const auto& entry : kindsNode)
                {
                    if (!(entry.is_number_integer() || entry.is_number_float()))
                    {
                        throw std::runtime_error("Filter 'kinds' entries must be numeric.");
                    }

                    const double rawKind = entry.get<double>();
                    if (rawKind > static_cast<double>(std::numeric_limits<int>::max()) || rawKind < static_cast<double>(std::numeric_limits<int>::min()))
                    {
                        throw std::runtime_error("Filter 'kinds' value out of range.");
                    }

                    filter.kinds.push_back(static_cast<int>(rawKind));
                }
            }

            filter.since = ReadOptionalNumber(value, "since");
            filter.until = ReadOptionalNumber(value, "until");
            filter.limit = ReadOptionalInt(value, "limit");

            for (auto it = value.begin(); it != value.end(); ++it)
            {
                const auto& key = it.key();
                if (!key.empty() && key[0] == '#' && it.value().is_array())
                {
                    const auto label = FromUtf8(key);
                    filter.tagQueries[label] = ReadStringArray(it.value());
                }
            }

            return filter;
        }

        json ParseJsonArray(const std::vector<uint8_t>& payload)
        {
            try
            {
                json parsed = json::parse(payload.begin(), payload.end());
                if (!parsed.is_array())
                {
                    throw std::runtime_error("JSON payload is not an array.");
                }

                return parsed;
            }
            catch (const json::parse_error& ex)
            {
                throw std::runtime_error(std::string("Failed to parse JSON payload: ") + ex.what());
            }
        }

        std::vector<std::vector<std::wstring>> ConvertTags(const std::vector<std::vector<CComBSTR>>& tags)
        {
            std::vector<std::vector<std::wstring>> result;
            result.reserve(tags.size());
            for (const auto& tag : tags)
            {
                std::vector<std::wstring> converted;
                converted.reserve(tag.size());
                for (const auto& value : tag)
                {
                    converted.push_back(BstrToWString(value));
                }
                result.push_back(std::move(converted));
            }

            return result;
        }

        std::vector<std::wstring> ConvertStrings(const std::vector<CComBSTR>& items)
        {
            std::vector<std::wstring> result;
            result.reserve(items.size());
            for (const auto& item : items)
            {
                result.push_back(BstrToWString(item));
            }

            return result;
        }
    }

    std::vector<uint8_t> NostrJsonSerializer::SerializeEvent(const EventData& event) const
    {
        json payload = json::array();
        payload.push_back("EVENT");
        payload.push_back(BuildEventObject(event));
        return ToBytes(payload);
    }

    std::vector<uint8_t> NostrJsonSerializer::SerializeAuth(const EventData& event) const
    {
        json payload = json::array();
        payload.push_back("AUTH");
        payload.push_back(BuildEventObject(event));
        return ToBytes(payload);
    }

    std::vector<uint8_t> NostrJsonSerializer::SerializeRequest(const RequestMessage& request) const
    {
        json payload = json::array();
        payload.push_back("REQ");
        payload.push_back(ToUtf8(request.subscriptionId));

        for (const auto& filter : request.filters)
        {
            payload.push_back(BuildFilterObject(filter));
        }

        return ToBytes(payload);
    }

    std::vector<uint8_t> NostrJsonSerializer::SerializeClose(const std::wstring& subscriptionId) const
    {
        json payload = json::array();
        payload.push_back("CLOSE");
        payload.push_back(ToUtf8(subscriptionId));
        return ToBytes(payload);
    }

    NostrJsonSerializer::EventMessage NostrJsonSerializer::DeserializeEvent(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 2)
        {
            throw std::runtime_error("EVENT message does not contain enough elements.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "EVENT")
        {
            throw std::runtime_error("JSON is not an EVENT message.");
        }

        EventMessage message{};
        size_t eventIndex = 1;
        if (parsed.size() >= 3)
        {
            if (!parsed[1].is_string())
            {
                throw std::runtime_error("EVENT subscription id must be a string.");
            }

            message.subscriptionId = FromUtf8(parsed[1].get<std::string>());
            eventIndex = 2;
        }

        message.event = ReadEventObject(parsed[eventIndex]);
        return message;
    }

    NostrJsonSerializer::OkMessage NostrJsonSerializer::DeserializeOk(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 3)
        {
            throw std::runtime_error("OK message missing required elements.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "OK")
        {
            throw std::runtime_error("JSON is not an OK message.");
        }

        if (!parsed[1].is_string())
        {
            throw std::runtime_error("OK message event id must be a string.");
        }

        if (!parsed[2].is_boolean())
        {
            throw std::runtime_error("OK message success flag must be boolean.");
        }

        OkMessage message{};
        message.eventId = FromUtf8(parsed[1].get<std::string>());
        message.success = parsed[2].get<bool>();
        if (parsed.size() >= 4)
        {
            if (!parsed[3].is_string())
            {
                throw std::runtime_error("OK message text must be a string.");
            }

            message.message = FromUtf8(parsed[3].get<std::string>());
        }

        return message;
    }

    NostrJsonSerializer::NoticeMessage NostrJsonSerializer::DeserializeNotice(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 2)
        {
            throw std::runtime_error("NOTICE message missing text.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "NOTICE")
        {
            throw std::runtime_error("JSON is not a NOTICE message.");
        }

        if (!parsed[1].is_string())
        {
            throw std::runtime_error("NOTICE message text must be a string.");
        }

        NoticeMessage message{};
        message.message = FromUtf8(parsed[1].get<std::string>());
        return message;
    }

    NostrJsonSerializer::AuthChallengeMessage NostrJsonSerializer::DeserializeAuthChallenge(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 2)
        {
            throw std::runtime_error("AUTH message missing payload.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "AUTH")
        {
            throw std::runtime_error("JSON is not an AUTH message.");
        }

        AuthChallengeMessage message{};
        const auto& data = parsed[1];
        if (data.is_string())
        {
            message.challenge = FromUtf8(data.get<std::string>());
        }
        else if (data.is_object())
        {
            if (data.contains("challenge") && data.at("challenge").is_string())
            {
                message.challenge = FromUtf8(data.at("challenge").get<std::string>());
            }
            else if (data.contains("message") && data.at("message").is_string())
            {
                message.challenge = FromUtf8(data.at("message").get<std::string>());
            }
            else if (data.contains("value") && data.at("value").is_string())
            {
                message.challenge = FromUtf8(data.at("value").get<std::string>());
            }
            else
            {
                throw std::runtime_error("AUTH payload object missing challenge string.");
            }

            if (const auto expires = ReadAuthExpiration(data))
            {
                message.expiresAt = *expires;
            }
        }
        else
        {
            throw std::runtime_error("AUTH payload must be a string or object.");
        }

        if (message.challenge.empty())
        {
            throw std::runtime_error("AUTH challenge must not be empty.");
        }

        return message;
    }

    NostrJsonSerializer::EndOfStoredEventsMessage NostrJsonSerializer::DeserializeEndOfStoredEvents(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 2)
        {
            throw std::runtime_error("EOSE message missing subscription id.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "EOSE")
        {
            throw std::runtime_error("JSON is not an EOSE message.");
        }

        if (!parsed[1].is_string())
        {
            throw std::runtime_error("EOSE subscription id must be a string.");
        }

        EndOfStoredEventsMessage message{};
        message.subscriptionId = FromUtf8(parsed[1].get<std::string>());
        return message;
    }

    NostrJsonSerializer::ClosedMessage NostrJsonSerializer::DeserializeClosed(const std::vector<uint8_t>& payload) const
    {
        const auto parsed = ParseJsonArray(payload);
        if (parsed.size() < 2)
        {
            throw std::runtime_error("CLOSED message missing subscription id.");
        }

        if (!parsed[0].is_string() || parsed[0].get<std::string>() != "CLOSED")
        {
            throw std::runtime_error("JSON is not a CLOSED message.");
        }

        if (!parsed[1].is_string())
        {
            throw std::runtime_error("CLOSED subscription id must be a string.");
        }

        ClosedMessage message{};
        message.subscriptionId = FromUtf8(parsed[1].get<std::string>());
        if (parsed.size() >= 3)
        {
            if (!parsed[2].is_string())
            {
                throw std::runtime_error("CLOSED reason must be a string.");
            }

            message.reason = FromUtf8(parsed[2].get<std::string>());
        }

        return message;
    }

    HRESULT NostrJsonSerializer::ReadEventFromDispatch(IDispatch* dispatch, EventData& event) const
    {
        if (!dispatch)
        {
            return E_POINTER;
        }

        CComDispatchDriver driver(dispatch);
        CComVariant value;

        HRESULT hr = driver.GetProperty(1, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }

        event.id = BstrToWString(value.bstrVal);

        value.Clear();
        hr = driver.GetProperty(2, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }
        event.publicKey = BstrToWString(value.bstrVal);

        value.Clear();
        hr = driver.GetProperty(3, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_R8);
        if (FAILED(hr))
        {
            return hr;
        }
        event.createdAt = value.dblVal;

        value.Clear();
        hr = driver.GetProperty(4, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_I4);
        if (FAILED(hr))
        {
            return hr;
        }
        event.kind = value.lVal;

        value.Clear();
        hr = driver.GetProperty(5, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        if (value.vt & VT_ARRAY)
        {
            std::vector<std::vector<CComBSTR>> tags;
            hr = SafeArrayToTagMatrix(value.parray, tags);
            if (FAILED(hr))
            {
                return hr;
            }

            event.tags = ConvertTags(tags);
        }
        else
        {
            event.tags.clear();
        }

        value.Clear();
        hr = driver.GetProperty(6, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }
        event.content = BstrToWString(value.bstrVal);

        value.Clear();
        hr = driver.GetProperty(7, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }
        event.signature = BstrToWString(value.bstrVal);

        return S_OK;
    }

    HRESULT NostrJsonSerializer::ReadEventDraftFromDispatch(IDispatch* dispatch, EventData& event) const
    {
        if (!dispatch)
        {
            return E_POINTER;
        }

        CComDispatchDriver driver(dispatch);
        CComVariant value;

        value.Clear();
        HRESULT hr = driver.GetProperty(1, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }
        event.publicKey = BstrToWString(value.bstrVal);

        value.Clear();
        hr = driver.GetProperty(2, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_R8);
        if (FAILED(hr))
        {
            return hr;
        }
        event.createdAt = value.dblVal;

        value.Clear();
        hr = driver.GetProperty(3, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_I4);
        if (FAILED(hr))
        {
            return hr;
        }
        event.kind = value.lVal;

        value.Clear();
        hr = driver.GetProperty(4, &value);
        if (FAILED(hr))
        {
            return hr;
        }

        if (value.vt & VT_ARRAY)
        {
            std::vector<std::vector<CComBSTR>> tags;
            hr = SafeArrayToTagMatrix(value.parray, tags);
            if (FAILED(hr))
            {
                return hr;
            }

            event.tags = ConvertTags(tags);
        }
        else
        {
            event.tags.clear();
        }

        value.Clear();
        hr = driver.GetProperty(5, &value);
        if (FAILED(hr))
        {
            return hr;
        }
        hr = value.ChangeType(VT_BSTR);
        if (FAILED(hr))
        {
            return hr;
        }
        event.content = BstrToWString(value.bstrVal);

        return S_OK;
    }

    HRESULT NostrJsonSerializer::ReadFiltersFromSafeArray(SAFEARRAY* filters, std::vector<FilterData>& out) const
    {
        out.clear();
        if (!filters)
        {
            return S_OK;
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = SafeArrayGetLBound(filters, 1, &lower);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = SafeArrayGetUBound(filters, 1, &upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        LONG count = upper - lower + 1;
        out.reserve(static_cast<size_t>(count));

        for (LONG offset = 0; offset < count; ++offset)
        {
            LONG index = lower + offset;
            CComVariant filterVariant;
            hr = SafeArrayGetElement(filters, &index, &filterVariant);
            if (FAILED(hr))
            {
                return hr;
            }

            if (filterVariant.vt != VT_DISPATCH || !filterVariant.pdispVal)
            {
                return DISP_E_TYPEMISMATCH;
            }

            CComDispatchDriver driver(filterVariant.pdispVal);
            FilterData filter;

            // ids
            CComVariant property;
            hr = driver.GetProperty(1, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            if (property.vt & VT_ARRAY)
            {
                std::vector<CComBSTR> ids;
                hr = SafeArrayToStringVector(property.parray, ids);
                if (FAILED(hr))
                {
                    return hr;
                }
                filter.ids = ConvertStrings(ids);
            }

            property.Clear();
            hr = driver.GetProperty(2, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            if (property.vt & VT_ARRAY)
            {
                std::vector<CComBSTR> authors;
                hr = SafeArrayToStringVector(property.parray, authors);
                if (FAILED(hr))
                {
                    return hr;
                }
                filter.authors = ConvertStrings(authors);
            }

            property.Clear();
            hr = driver.GetProperty(3, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            if (property.vt & VT_ARRAY)
            {
                std::vector<long> kinds;
                hr = SafeArrayToLongVector(property.parray, kinds);
                if (FAILED(hr))
                {
                    return hr;
                }
                filter.kinds.assign(kinds.begin(), kinds.end());
            }

            property.Clear();
            hr = driver.GetProperty(4, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            if (property.vt & VT_ARRAY)
            {
                LONG tagLower = 0;
                LONG tagUpper = -1;
                hr = SafeArrayGetLBound(property.parray, 1, &tagLower);
                if (FAILED(hr))
                {
                    return hr;
                }
                hr = SafeArrayGetUBound(property.parray, 1, &tagUpper);
                if (FAILED(hr))
                {
                    return hr;
                }

                for (LONG tagIndex = tagLower; tagIndex <= tagUpper; ++tagIndex)
                {
                    CComVariant tagVariant;
                    hr = SafeArrayGetElement(property.parray, &tagIndex, &tagVariant);
                    if (FAILED(hr))
                    {
                        return hr;
                    }

                    if (tagVariant.vt != VT_DISPATCH || !tagVariant.pdispVal)
                    {
                        return DISP_E_TYPEMISMATCH;
                    }

                    CComDispatchDriver tagDriver(tagVariant.pdispVal);
                    CComVariant labelVariant;
                    hr = tagDriver.GetProperty(1, &labelVariant);
                    if (FAILED(hr))
                    {
                        return hr;
                    }
                    hr = labelVariant.ChangeType(VT_BSTR);
                    if (FAILED(hr))
                    {
                        return hr;
                    }

                    std::wstring label = BstrToWString(labelVariant.bstrVal);

                    CComVariant valuesVariant;
                    hr = tagDriver.GetProperty(2, &valuesVariant);
                    if (FAILED(hr))
                    {
                        return hr;
                    }
                    if (!(valuesVariant.vt & VT_ARRAY))
                    {
                        return DISP_E_TYPEMISMATCH;
                    }

                    std::vector<CComBSTR> valueStrings;
                    hr = SafeArrayToStringVector(valuesVariant.parray, valueStrings);
                    if (FAILED(hr))
                    {
                        return hr;
                    }

                    filter.tagQueries[label] = ConvertStrings(valueStrings);
                }
            }

            property.Clear();
            hr = driver.GetProperty(5, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            const auto since = VariantToDouble(property);
            if (since)
            {
                filter.since = since;
            }

            property.Clear();
            hr = driver.GetProperty(6, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            const auto until = VariantToDouble(property);
            if (until)
            {
                filter.until = until;
            }

            property.Clear();
            hr = driver.GetProperty(7, &property);
            if (FAILED(hr))
            {
                return hr;
            }
            const auto limit = VariantToLong(property);
            if (limit)
            {
                filter.limit = static_cast<int>(*limit);
            }

            out.push_back(std::move(filter));
        }

        return S_OK;
    }

    HRESULT NostrJsonSerializer::PopulateEventDispatch(const EventData& data, INostrEvent** result) const
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;
        CComObject<CNostrEvent>* instance = nullptr;
        HRESULT hr = CComObject<CNostrEvent>::CreateInstance(&instance);
        if (FAILED(hr))
        {
            return hr;
        }

        instance->AddRef();

        hr = instance->put_Id(CComBSTR(data.id.c_str()));
        if (SUCCEEDED(hr))
        {
            hr = instance->put_PublicKey(CComBSTR(data.publicKey.c_str()));
        }
        if (SUCCEEDED(hr))
        {
            hr = instance->put_CreatedAt(data.createdAt);
        }
        if (SUCCEEDED(hr))
        {
            hr = instance->put_Kind(data.kind);
        }

        if (SUCCEEDED(hr))
        {
            std::vector<std::vector<CComBSTR>> tags;
            tags.reserve(data.tags.size());
            for (const auto& row : data.tags)
            {
                std::vector<CComBSTR> converted;
                converted.reserve(row.size());
                for (const auto& entry : row)
                {
                    converted.emplace_back(entry.c_str());
                }
                tags.push_back(std::move(converted));
            }

            SAFEARRAY* tagArray = nullptr;
            hr = CreateSafeArrayFromTagMatrix(tags, &tagArray);
            if (SUCCEEDED(hr))
            {
                hr = instance->put_Tags(tagArray);
                SafeArrayDestroy(tagArray);
            }
        }

        if (SUCCEEDED(hr))
        {
            hr = instance->put_Content(CComBSTR(data.content.c_str()));
        }

        if (SUCCEEDED(hr))
        {
            hr = instance->put_Signature(CComBSTR(data.signature.c_str()));
        }

        if (FAILED(hr))
        {
            instance->Release();
            return hr;
        }

        hr = instance->QueryInterface(__uuidof(INostrEvent), reinterpret_cast<void**>(result));
        instance->Release();
        return hr;
    }
}
