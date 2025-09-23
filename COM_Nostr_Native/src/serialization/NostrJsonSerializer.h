#pragma once

#include "NostrDtoComObjects.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace com::nostr::native
{
    class NostrJsonSerializer
    {
    public:
        struct EventData
        {
            std::wstring id;
            std::wstring publicKey;
            double createdAt = 0.0;
            int kind = 0;
            std::vector<std::vector<std::wstring>> tags;
            std::wstring content;
            std::wstring signature;
        };

        struct FilterData
        {
            std::vector<std::wstring> ids;
            std::vector<std::wstring> authors;
            std::vector<int> kinds;
            std::map<std::wstring, std::vector<std::wstring>, std::less<>> tagQueries;
            std::optional<double> since;
            std::optional<double> until;
            std::optional<int> limit;
        };

        struct RequestMessage
        {
            std::wstring subscriptionId;
            std::vector<FilterData> filters;
        };

        struct EventMessage
        {
            std::optional<std::wstring> subscriptionId;
            EventData event;
        };

        struct OkMessage
        {
            std::wstring eventId;
            bool success = false;
            std::wstring message;
        };

        struct NoticeMessage
        {
            std::wstring message;
        };

        struct AuthChallengeMessage
        {
            std::wstring challenge;
            std::optional<double> expiresAt;
        };

        struct EndOfStoredEventsMessage
        {
            std::wstring subscriptionId;
        };

        struct ClosedMessage
        {
            std::wstring subscriptionId;
            std::wstring reason;
        };

        std::vector<uint8_t> SerializeEvent(const EventData& event) const;
        std::vector<uint8_t> SerializeAuth(const EventData& event) const;
        std::vector<uint8_t> SerializeRequest(const RequestMessage& request) const;
        std::vector<uint8_t> SerializeClose(const std::wstring& subscriptionId) const;

        EventMessage DeserializeEvent(const std::vector<uint8_t>& payload) const;
        OkMessage DeserializeOk(const std::vector<uint8_t>& payload) const;
        NoticeMessage DeserializeNotice(const std::vector<uint8_t>& payload) const;
        AuthChallengeMessage DeserializeAuthChallenge(const std::vector<uint8_t>& payload) const;
        EndOfStoredEventsMessage DeserializeEndOfStoredEvents(const std::vector<uint8_t>& payload) const;
        ClosedMessage DeserializeClosed(const std::vector<uint8_t>& payload) const;

        HRESULT ReadEventFromDispatch(IDispatch* dispatch, EventData& event) const;
        HRESULT ReadEventDraftFromDispatch(IDispatch* dispatch, EventData& event) const;
        HRESULT ReadFiltersFromSafeArray(SAFEARRAY* filters, std::vector<FilterData>& out) const;
        HRESULT PopulateEventDispatch(const EventData& data, INostrEvent** result) const;
    };
}
