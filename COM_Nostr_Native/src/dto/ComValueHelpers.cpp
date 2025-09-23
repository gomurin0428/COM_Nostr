#include "pch.h"

#include "ComValueHelpers.h"

#include <cwchar>
#include <limits>
#include <cerrno>

namespace com::nostr::native
{
    using ATL::CComBSTR;
    using ATL::CComVariant;

    namespace
    {
        HRESULT GetBounds(SAFEARRAY* array, LONG& lower, LONG& upper)
        {
            if (!array)
            {
                lower = 0;
                upper = -1;
                return S_OK;
            }

            HRESULT hr = SafeArrayGetLBound(array, 1, &lower);
            if (FAILED(hr))
            {
                return hr;
            }

            hr = SafeArrayGetUBound(array, 1, &upper);
            if (FAILED(hr))
            {
                return hr;
            }

            return S_OK;
        }

        std::optional<VARTYPE> TryGetVarType(SAFEARRAY* array)
        {
            VARTYPE vt = VT_EMPTY;
            HRESULT hr = SafeArrayGetVartype(array, &vt);
            if (FAILED(hr))
            {
                return std::nullopt;
            }

            return vt;
        }

        double ParseWideToDouble(const std::wstring& text, bool& success)
        {
            wchar_t* end = nullptr;
            errno = 0;
            const double value = std::wcstod(text.c_str(), &end);
            if (errno != 0 || end == text.c_str())
            {
                success = false;
                return 0.0;
            }

            success = !std::isnan(value) && !std::isinf(value);
            return value;
        }

        long ParseWideToLong(const std::wstring& text, bool& success)
        {
            wchar_t* end = nullptr;
            errno = 0;
            const long value = std::wcstol(text.c_str(), &end, 10);
            if (errno != 0 || end == text.c_str())
            {
                success = false;
                return 0;
            }

            success = true;
            return value;
        }
    }

    std::wstring BstrToWString(BSTR value)
    {
        if (!value)
        {
            return std::wstring();
        }

        return std::wstring(value, value + SysStringLen(value));
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return std::string();
        }

