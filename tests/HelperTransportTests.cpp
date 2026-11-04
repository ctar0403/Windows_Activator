#include "TestFramework.h"
#include "common/HelperTransport.h"
#include "common/WinHandle.h"

#include <array>
#include <objbase.h>

namespace {
struct Pipes {
    winactivate::UniqueHandle server;
    winactivate::UniqueHandle client;
};

Pipes ConnectLocalTestPipe() {
    GUID guid{};
    REQUIRE(SUCCEEDED(CoCreateGuid(&guid)));
    wchar_t text[40]{};
    REQUIRE(StringFromGUID2(guid, text, 40) != 0);
    const std::wstring name = std::wstring(L"\\\\.\\pipe\\WinActivate-Test-") + text;
    Pipes pipes;
    pipes.server.reset(CreateNamedPipeW(name.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 1024, 1024, 0, nullptr));
    REQUIRE(pipes.server);
    winactivate::UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    REQUIRE(event);
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    const BOOL started = ConnectNamedPipe(pipes.server.get(), &operation);
    const auto error = started ? ERROR_SUCCESS : GetLastError();
    pipes.client.reset(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
    if (!pipes.client) {
        CancelIoEx(pipes.server.get(), &operation);
        DWORD ignored = 0;
        GetOverlappedResult(pipes.server.get(), &operation, &ignored, TRUE);
        REQUIRE(pipes.client);
    }
    DWORD transferred = 0;
    SetLastError(error);
    REQUIRE(SUCCEEDED(winactivate::CompletePipeOperation(
        pipes.server.get(), operation, started, 2000, transferred)));
    DWORD mode = PIPE_READMODE_MESSAGE;
    REQUIRE(SetNamedPipeHandleState(pipes.client.get(), &mode, nullptr, nullptr));
    return pipes;
}
}

TEST_CASE("local pipe transports bounded requests and responses without elevation") {
    auto pipes = ConnectLocalTestPipe();
    winactivate::HelperRequest sent{}, received{};
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.client.get(), &sent, sizeof(sent), true, 2000)));
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.server.get(), &received, sizeof(received), false, 2000)));
    REQUIRE(winactivate::ValidateRequest(received, sizeof(received)));
    winactivate::HelperResponse response{}, answer{};
    response.flags = 8;
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.server.get(), &response, sizeof(response), true, 2000)));
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.client.get(), &answer, sizeof(answer), false, 2000)));
    REQUIRE(winactivate::ValidateResponse(answer, sizeof(answer)));
    REQUIRE(answer.flags == 8);
    ULONG server = 0, client = 0;
    REQUIRE(GetNamedPipeServerProcessId(pipes.client.get(), &server));
    REQUIRE(GetNamedPipeClientProcessId(pipes.server.get(), &client));
    REQUIRE(server == GetCurrentProcessId());
    REQUIRE(client == GetCurrentProcessId());
}

TEST_CASE("local pipe timeout cancels pending IO and leaves the buffer safe") {
    auto pipes = ConnectLocalTestPipe();
    winactivate::HelperRequest request{};
    const auto hr = winactivate::TransferFrame(pipes.server.get(), &request, sizeof(request), false, 25);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT));
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.client.get(), &request, sizeof(request), true, 2000)));
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.server.get(), &request, sizeof(request), false, 2000)));
}

TEST_CASE("local pipe rejects oversized and truncated wire messages") {
    auto pipes = ConnectLocalTestPipe();
    std::array<BYTE, sizeof(winactivate::HelperRequest) + 8> oversized{};
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(pipes.client.get(), oversized.data(),
        static_cast<DWORD>(oversized.size()), true, 2000)));
    winactivate::HelperRequest received{};
    REQUIRE(FAILED(winactivate::TransferFrame(pipes.server.get(), &received, sizeof(received), false, 2000)));
    auto shortPipes = ConnectLocalTestPipe();
    DWORD tiny = 0;
    REQUIRE(SUCCEEDED(winactivate::TransferFrame(shortPipes.client.get(), &tiny, sizeof(tiny), true, 2000)));
    REQUIRE(FAILED(winactivate::TransferFrame(shortPipes.server.get(), &received, sizeof(received), false, 2000)));
}

TEST_CASE("process path verification rejects a different sibling executable") {
    REQUIRE(winactivate::ProcessMatchesApplication(GetCurrentProcess(), L"WinActivateTests.exe"));
    REQUIRE(!winactivate::ProcessMatchesApplication(GetCurrentProcess(), L"WinActivateHelper.exe"));
}
