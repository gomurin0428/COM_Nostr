#include "pch.h"

#include <nlohmann/json.hpp>

#include "../../../COM_Nostr_Native/src/dto/NostrDtoComObjects.h"
#include "../../../COM_Nostr_Native/src/serialization/NostrJsonSerializer.h"
#include "../../../COM_Nostr_Native/include/ComValueHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace com::nostr::native::tests
{
    namespace
    {
        std::string ToString(const std::vector<uint8_t>& bytes)
        {
            return std::string(bytes.begin(), bytes.end());
        }

        class SimpleDispatch : public IDispatch
        {
        public:
            SimpleDispatch() noexcept : refCount_(1) {}
            SimpleDispatch(const SimpleDispatch&) = delete;
            SimpleDispatch& operator=(const SimpleDispatch&) = delete;

            STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
            {
                if (!ppv)
                {
                    return E_POINTER;
                }

                if (riid == IID_IUnknown || riid == IID_IDispatch)
                {
                    *ppv = static_cast<IDispatch*>(this);
                    AddRef();
                    return S_OK;
                }

                *ppv = nullptr;
                return E_NOINTERFACE;
            }

            STDMETHOD_(ULONG, AddRef)() override
            {
                return static_cast<ULONG>(InterlockedIncrement(&refCount_));
            }

            STDMETHOD_(ULONG, Release)() override
            {
                const ULONG remaining = static_cast<ULONG>(InterlockedDecrement(&refCount_));
                if (remaining == 0)
                {
                    delete this;
                }

                return remaining;
            }

            STDMETHOD(GetTypeInfoCount)(UINT* pctinfo) override
            {
                if (!pctinfo)
                {
                    return E_POINTER;
                }

                *pctinfo = 0;
                return S_OK;
            }

            STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) override
            {
                return E_NOTIMPL;
            }

            STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override
            {
                return DISP_E_UNKNOWNNAME;
            }

        protected:
            virtual ~SimpleDispatch() = default;

        private:
            volatile LONG refCount_;
        };

        class FakeEventDispatch final : public SimpleDispatch
        {
        public:
            explicit FakeEventDispatch(const com::nostr::native::NostrJsonSerializer::EventData& data)
            {
                id_ = data.id.c_str();
                publicKey_ = data.publicKey.c_str();
                createdAt_.vt = VT_R8;
                createdAt_.dblVal = data.createdAt;
                kind_.vt = VT_I4;
                kind_.lVal = data.kind;
                content_ = data.content.c_str();
                signature_ = data.signature.c_str();

                if (!data.tags.empty())
                {
                    std::vector<std::vector<CComBSTR>> tagMatrix;
                    tagMatrix.reserve(data.tags.size());
                    for (const auto& row : data.tags)
                    {
                        std::vector<CComBSTR> converted;
                        converted.reserve(row.size());
                        for (const auto& value : row)
                        {
                            converted.emplace_back(value.c_str());
                        }
                        tagMatrix.push_back(std::move(converted));
                    }

                    SAFEARRAY* array = nullptr;
                    if (SUCCEEDED(CreateSafeArrayFromTagMatrix(tagMatrix, &array)))
                    {
                        tags_.vt = VT_ARRAY | VT_VARIANT;
                        tags_.parray = array;
                    }
                }
            }

            ~FakeEventDispatch() override
            {
                tags_.Clear();
            }

            STDMETHOD(Invoke)(DISPID dispIdMember, REFIID, LCID, WORD wFlags, DISPPARAMS*, VARIANT* pVarResult, EXCEPINFO*, UINT*) override
            {
                if (!(wFlags & DISPATCH_PROPERTYGET) || !pVarResult)
                {
                    return DISP_E_MEMBERNOTFOUND;
                }

                const CComVariant* source = nullptr;
                switch (dispIdMember)
                {
                case 1:
                    source = &id_;
                    break;
                case 2:
                    source = &publicKey_;
                    break;
                case 3:
                    source = &createdAt_;
                    break;
                case 4:
                    source = &kind_;
                    break;
                case 5:
                    source = &tags_;
                    break;
                case 6:
                    source = &content_;
                    break;
                case 7:
                    source = &signature_;
                    break;
                default:
                    return DISP_E_MEMBERNOTFOUND;
                }

                return VariantCopy(pVarResult, const_cast<CComVariant*>(source));
            }

        private:
            CComVariant id_;
            CComVariant publicKey_;
            CComVariant createdAt_;
            CComVariant kind_;
            CComVariant tags_;
            CComVariant content_;
            CComVariant signature_;
        };

        class FakeTagQueryDispatch final : public SimpleDispatch
        {
        public:
            FakeTagQueryDispatch(std::wstring label, std::vector<std::wstring> values)
            {
                label_ = label.c_str();

                std::vector<CComBSTR> converted;
                converted.reserve(values.size());
                for (const auto& value : values)
                {
                    converted.emplace_back(value.c_str());
                }

                SAFEARRAY* array = nullptr;
                if (SUCCEEDED(CreateSafeArrayFromStrings(converted, &array)))
                {
                    values_.vt = VT_ARRAY | VT_BSTR;
                    values_.parray = array;
                }
            }

            ~FakeTagQueryDispatch() override
            {
                values_.Clear();
            }

            STDMETHOD(Invoke)(DISPID dispIdMember, REFIID, LCID, WORD wFlags, DISPPARAMS*, VARIANT* pVarResult, EXCEPINFO*, UINT*) override
            {
                if (!(wFlags & DISPATCH_PROPERTYGET) || !pVarResult)
                {
                    return DISP_E_MEMBERNOTFOUND;
                }

                const CComVariant* source = nullptr;
                switch (dispIdMember)
                {
                case 1:
                    source = &label_;
                    break;
                case 2:
                    source = &values_;
                    break;
                default:
                    return DISP_E_MEMBERNOTFOUND;
                }

                return VariantCopy(pVarResult, const_cast<CComVariant*>(source));
            }

        private:
            CComVariant label_;
            CComVariant values_;
        };

        class FakeFilterDispatch final : public SimpleDispatch
        {
        public:
            FakeFilterDispatch(std::vector<std::wstring> ids, std::vector<CComPtr<IDispatch>> tagQueries)
                : tagDispatches_(std::move(tagQueries))
            {
                if (!ids.empty())
                {
                    std::vector<CComBSTR> converted;
                    converted.reserve(ids.size());
                    for (const auto& value : ids)
                    {
                        converted.emplace_back(value.c_str());
                    }

                    SAFEARRAY* array = nullptr;
                    if (SUCCEEDED(CreateSafeArrayFromStrings(converted, &array)))
                    {
                        ids_.vt = VT_ARRAY | VT_BSTR;
                        ids_.parray = array;
                    }
                }

                if (!tagDispatches_.empty())
                {
                    SAFEARRAYBOUND bound{};
                    bound.lLbound = 0;
                    bound.cElements = static_cast<ULONG>(tagDispatches_.size());
                    SAFEARRAY* array = SafeArrayCreate(VT_VARIANT, 1, &bound);
                    if (array)
                    {
                        for (LONG index = 0; index < static_cast<LONG>(tagDispatches_.size()); ++index)
                        {
                            VARIANT element;
                            VariantInit(&element);
                            element.vt = VT_DISPATCH;
                            element.pdispVal = tagDispatches_[static_cast<size_t>(index)];
                            if (element.pdispVal)
                            {
                                element.pdispVal->AddRef();
                            }

                            SafeArrayPutElement(array, &index, &element);
                            VariantClear(&element);
                        }

                        tags_.vt = VT_ARRAY | VT_VARIANT;
                        tags_.parray = array;
                    }
                }

                since_.vt = VT_EMPTY;
                until_.vt = VT_EMPTY;
                limit_.vt = VT_EMPTY;
            }

            ~FakeFilterDispatch() override
            {
                ids_.Clear();
                tags_.Clear();
            }

            STDMETHOD(Invoke)(DISPID dispIdMember, REFIID, LCID, WORD wFlags, DISPPARAMS*, VARIANT* pVarResult, EXCEPINFO*, UINT*) override
            {
                if (!(wFlags & DISPATCH_PROPERTYGET) || !pVarResult)
                {
                    return DISP_E_MEMBERNOTFOUND;
                }

                const CComVariant* source = nullptr;
                switch (dispIdMember)
                {
                case 1:
                    source = &ids_;
                    break;
                case 2:
                    source = &authors_;
                    break;
                case 3:
                    source = &kinds_;
                    break;
                case 4:
                    source = &tags_;
                    break;
                case 5:
                    source = &since_;
                    break;
                case 6:
                    source = &until_;
                    break;
                case 7:
                    source = &limit_;
                    break;
                default:
                    return DISP_E_MEMBERNOTFOUND;
                }

                return VariantCopy(pVarResult, const_cast<CComVariant*>(source));
            }

        private:
            CComVariant ids_;
            CComVariant authors_;
            CComVariant kinds_;
            CComVariant tags_;
            CComVariant since_;
            CComVariant until_;
            CComVariant limit_;
            std::vector<CComPtr<IDispatch>> tagDispatches_;
        };
    }

    TEST_CLASS(SerializerTests)
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

        TEST_METHOD(SerializeEvent_ProducesExpectedSchema)
        {
            NostrJsonSerializer serializer;
            NostrJsonSerializer::EventData event{};
            event.id = L"123";
            event.publicKey = L"abcdef";
            event.createdAt = 1700000000;
            event.kind = 1;
            event.tags = { { L"p", L"value" } };
            event.content = L"hello";
            event.signature = L"deadbeef";

            const auto bytes = serializer.SerializeEvent(event);
            const auto text = ToString(bytes);
            const auto json = nlohmann::json::parse(text);

            Assert::AreEqual(std::string("EVENT"), json[0].get<std::string>());
            const auto& payload = json[1];
            Assert::AreEqual(std::string("123"), payload.at("id").get<std::string>());
            Assert::AreEqual(1700000000.0, payload.at("created_at").get<double>());
            Assert::AreEqual(1, payload.at("kind").get<int>());
            Assert::AreEqual(std::string("hello"), payload.at("content").get<std::string>());
        }

        TEST_METHOD(DeserializeEvent_RoundTrip)
        {
            NostrJsonSerializer serializer;
            const std::string jsonText = R"(["EVENT","sub",{"id":"A","pubkey":"B","created_at":10,"kind":2,"tags":[["p","tag"]],"content":"body","sig":"sig"}])";
            const std::vector<uint8_t> payload(jsonText.begin(), jsonText.end());

            const auto message = serializer.DeserializeEvent(payload);
            Assert::IsTrue(message.subscriptionId.has_value());
            Assert::AreEqual(std::wstring(L"sub"), message.subscriptionId.value());
            Assert::AreEqual(std::wstring(L"A"), message.event.id);
            Assert::AreEqual(10.0, message.event.createdAt);
            Assert::AreEqual(2, message.event.kind);
            Assert::AreEqual(std::wstring(L"body"), message.event.content);
            Assert::AreEqual<size_t>(1, message.event.tags.size());
            Assert::AreEqual(std::wstring(L"tag"), message.event.tags[0][1]);
        }

        TEST_METHOD(ReadEventFromDispatch_CapturesAllFields)
        {
            NostrJsonSerializer::EventData source{};
            source.id = L"id123";
            source.publicKey = L"pk";
            source.createdAt = 42.0;
            source.kind = 5;
            source.tags = { { L"p", L"inner" } };
            source.content = L"content";
            source.signature = L"sig";

            CComPtr<IDispatch> dispatch = new FakeEventDispatch(source);

            NostrJsonSerializer serializer;
            NostrJsonSerializer::EventData data{};
            Assert::AreEqual(S_OK, serializer.ReadEventFromDispatch(dispatch, data));

            Assert::AreEqual(std::wstring(L"id123"), data.id);
            Assert::AreEqual(std::wstring(L"pk"), data.publicKey);
            Assert::AreEqual(42.0, data.createdAt);
            Assert::AreEqual(5, data.kind);
            Assert::AreEqual(std::wstring(L"inner"), data.tags[0][1]);
            Assert::AreEqual(std::wstring(L"content"), data.content);
            Assert::AreEqual(std::wstring(L"sig"), data.signature);
        }

        TEST_METHOD(ReadFiltersFromSafeArray_ParsesTagQuery)
        {
            CComPtr<IDispatch> tagQuery = new FakeTagQueryDispatch(L"#p", { L"alpha", L"beta" });
            CComPtr<IDispatch> filterDispatch = new FakeFilterDispatch({ L"id" }, { tagQuery });

            SAFEARRAYBOUND filtersBound{};
            filtersBound.lLbound = 0;
            filtersBound.cElements = 1;
            SAFEARRAY* filtersArray = SafeArrayCreate(VT_VARIANT, 1, &filtersBound);
            LONG filterIndex = 0;
            VARIANT element;
            VariantInit(&element);
            element.vt = VT_DISPATCH;
            IDispatch* rawFilter = filterDispatch;
            rawFilter->AddRef();
            element.pdispVal = rawFilter;
            Assert::AreEqual(S_OK, SafeArrayPutElement(filtersArray, &filterIndex, &element));
            VariantClear(&element);

            NostrJsonSerializer serializer;
            std::vector<NostrJsonSerializer::FilterData> filters;
            Assert::AreEqual(S_OK, serializer.ReadFiltersFromSafeArray(filtersArray, filters));
            SafeArrayDestroy(filtersArray);

            Assert::AreEqual<size_t>(1, filters.size());
            Assert::AreEqual(std::wstring(L"id"), filters[0].ids[0]);
            Assert::AreEqual<size_t>(1, filters[0].tagQueries.size());
            const auto& firstTag = filters[0].tagQueries.begin();
            Assert::AreEqual(std::wstring(L"#p"), firstTag->first);
            Assert::AreEqual(std::wstring(L"beta"), firstTag->second[1]);
        }

        TEST_METHOD(DeserializeAuthChallenge_ReadsNumericExpiration)
        {
            NostrJsonSerializer serializer;
            const std::string jsonText = R"(["AUTH", {"challenge":"abc","expires_at":1700000000}])";
            const std::vector<uint8_t> payload(jsonText.begin(), jsonText.end());

            const auto message = serializer.DeserializeAuthChallenge(payload);
            Assert::AreEqual(std::wstring(L"abc"), message.challenge);
            Assert::IsTrue(message.expiresAt.has_value());
            Assert::AreEqual(1700000000.0, message.expiresAt.value());
        }

        TEST_METHOD(DeserializeAuthChallenge_ReadsStringExpiration)
        {
            NostrJsonSerializer serializer;
            const std::string jsonText = R"(["AUTH", {"challenge":"xyz","expiresAt":"1700000012"}])";
            const std::vector<uint8_t> payload(jsonText.begin(), jsonText.end());

            const auto message = serializer.DeserializeAuthChallenge(payload);
            Assert::AreEqual(std::wstring(L"xyz"), message.challenge);
            Assert::IsTrue(message.expiresAt.has_value());
            Assert::AreEqual(1700000012.0, message.expiresAt.value());
        }
    };
}
