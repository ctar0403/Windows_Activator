#include "common/HelperTransport.h"
#include "common/WinHandle.h"

#include <sddl.h>
#include <shellapi.h>
#include <objbase.h>

#include <vector>

namespace winactivate {

std::wstring ApplicationDirectory() {
    std::wstring path(32768, 0);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    const auto separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator + 1);
}

bool ProcessMatchesApplication(HANDLE process, const wchar_t* filename) {
    const auto directory = ApplicationDirectory();
    if (directory.empty()) return false;
    std::wstring path(32768, 0);
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length)) return false;
    path.resize(length);
    const auto expected = directory + filename;
    return CompareStringOrdinal(path.c_str(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL;
}

HRESULT CompletePipeOperation(HANDLE pipe, OVERLAPPED& operation,
                             BOOL started, DWORD timeoutMs, DWORD& transferred) {
    if (!started) {
        const auto error = GetLastError();
        if (error != ERROR_IO_PENDING) return HRESULT_FROM_WIN32(error);
        const DWORD wait = WaitForSingleObject(operation.hEvent, timeoutMs);
        if (wait != WAIT_OBJECT_0) {
            const DWORD waitError = wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
            CancelIoEx(pipe, &operation);
            // Drain before releasing the OVERLAPPED or its buffer.
            GetOverlappedResult(pipe, &operation, &transferred, TRUE);
            return HRESULT_FROM_WIN32(waitError);
        }
    }
    if (!GetOverlappedResult(pipe, &operation, &transferred, FALSE)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}

HRESULT TransferFrame(HANDLE pipe, void* frame, DWORD bytes,
                      bool writing, DWORD timeoutMs) {
    if (bytes >= 1024) return E_INVALIDARG;
    UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return HRESULT_FROM_WIN32(GetLastError());
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    DWORD transferred = 0;
    const BOOL started = writing ? WriteFile(pipe, frame, bytes, nullptr, &operation)
                                 : ReadFile(pipe, frame, bytes, nullptr, &operation);
    const auto hr = CompletePipeOperation(pipe, operation, started, timeoutMs, transferred);
    if (FAILED(hr)) return hr;
    return transferred == bytes ? S_OK : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
}

std::optional<ActivationError> RunElevatedHelper(const HelperRequest& request, HWND owner) {
    auto failure = [](HRESULT hr, ActivationStage stage = ActivationStage::Ipc) {
        return std::optional<ActivationError>(ActivationError{stage, static_cast<std::int32_t>(hr), {},
            hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)
                ? L"Administrator approval was cancelled. No activation request was sent."
                : L"The activation helper could not finish. Changes may be incomplete; refresh status before retrying.", {}});
    };
    if (!ValidateRequest(request, sizeof(request))) return failure(E_INVALIDARG);
    const auto directory = ApplicationDirectory();
    if (directory.empty()) return failure(E_FAIL);
    const auto executable = directory + L"WinActivateHelper.exe";
    if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return failure(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), ActivationStage::Elevate);
    }
    GUID guid{};
    HRESULT hr = CoCreateGuid(&guid);
    if (FAILED(hr)) return failure(hr);
    wchar_t guidText[40]{};
    if (StringFromGUID2(guid, guidText, 40) == 0) return failure(E_FAIL);
    const std::wstring name = std::wstring(L"\\\\.\\pipe\\WinActivate-") + guidText;

    UniqueHandle token;
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) return failure(HRESULT_FROM_WIN32(GetLastError()));
    token.reset(rawToken);
    DWORD tokenBytes = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &tokenBytes);
    if (tokenBytes == 0) return failure(HRESULT_FROM_WIN32(GetLastError()));
    std::vector<BYTE> tokenData(tokenBytes);
    if (!GetTokenInformation(token.get(), TokenUser, tokenData.data(), tokenBytes, &tokenBytes)) return failure(HRESULT_FROM_WIN32(GetLastError()));
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(tokenData.data())->User.Sid, &sid)) return failure(HRESULT_FROM_WIN32(GetLastError()));
    // Administrators may connect under UAC's alternate-account elevation.
    // Their mask excludes FILE_CREATE_PIPE_INSTANCE. Peer PID checks follow.
    const std::wstring sddl = std::wstring(L"D:P(A;;GA;;;") + sid + L")(A;;0x0012019b;;;BA)";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return failure(HRESULT_FROM_WIN32(GetLastError()));
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    UniqueHandle pipe(CreateNamedPipeW(name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 1024, 1024, 0, &attributes));
    const DWORD pipeError = GetLastError();
    LocalFree(descriptor);
    if (!pipe) return failure(HRESULT_FROM_WIN32(pipeError));

    const std::wstring arguments = L"\"" + name + L"\" " + std::to_wstring(GetCurrentProcessId());
    SHELLEXECUTEINFOW launch{};
    launch.cbSize = sizeof(launch);
    launch.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    launch.hwnd = owner;
    launch.lpVerb = L"runas";
    launch.lpFile = executable.c_str();
    launch.lpParameters = arguments.c_str();
    launch.lpDirectory = directory.c_str();
    launch.nShow = SW_HIDE;
    if (!ShellExecuteExW(&launch)) return failure(HRESULT_FROM_WIN32(GetLastError()), ActivationStage::Elevate);
    UniqueHandle helper(launch.hProcess);
    if (!helper) return failure(E_FAIL, ActivationStage::Elevate);

    UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event) return failure(HRESULT_FROM_WIN32(GetLastError()));
    OVERLAPPED connect{};
    connect.hEvent = event.get();
    DWORD ignored = 0;
    const BOOL started = ConnectNamedPipe(pipe.get(), &connect);
    const DWORD connectError = started ? ERROR_SUCCESS : GetLastError();
    if (connectError != ERROR_PIPE_CONNECTED) {
        // CompletePipeOperation must see the original last error.
        SetLastError(connectError);
        hr = CompletePipeOperation(pipe.get(), connect, started, 45000, ignored);
        if (FAILED(hr)) return failure(hr);
    }
    ULONG clientId = 0;
    if (!GetNamedPipeClientProcessId(pipe.get(), &clientId) ||
        clientId != GetProcessId(helper.get()) ||
        WaitForSingleObject(helper.get(), 0) != WAIT_TIMEOUT ||
        !ProcessMatchesApplication(helper.get(), L"WinActivateHelper.exe")) return failure(E_ACCESSDENIED);
    auto outgoing = request;
    hr = TransferFrame(pipe.get(), &outgoing, sizeof(outgoing), true, 10000);
    if (FAILED(hr)) return failure(hr);
    HelperResponse response{};
    hr = TransferFrame(pipe.get(), &response, sizeof(response), false, 240000);
    if (FAILED(hr)) return failure(hr);
    if (!ValidateResponse(response, sizeof(response))) return failure(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
    return ResponseError(response);
}

}  // namespace winactivate
