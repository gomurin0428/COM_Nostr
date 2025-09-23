#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include <atlbase.h>
#include <atlcom.h>

namespace com::nostr::native
{
    std::wstring BstrToWString(BSTR value);

    std::string WideToUtf8(const std::wstring& value);

    std::wstring Utf8ToWide(std::string_view value);

    std::string BstrToUtf8(BSTR value);

    ATL::CComBSTR Utf8ToBstr(std::string_view value);

    HRESULT SafeArrayToStringVector(SAFEARRAY* array, std::vector<ATL::CComBSTR>& output);

    HRESULT SafeArrayToLongVector(SAFEARRAY* array, std::vector<long>& output);

    HRESULT SafeArrayToVariantVector(SAFEARRAY* array, std::vector<ATL::CComVariant>& output);

    HRESULT SafeArrayToTagMatrix(SAFEARRAY* array, std::vector<std::vector<ATL::CComBSTR>>& output);

    HRESULT CreateSafeArrayFromStrings(const std::vector<ATL::CComBSTR>& values, SAFEARRAY** result);

    HRESULT CreateSafeArrayFromLongs(const std::vector<long>& values, SAFEARRAY** result);

    HRESULT CreateSafeArrayFromVariants(const std::vector<ATL::CComVariant>& values, SAFEARRAY** result);

    HRESULT CreateSafeArrayFromTagMatrix(const std::vector<std::vector<ATL::CComBSTR>>& tags, SAFEARRAY** result);

    HRESULT CopyVariant(const VARIANT& source, VARIANT* destination);

    std::optional<double> VariantToDouble(const VARIANT& value);

    std::optional<long> VariantToLong(const VARIANT& value);

    bool VariantHasValue(const VARIANT& value);
}
