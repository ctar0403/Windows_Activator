#include "core/WmiConnection.h"

#include <objbase.h>

namespace winactivate {
namespace {

class UniqueBstr final {
public:
    explicit UniqueBstr(const wchar_t* text) : value_(SysAllocString(text)) {}
    explicit UniqueBstr(std::wstring_view text)
        : value_(SysAllocStringLen(text.data(), static_cast<UINT>(text.size()))) {}
    ~UniqueBstr() { SysFreeString(value_); }

    UniqueBstr(const UniqueBstr&) = delete;
    UniqueBstr& operator=(const UniqueBstr&) = delete;

    [[nodiscard]] BSTR get() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }

private:
    BSTR value_{};
};

}  // namespace

HRESULT InitializeProcessComSecurity() noexcept {
    const HRESULT result = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    return result == RPC_E_TOO_LATE ? S_OK : result;
}

WmiConnection::WmiConnection() {
    status_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(status_)) {
        return;
    }
    comInitialized_ = true;

    Microsoft::WRL::ComPtr<IWbemLocator> locator;
    status_ = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&locator));
    if (FAILED(status_)) {
        return;
    }

    UniqueBstr nameSpace(L"ROOT\\CIMV2");
    if (!nameSpace.valid()) {
        status_ = E_OUTOFMEMORY;
        return;
    }
    status_ = locator->ConnectServer(nameSpace.get(), nullptr, nullptr, nullptr, 0,
                                     nullptr, nullptr, &services_);
    if (FAILED(status_)) {
        return;
    }

    status_ = CoSetProxyBlanket(
        services_.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
}

WmiConnection::~WmiConnection() {
    services_.Reset();
    if (comInitialized_) {
        CoUninitialize();
    }
}

HRESULT WmiConnection::Query(
    std::wstring_view wql,
    std::vector<Microsoft::WRL::ComPtr<IWbemClassObject>>& results) const {
    if (FAILED(status_)) {
        return status_;
    }

    UniqueBstr language(L"WQL");
    UniqueBstr query(wql);
    if (!language.valid() || !query.valid()) {
        return E_OUTOFMEMORY;
    }

    Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;
    HRESULT result = services_->ExecQuery(
        language.get(), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    if (FAILED(result)) {
        return result;
    }

    const ULONGLONG deadline = GetTickCount64() + 60000;
    for (;;) {
        Microsoft::WRL::ComPtr<IWbemClassObject> object;
        ULONG returned = 0;
        result = enumerator->Next(10000, 1, &object, &returned);
        if (FAILED(result)) {
            return result;
        }
        if (result == WBEM_S_TIMEDOUT) {
            if (GetTickCount64() >= deadline) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            continue;
        }
        if (returned == 0) {
            return S_OK;
        }
        results.push_back(std::move(object));
    }
}

BackendCallResult WmiConnection::CallMethod(
    std::wstring_view path, const wchar_t* method,
    const wchar_t* parameter, const VARIANT* value) const {
    auto failure = [](HRESULT hr) {
        return BackendCallResult{false, static_cast<std::int32_t>(hr), {}};
    };
    if (FAILED(status_)) return failure(status_);
    UniqueBstr objectPath(path);
    UniqueBstr methodName(method);
    if (!objectPath.valid() || !methodName.valid()) return failure(E_OUTOFMEMORY);
    Microsoft::WRL::ComPtr<IWbemClassObject> input;
    HRESULT hr = S_OK;
    if (parameter != nullptr && value != nullptr) {
        Microsoft::WRL::ComPtr<IWbemClassObject> object, classObject, definition;
        hr = services_->GetObject(objectPath.get(), 0, nullptr, &object, nullptr);
        // GetMethod requires a CIM class definition, not an instance pointer.
        // ExecMethod below still targets the selected instance path.
        VARIANT className;
        VariantInit(&className);
        if (SUCCEEDED(hr)) hr = object->Get(L"__CLASS", 0, &className, nullptr, nullptr);
        if (SUCCEEDED(hr) && (className.vt != VT_BSTR || className.bstrVal == nullptr)) {
            hr = WBEM_E_TYPE_MISMATCH;
        }
        if (SUCCEEDED(hr)) hr = services_->GetObject(className.bstrVal, 0, nullptr, &classObject, nullptr);
        VariantClear(&className);
        if (SUCCEEDED(hr)) hr = classObject->GetMethod(method, 0, &definition, nullptr);
        if (SUCCEEDED(hr) && !definition) hr = WBEM_E_INVALID_METHOD_PARAMETERS;
        if (SUCCEEDED(hr)) hr = definition->SpawnInstance(0, &input);
        if (SUCCEEDED(hr)) hr = input->Put(parameter, 0, const_cast<VARIANT*>(value), 0);
        if (FAILED(hr)) return failure(hr);
    }
    Microsoft::WRL::ComPtr<IWbemCallResult> pending;
    hr = services_->ExecMethod(objectPath.get(), methodName.get(),
        WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, input.Get(), nullptr, &pending);
    if (FAILED(hr)) return failure(hr);
    if (!pending) return failure(E_UNEXPECTED);
    Microsoft::WRL::ComPtr<IWbemClassObject> output;
    hr = pending->GetResultObject(90000, &output);
    if (hr == WBEM_S_TIMEDOUT) return failure(HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    if (FAILED(hr)) return failure(hr);
    if (!output) return failure(E_UNEXPECTED);
    VARIANT code;
    VariantInit(&code);
    hr = output->Get(L"ReturnValue", 0, &code, nullptr, nullptr);
    if (FAILED(hr)) { VariantClear(&code); return failure(hr); }
    if (code.vt != VT_I4 && code.vt != VT_UI4) {
        VariantClear(&code);
        return failure(WBEM_E_TYPE_MISMATCH);
    }
    const auto result = code.vt == VT_I4 ? static_cast<std::uint32_t>(code.lVal) : code.ulVal;
    VariantClear(&code);
    return {result == 0, {}, result};
}

}  // namespace winactivate

