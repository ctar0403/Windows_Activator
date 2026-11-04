#include "core/LicenseActivator.h"
#include "core/LicenseReader.h"
#include "core/WmiConnection.h"

namespace winactivate {
namespace {

BackendCallResult ServiceMethod(const wchar_t* method,
                                const wchar_t* parameter = nullptr,
                                const VARIANT* value = nullptr) {
    WmiConnection connection;
    std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>> services;
    HRESULT hr = connection.Query(L"SELECT * FROM SoftwareLicensingService", services);
    if (FAILED(hr) || services.size() != 1) {
        return {false, static_cast<std::int32_t>(FAILED(hr) ? hr : static_cast<HRESULT>(WBEM_E_FAILED)), {}};
    }
    VARIANT path;
    VariantInit(&path);
    hr = services.front()->Get(L"__PATH", 0, &path, nullptr, nullptr);
    BackendCallResult result{false, static_cast<std::int32_t>(FAILED(hr) ? hr : static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH)), {}};
    if (SUCCEEDED(hr) && path.vt == VT_BSTR && path.bstrVal != nullptr) {
        result = connection.CallMethod(path.bstrVal, method, parameter, value);
    }
    VariantClear(&path);
    return result;
}

BackendCallResult StringMethod(std::wstring_view path, const wchar_t* method,
                               const wchar_t* parameter, std::wstring_view text) {
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    if (!value.bstrVal) return {false, static_cast<std::int32_t>(E_OUTOFMEMORY), {}};
    BackendCallResult result;
    if (path.empty()) result = ServiceMethod(method, parameter, &value);
    else {
        WmiConnection connection;
        result = connection.CallMethod(path, method, parameter, &value);
    }
    SecureZeroMemory(value.bstrVal, SysStringByteLen(value.bstrVal));
    VariantClear(&value);
    return result;
}

}  // namespace

SnapshotQueryResult WmiActivationBackend::QuerySnapshot() {
    WmiLicensingBackend reader;
    return reader.QuerySnapshot();
}

BackendCallResult WmiActivationBackend::InstallProductKey(std::wstring_view key) {
    return StringMethod({}, L"InstallProductKey", L"ProductKey", key);
}

BackendCallResult WmiActivationBackend::ActivateProduct(std::wstring_view path) {
    WmiConnection connection;
    return connection.CallMethod(path, L"Activate");
}

BackendCallResult WmiActivationBackend::RefreshLicenseStatus() {
    return ServiceMethod(L"RefreshLicenseStatus");
}

BackendCallResult WmiActivationBackend::ConfigureKms(
    std::wstring_view path, const KmsOptions& options) {
    if (options.host.empty() || !IsValidKmsHost(options.host) ||
        options.port == 0 || options.port > 65535) {
        return {false, static_cast<std::int32_t>(E_INVALIDARG), {}};
    }
    // Per-product configuration: do not affect Office or another Windows SKU.
    auto result = StringMethod(path, L"SetKeyManagementServiceMachine", L"MachineName", options.host);
    if (!result.succeeded) return result;
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_I4; // WMI represents CIM_UINT32 parameters as VT_I4.
    value.lVal = static_cast<LONG>(options.port);
    WmiConnection connection;
    return connection.CallMethod(path, L"SetKeyManagementServicePort", L"PortNumber", &value);
}

}  // namespace winactivate
