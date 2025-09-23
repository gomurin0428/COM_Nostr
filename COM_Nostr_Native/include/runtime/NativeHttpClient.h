#pragma once

#include "runtime/ClientRuntimeOptions.h"

#include <optional>
#include <string>
#include <vector>
#include <winhttp.h>

namespace com::nostr::native
{
    class NativeHttpClient
    {
    public:
        struct RelayInformation
        {
            std::wstring metadataJson;
            std::vector<int> supportedNips;
        };

        explicit NativeHttpClient(ClientRuntimeOptions options);

        [[nodiscard]] HRESULT FetchRelayInformation(const std::wstring& url, RelayInformation& information) const;

    private:
        [[nodiscard]] HRESULT PerformGet(const std::wstring& host,
                                         INTERNET_PORT port,
                                         const std::wstring& path,
                                         bool useTls,
                                         RelayInformation& information) const;

        ClientRuntimeOptions options_;
    };
}