        const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            return std::string();
        }

        std::string buffer(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), buffer.data(), required, nullptr, nullptr);
        return buffer;
    }

    std::wstring Utf8ToWide(std::string_view value)
    {
        if (value.empty())
        {
            return std::wstring();
        }

        const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (required <= 0)
        {
            return std::wstring();
        }

        std::wstring buffer(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), buffer.data(), required);
        return buffer;
    }

    std::string BstrToUtf8(BSTR value)
    {
        return WideToUtf8(BstrToWString(value));
    }

    CComBSTR Utf8ToBstr(std::string_view value)
    {
        return CComBSTR(Utf8ToWide(value).c_str());
    }

    HRESULT SafeArrayToStringVector(SAFEARRAY* array, std::vector<CComBSTR>& output)
    {
        output.clear();
        if (!array)
        {
            return S_OK;
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = GetBounds(array, lower, upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        const auto vt = TryGetVarType(array);
        const LONG count = upper - lower + 1;
        output.reserve(static_cast<size_t>(count));

        for (LONG offset = 0; offset < count; ++offset)
        {
            LONG index = lower + offset;

            if (vt && vt.value() == VT_BSTR)
            {
                BSTR raw = nullptr;
                hr = SafeArrayGetElement(array, &index, &raw);
                if (FAILED(hr))
                {
                    return hr;
                }

                CComBSTR value(raw);
                SysFreeString(raw);
                output.emplace_back(value);
            }
            else
            {
                CComVariant variant;
                hr = SafeArrayGetElement(array, &index, &variant);
                if (FAILED(hr))
                {
                    return hr;
                }

                if (variant.vt == VT_EMPTY || variant.vt == VT_NULL)
                {
                    output.emplace_back();
                    continue;
                }

                hr = variant.ChangeType(VT_BSTR);
                if (FAILED(hr))
                {
                    return DISP_E_TYPEMISMATCH;
                }

                output.emplace_back(variant.bstrVal);
            }
        }

        return S_OK;
    }

    HRESULT SafeArrayToLongVector(SAFEARRAY* array, std::vector<long>& output)
    {
        output.clear();
        if (!array)
        {
            return S_OK;
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = GetBounds(array, lower, upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        const auto vt = TryGetVarType(array);
        const LONG count = upper - lower + 1;
        output.reserve(static_cast<size_t>(count));

        for (LONG offset = 0; offset < count; ++offset)
        {
            LONG index = lower + offset;

            if (vt && (vt.value() == VT_I4 || vt.value() == VT_INT))
            {
                long value = 0;
                hr = SafeArrayGetElement(array, &index, &value);
                if (FAILED(hr))
                {
                    return hr;
                }

                output.push_back(value);
            }
            else
            {
                CComVariant variant;
                hr = SafeArrayGetElement(array, &index, &variant);
                if (FAILED(hr))
                {
                    return hr;
                }

                if (variant.vt == VT_EMPTY || variant.vt == VT_NULL)
                {
                    output.push_back(0);
                    continue;
                }

                hr = variant.ChangeType(VT_I4);
                if (FAILED(hr))
                {
                    return DISP_E_TYPEMISMATCH;
                }

                output.push_back(variant.lVal);
            }
        }

        return S_OK;
    }

    HRESULT SafeArrayToVariantVector(SAFEARRAY* array, std::vector<CComVariant>& output)
    {
        output.clear();
        if (!array)
        {
            return S_OK;
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = GetBounds(array, lower, upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        const LONG count = upper - lower + 1;
        output.reserve(static_cast<size_t>(count));

        for (LONG offset = 0; offset < count; ++offset)
        {
            LONG index = lower + offset;
            CComVariant value;
            hr = SafeArrayGetElement(array, &index, &value);
            if (FAILED(hr))
            {
                return hr;
            }

            output.emplace_back(value);
        }

        return S_OK;
    }

    HRESULT SafeArrayToTagMatrix(SAFEARRAY* array, std::vector<std::vector<CComBSTR>>& output)
    {
        output.clear();
        if (!array)
        {
            return S_OK;
        }

        LONG lower = 0;
        LONG upper = -1;
        HRESULT hr = GetBounds(array, lower, upper);
        if (FAILED(hr))
        {
            return hr;
        }

        if (upper < lower)
        {
            return S_OK;
        }

        const LONG count = upper - lower + 1;
        output.reserve(static_cast<size_t>(count));

        for (LONG offset = 0; offset < count; ++offset)
        {
            LONG index = lower + offset;
            CComVariant variant;
            hr = SafeArrayGetElement(array, &index, &variant);
            if (FAILED(hr))
            {
                return hr;
            }

            if (!(variant.vt & VT_ARRAY))
            {
                return DISP_E_TYPEMISMATCH;
            }

            SAFEARRAY* inner = variant.parray;
            std::vector<CComBSTR> innerVector;
            hr = SafeArrayToStringVector(inner, innerVector);
            if (FAILED(hr))
            {
                return hr;
            }

            output.emplace_back(std::move(innerVector));
        }

        return S_OK;
    }

    HRESULT CreateSafeArrayFromStrings(const std::vector<CComBSTR>& values, SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;
        SAFEARRAYBOUND bound{};
        bound.lLbound = 0;
        bound.cElements = static_cast<ULONG>(values.size());

        SAFEARRAY* array = SafeArrayCreate(VT_BSTR, 1, &bound);
        if (!array)
        {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = S_OK;
        for (LONG index = 0; index < static_cast<LONG>(values.size()); ++index)
        {
            const CComBSTR& source = values[static_cast<size_t>(index)];
            const BSTR copy = source.Copy();
            if (!copy && source.m_str)
            {
                hr = E_OUTOFMEMORY;
                break;
            }

            hr = SafeArrayPutElement(array, &index, copy);
            SysFreeString(copy);
            if (FAILED(hr))
            {
                break;
            }
        }

        if (FAILED(hr))
        {
            SafeArrayDestroy(array);
            return hr;
        }

        *result = array;
        return S_OK;
    }

    HRESULT CreateSafeArrayFromLongs(const std::vector<long>& values, SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;
        SAFEARRAYBOUND bound{};
        bound.lLbound = 0;
        bound.cElements = static_cast<ULONG>(values.size());

        SAFEARRAY* array = SafeArrayCreate(VT_I4, 1, &bound);
        if (!array)
        {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = S_OK;
        for (LONG index = 0; index < static_cast<LONG>(values.size()); ++index)
        {
            long value = values[static_cast<size_t>(index)];
            hr = SafeArrayPutElement(array, &index, &value);
            if (FAILED(hr))
            {
                break;
            }
        }

        if (FAILED(hr))
        {
            SafeArrayDestroy(array);
            return hr;
        }

        *result = array;
        return S_OK;
    }

    HRESULT CreateSafeArrayFromVariants(const std::vector<CComVariant>& values, SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;
        SAFEARRAYBOUND bound{};
        bound.lLbound = 0;
        bound.cElements = static_cast<ULONG>(values.size());

        SAFEARRAY* array = SafeArrayCreate(VT_VARIANT, 1, &bound);
        if (!array)
        {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = S_OK;
        for (LONG index = 0; index < static_cast<LONG>(values.size()); ++index)
        {
            VARIANT copy;
            VariantInit(&copy);
            const VARIANT* sourceVariant = reinterpret_cast<const VARIANT*>(&values[static_cast<size_t>(index)]);
            hr = VariantCopy(&copy, const_cast<VARIANT*>(sourceVariant));
            if (FAILED(hr))
            {
                SafeArrayDestroy(array);
                return hr;
            }

            hr = SafeArrayPutElement(array, &index, &copy);
            VariantClear(&copy);
            if (FAILED(hr))
            {
                SafeArrayDestroy(array);
                return hr;
            }
        }

        *result = array;
        return S_OK;
    }

    HRESULT CreateSafeArrayFromTagMatrix(const std::vector<std::vector<CComBSTR>>& tags, SAFEARRAY** result)
    {
        if (!result)
        {
            return E_POINTER;
        }

        *result = nullptr;
        SAFEARRAYBOUND bound{};
        bound.lLbound = 0;
        bound.cElements = static_cast<ULONG>(tags.size());

        SAFEARRAY* array = SafeArrayCreate(VT_VARIANT, 1, &bound);
        if (!array)
        {
            return E_OUTOFMEMORY;
        }

        HRESULT hr = S_OK;
        for (LONG index = 0; index < static_cast<LONG>(tags.size()); ++index)
        {
            SAFEARRAY* inner = nullptr;
            hr = CreateSafeArrayFromStrings(tags[static_cast<size_t>(index)], &inner);
            if (FAILED(hr))
            {
                SafeArrayDestroy(array);
                return hr;
            }

            VARIANT element;
            VariantInit(&element);
            element.vt = VT_ARRAY | VT_BSTR;
            element.parray = inner;
            hr = SafeArrayPutElement(array, &index, &element);
            if (FAILED(hr))
            {
                SafeArrayDestroy(inner);
                SafeArrayDestroy(array);
                return hr;
            }

            element.parray = nullptr;
            VariantClear(&element);
        }

        *result = array;
        return S_OK;
    }

    HRESULT CopyVariant(const VARIANT& source, VARIANT* destination)
    {
        if (!destination)
        {
            return E_POINTER;
        }

        HRESULT hr = VariantClear(destination);
        if (FAILED(hr))
        {
            return hr;
        }

        return VariantCopy(destination, const_cast<VARIANT*>(&source));
    }

    std::optional<double> VariantToDouble(const VARIANT& value)
    {
        if (value.vt == VT_EMPTY || value.vt == VT_NULL || value.vt == VT_ERROR)
        {
            return std::nullopt;
        }

        CComVariant copy(value);
        if (copy.vt == VT_DATE)
        {
            SYSTEMTIME st{};
            if (VariantTimeToSystemTime(copy.date, &st))
            {
                FILETIME ft{};
                if (SystemTimeToFileTime(&st, &ft))
                {
                    ULARGE_INTEGER uli{};
                    uli.LowPart = ft.dwLowDateTime;
                    uli.HighPart = ft.dwHighDateTime;
                    constexpr ULONGLONG hundredNanosecondsPerSecond = 10000000ULL;
                    constexpr ULONGLONG unixEpochOffset = 116444736000000000ULL;
                    if (uli.QuadPart >= unixEpochOffset)
                    {
                        const double seconds = static_cast<double>(static_cast<long double>(uli.QuadPart - unixEpochOffset) / static_cast<long double>(hundredNanosecondsPerSecond));
                        return seconds;
                    }
                }
            }

            return std::nullopt;
        }

        HRESULT hr = copy.ChangeType(VT_R8);
        if (FAILED(hr))
        {
            if (copy.vt == VT_BSTR)
            {
                bool success = false;
                const double parsed = ParseWideToDouble(BstrToWString(copy.bstrVal), success);
                if (success)
                {
                    return parsed;
                }
            }

            return std::nullopt;
        }

        if (std::isnan(copy.dblVal) || std::isinf(copy.dblVal))
        {
            return std::nullopt;
        }

        return copy.dblVal;
    }

    std::optional<long> VariantToLong(const VARIANT& value)
    {
        if (value.vt == VT_EMPTY || value.vt == VT_NULL || value.vt == VT_ERROR)
        {
            return std::nullopt;
        }

        CComVariant copy(value);
        HRESULT hr = copy.ChangeType(VT_I4);
        if (FAILED(hr))
        {
            if (copy.vt == VT_BSTR)
            {
                bool success = false;
                const long parsed = ParseWideToLong(BstrToWString(copy.bstrVal), success);
                if (success)
                {
                    return parsed;
                }
            }

            return std::nullopt;
        }

        return copy.lVal;
    }

    bool VariantHasValue(const VARIANT& value)
    {
        if (value.vt == VT_ERROR && value.scode == DISP_E_PARAMNOTFOUND)
        {
            return false;
        }

        return value.vt != VT_EMPTY && value.vt != VT_NULL;
    }
}
