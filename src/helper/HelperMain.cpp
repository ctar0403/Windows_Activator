#include "common/HelperTransport.h"
#include "common/WinHandle.h"
#include "core/ActivationWorkflow.h"
#include "core/LicenseActivator.h"
#include "core/WmiConnection.h"

#include <shellapi.h>
#include <cerrno>
#include <cwchar>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace winactivate;
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return 1;
    if (count != 3) { LocalFree(arguments); return 1; }
    const std::wstring name = arguments[1];
    wchar_t* end = nullptr;
    errno = 0;
    const auto parentId = std::wcstoul(arguments[2], &end, 10);
    const bool validId = errno == 0 && parentId != 0 && end != arguments[2] && *end == 0;
    LocalFree(arguments);
    if (!validId || !IsValidPipeName(name)) return 1;

    UniqueHandle token;
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) return 1;
    token.reset(rawToken);
    TOKEN_ELEVATION elevation{};
    DWORD bytes = 0;
    if (!GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &bytes) ||
        !elevation.TokenIsElevated) return 1;

    UniqueHandle parent(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, parentId));
    DWORD parentSession = 0, ownSession = 0;
    if (!parent || !ProcessMatchesApplication(parent.get(), L"WinActivate.exe") ||
        !ProcessIdToSessionId(parentId, &parentSession) ||
        !ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) ||
        parentSession != ownSession || WaitForSingleObject(parent.get(), 0) != WAIT_TIMEOUT) return 1;

    if (!WaitNamedPipeW(name.c_str(), 45000)) return 1;
    UniqueHandle pipe(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
    if (!pipe) return 1;
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) return 1;
    ULONG serverId = 0;
    if (!GetNamedPipeServerProcessId(pipe.get(), &serverId) || serverId != parentId ||
        WaitForSingleObject(parent.get(), 0) != WAIT_TIMEOUT) return 1;

    HelperRequest request{};
    if (FAILED(TransferFrame(pipe.get(), &request, sizeof(request), false, 10000)) ||
        !ValidateRequest(request, sizeof(request))) return 1;

    ActivationResult result;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const HRESULT security = SUCCEEDED(initialized) ? InitializeProcessComSecurity() : initialized;
    if (FAILED(security)) {
        result.error = ActivationError{ActivationStage::Query,
            static_cast<std::int32_t>(security), {}, L"The helper could not initialize COM security.", {}};
    } else {
        try {
            WmiActivationBackend backend;
            if (request.command == HelperCommand::ActivateKms) {
                result = ActivateWithKms(backend, {request.host, request.port});
            } else {
                ActivationWorkflow workflow(backend);
                result = workflow.ActivateExisting();
            }
        } catch (...) {
            result.error = ActivationError{ActivationStage::Activate, {}, {},
                L"The helper stopped unexpectedly. Refresh Windows status before retrying.", {}};
        }
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    auto response = MakeResponse(result);
    const auto sent = TransferFrame(pipe.get(), &response, sizeof(response), true, 10000);
    return SUCCEEDED(sent) ? 0 : 1;
}
