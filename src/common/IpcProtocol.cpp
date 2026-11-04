#include "common/IpcProtocol.h"

#include <algorithm>
#include <iterator>

namespace winactivate {
namespace {
template<std::size_t N>
bool CanonicalString(const wchar_t (&text)[N]) {
    const auto end = std::find(std::begin(text), std::end(text), L'\0');
    return end != std::end(text) &&
        std::all_of(end, std::end(text), [](wchar_t c) { return c == 0; });
}
}

bool ValidateRequest(const HelperRequest& request, std::size_t actualBytes) {
    if (actualBytes != sizeof(request) || request.bytes != sizeof(request) ||
        request.magic != kIpcMagic || request.version != kIpcVersion ||
        !CanonicalString(request.host)) return false;
    if (request.command != HelperCommand::ActivateKms &&
        request.command != HelperCommand::ActivateExisting) return false;
    if (request.port == 0 || request.port > 65535 || !IsValidKmsHost(request.host)) return false;
    if ((request.host[0] == 0 || request.command == HelperCommand::ActivateExisting) &&
        request.port != 1688) return false;
    return request.command != HelperCommand::ActivateExisting || request.host[0] == 0;
}

bool ValidateResponse(const HelperResponse& response, std::size_t actualBytes) {
    return actualBytes == sizeof(response) && response.bytes == sizeof(response) &&
        response.magic == kIpcMagic && response.version == kIpcVersion &&
        response.stage <= static_cast<std::uint32_t>(ActivationStage::Ipc) &&
        (response.flags & ~15U) == 0 && CanonicalString(response.message);
}

HelperResponse MakeResponse(const ActivationResult& result) {
    HelperResponse response{};
    if (result.verifiedActivated) response.flags |= 8;
    if (result.error) {
        response.flags |= 1;
        response.stage = static_cast<std::uint32_t>(result.error->stage);
        if (result.error->hresult) { response.flags |= 2; response.hresult = *result.error->hresult; }
        if (result.error->wmiReturnValue) { response.flags |= 4; response.wmiReturnValue = *result.error->wmiReturnValue; }
        std::copy_n(result.error->message.begin(),
            (std::min)(result.error->message.size(), std::size(response.message) - 1), response.message);
    }
    return response;
}

std::optional<ActivationError> ResponseError(const HelperResponse& response) {
    if (!(response.flags & 1)) return {};
    ActivationError error;
    error.stage = static_cast<ActivationStage>(response.stage);
    if (response.flags & 2) error.hresult = response.hresult;
    if (response.flags & 4) error.wmiReturnValue = response.wmiReturnValue;
    error.message = response.message;
    return error;
}

bool IsValidPipeName(std::wstring_view name) noexcept {
    constexpr std::wstring_view prefix = L"\\\\.\\pipe\\WinActivate-";
    if (name.size() != prefix.size() + 38 || name.substr(0, prefix.size()) != prefix) return false;
    const auto guid = name.substr(prefix.size());
    if (guid.front() != L'{' || guid.back() != L'}') return false;
    for (std::size_t i = 1; i < 37; ++i) {
        if (i == 9 || i == 14 || i == 19 || i == 24) {
            if (guid[i] != L'-') return false;
        } else if (!((guid[i] >= L'0' && guid[i] <= L'9') ||
                     (guid[i] >= L'a' && guid[i] <= L'f') ||
                     (guid[i] >= L'A' && guid[i] <= L'F'))) return false;
    }
    return true;
}

}  // namespace winactivate
