#include "pch.h"

#include "../../../COM_Nostr_Native/include/runtime/ClientRuntimeOptions.h"
#include "../../../COM_Nostr_Native/include/runtime/WinHttpWebSocket.h"

#include <nlohmann/json.hpp>
#include <winhttp.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <random>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace com::nostr::native::tests
{
    namespace
    {
        class StrfryRelayHost
        {
        public:
            StrfryRelayHost()
            {
                GenerateIdentifiers();
            }

            ~StrfryRelayHost()
            {
                Stop();
                std::error_code ec;
                std::filesystem::remove_all(dataPath_, ec);
            }

            bool Start()
            {
                if (started_)
                {
                    return true;
                }

                for (int attempt = 0; attempt < 5; ++attempt)
                {
                    if (attempt > 0)
                    {
                        std::error_code cleanupEc;
                        std::filesystem::remove_all(dataPath_, cleanupEc);
                        GenerateIdentifiers();
                    }

                    std::error_code dirEc;
                    std::filesystem::create_directories(dataPath_, dirEc);
                    if (dirEc)
                    {
                        continue;
                    }

                    std::wstringstream command;
                    command << L"docker run --rm -d -p " << port_ << L":7777 -v \"" << dataPath_.wstring()
                            << L"\":/app/strfry-db --name " << containerName_ << L" dockurr/strfry";

                    const int exitCode = _wsystem(command.str().c_str());
                    if (exitCode == 0)
                    {
                        started_ = true;
                        std::this_thread::sleep_for(std::chrono::seconds(4));
                        return true;
                    }
                }

                return false;
            }

            void Stop()
            {
                if (!started_)
                {
                    return;
                }

                std::wstringstream command;
                command << L"docker stop " << containerName_;
                _wsystem(command.str().c_str());
                started_ = false;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            [[nodiscard]] std::wstring WebSocketUrl() const
            {
                std::wstringstream url;
                url << L"ws://127.0.0.1:" << port_;
                return url.str();
            }

            [[nodiscard]] std::wstring MetadataUrl() const
            {
                std::wstringstream url;
                url << L"http://127.0.0.1:" << port_;
                return url.str();
            }

            [[nodiscard]] int Port() const noexcept
            {
                return port_;
            }

        private:
            void GenerateIdentifiers()
            {
                std::random_device rd;
                std::mt19937_64 gen(rd());
                std::uniform_int_distribution<int> portDist(20000, 59000);
                port_ = portDist(gen);
                std::uniform_int_distribution<int> idDist(0, (std::numeric_limits<int>::max)());
                containerName_ = L"strfry_native_test_" + std::to_wstring(idDist(gen));
                const std::filesystem::path base = std::filesystem::temp_directory_path();
                dataPath_ = base / (containerName_ + L"_db");
            }

            int port_ = 0;
            std::wstring containerName_;
            std::filesystem::path dataPath_;
            bool started_ = false;
        };
    }

    TEST_CLASS(WebSocketHandshakeTests)
    {
    public:
        static void ClassInitialize()
        {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr))
            {
                throw std::runtime_error("CoInitializeEx failed");
            }
        }

        static void ClassCleanup()
        {
            CoUninitialize();
        }

        TEST_METHOD(ConnectsAndReceivesEndOfStoredEvents)
        {
            StrfryRelayHost relay;
            Assert::IsTrue(relay.Start(), L"Failed to start strfry docker container.");

                        ClientRuntimeOptions options;
            auto logMessage = [](const std::wstring& message)
            {
                std::wofstream trace(L"TestResults\\ConnectsAndReceives.trace.log", std::ios::app);
                trace << message << std::endl;
            };
            logMessage(L"=== Test start ===");
            options.SetUserAgent(L"COM_Nostr_NativeTests/1.0");
            options.SetConnectTimeout(std::chrono::seconds(5));
            options.SetReceiveTimeout(std::chrono::seconds(5));
            options.SetSendTimeout(std::chrono::seconds(5));

            auto waitForHttp = [&]() -> bool
            {
                struct HttpHandle
                {
                    HINTERNET handle = nullptr;
                    ~HttpHandle()
                    {
                        if (handle)
                        {
                            WinHttpCloseHandle(handle);
                        }
                    }
                };

                for (int attempt = 0; attempt < 1; ++attempt)
                {
                    HttpHandle session{ WinHttpOpen(options.UserAgent().c_str(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
                    if (!session.handle)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }

                    HttpHandle connect{ WinHttpConnect(session.handle, L"127.0.0.1", static_cast<INTERNET_PORT>(relay.Port()), 0) };
                    if (!connect.handle)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }

                    HttpHandle request{ WinHttpOpenRequest(connect.handle, L"HEAD", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0) };
                    if (!request.handle)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }

                    if (!WinHttpSendRequest(request.handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                        !WinHttpReceiveResponse(request.handle, nullptr))
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }

                    DWORD statusCode = 0;
                    DWORD statusSize = sizeof(statusCode);
                    if (WinHttpQueryHeaders(request.handle,
                                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                             WINHTTP_HEADER_NAME_BY_INDEX,
                                             &statusCode,
                                             &statusSize,
                                             WINHTTP_NO_HEADER_INDEX) &&
                        statusCode >= HTTP_STATUS_OK && statusCode < HTTP_STATUS_BAD_REQUEST)
                    {
                        return true;
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                return false;
            };

            Assert::IsTrue(waitForHttp(), L"Relay HTTP endpoint did not respond within timeout.");

            WinHttpWebSocket socket;
            HRESULT hr = E_FAIL;
            for (int attempt = 0; attempt < 1; ++attempt)
            {
                hr = socket.Connect(relay.WebSocketUrl(), options);
                if (SUCCEEDED(hr))
                {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            Assert::AreEqual(S_OK, hr, L"Failed to establish WebSocket connection to strfry.");

            const std::string requestText = R"(["REQ","native-test",{"kinds":[1],"limit":1}])";
                        const std::vector<uint8_t> requestPayload(requestText.begin(), requestText.end());
            {
                std::wstringstream log;
                log << L"REQ bytes=" << requestPayload.size() << L" content=" << std::wstring(requestText.begin(), requestText.end());
                logMessage(log.str());
            }
            Assert::AreEqual(S_OK, socket.SendText(requestPayload, true));

            bool eoseReceived = false;
            for (int attempt = 0; attempt < 4 && !eoseReceived; ++attempt)
            {
                NativeWebSocketMessage message;
                                const HRESULT receiveHr = socket.Receive(10000, message);
                {
                    std::wstringstream log;
                    log << L"receive attempt=" << attempt << L" hr=0x" << std::hex << receiveHr;
                    logMessage(log.str());
                }
                Assert::AreEqual(S_OK, receiveHr, L"Failed to receive WebSocket message.");

                                const std::string payload(message.payload.begin(), message.payload.end());
                logMessage(std::wstring(L"payload: ") + std::wstring(payload.begin(), payload.end()));
                const auto json = nlohmann::json::parse(payload);
                Assert::IsTrue(json.is_array(), L"Unexpected non-array JSON payload.", LINE_INFO());
                                const std::string messageType = json.at(0).get<std::string>();
                logMessage(std::wstring(L"messageType: ") + std::wstring(messageType.begin(), messageType.end()));
                if (messageType == "EOSE")
                {
                    Assert::AreEqual<std::string>("native-test", json.at(1).get<std::string>());
                    eoseReceived = true;
                }
            }

            Assert::IsTrue(eoseReceived, L"EOSE message was not received.");

            Assert::AreEqual(S_OK, socket.Close(WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, L""));
        }
    };
}



















