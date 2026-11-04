#include "TestFramework.h"
#include "common/IpcProtocol.h"

#include <algorithm>
#include <iterator>

using namespace winactivate;

TEST_CASE("helper protocol rejects wrong length magic version commands and ports") {
    HelperRequest request{};
    REQUIRE(ValidateRequest(request, sizeof(request)));
    REQUIRE(!ValidateRequest(request, sizeof(request) - 1));
    REQUIRE(!ValidateRequest(request, sizeof(request) + 1));
    request.version = 99;
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.magic = 0;
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.command = static_cast<HelperCommand>(99);
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.bytes = 1024;
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.port = 0;
    REQUIRE(!ValidateRequest(request, sizeof(request)));
}

TEST_CASE("helper protocol rejects unterminated and hidden hostname data") {
    HelperRequest request{};
    std::fill(std::begin(request.host), std::end(request.host), L'a');
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.host[2] = L'x';
    REQUIRE(!ValidateRequest(request, sizeof(request)));
    request = {};
    request.command = HelperCommand::ActivateExisting;
    request.host[0] = L'a';
    REQUIRE(!ValidateRequest(request, sizeof(request)));
}

TEST_CASE("helper response preserves both transport and provider errors") {
    ActivationResult result;
    result.error = ActivationError{ActivationStage::Activate,
        static_cast<std::int32_t>(0x80070005), 0xC004F074, L"Company KMS unreachable", {}};
    auto response = MakeResponse(result);
    REQUIRE(ValidateResponse(response, sizeof(response)));
    const auto error = ResponseError(response);
    REQUIRE(error);
    REQUIRE(error->hresult == result.error->hresult);
    REQUIRE(error->wmiReturnValue == result.error->wmiReturnValue);
    REQUIRE(error->message == result.error->message);
    response.stage = 500;
    REQUIRE(!ValidateResponse(response, sizeof(response)));
}

TEST_CASE("helper response bounds long messages and rejects unknown flags") {
    ActivationResult result;
    result.error = ActivationError{ActivationStage::Ipc, {}, {}, std::wstring(5000, L'x'), {}};
    auto response = MakeResponse(result);
    REQUIRE(ValidateResponse(response, sizeof(response)));
    REQUIRE(ResponseError(response)->message.size() == 255);
    response.flags = 0x80000000;
    REQUIRE(!ValidateResponse(response, sizeof(response)));
    REQUIRE(!ValidateResponse(response, sizeof(response) + 1));
}

TEST_CASE("helper accepts only local unpredictable application pipe names") {
    REQUIRE(IsValidPipeName(L"\\\\.\\pipe\\WinActivate-{01234567-89AB-CDEF-0123-456789ABCDEF}"));
    REQUIRE(!IsValidPipeName(L"\\\\remote\\pipe\\WinActivate-{01234567-89AB-CDEF-0123-456789ABCDEF}"));
    REQUIRE(!IsValidPipeName(L"\\\\.\\pipe\\arbitrary"));
    REQUIRE(!IsValidPipeName(L"\\\\.\\pipe\\WinActivate-{Z1234567-89AB-CDEF-0123-456789ABCDEF}"));
}
