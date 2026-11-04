#pragma once

#include "common/KmsOptions.h"

#include <cstdint>
#include <string_view>

namespace winactivate {

enum class HelperCommand : std::uint32_t { ActivateKms = 1, ActivateExisting = 2 };
inline constexpr std::uint32_t kIpcMagic = 0x57414354;
inline constexpr std::uint32_t kIpcVersion = 1;

// Fixed, bounded, versioned frames. No private product keys in this protocol.
// bytes is the frame length; both sides also check the actual pipe message length.
struct HelperRequest {
    std::uint32_t magic{kIpcMagic};
    std::uint32_t bytes{sizeof(HelperRequest)};
    std::uint32_t version{kIpcVersion};
    HelperCommand command{HelperCommand::ActivateKms};
    std::uint32_t port{1688};
    wchar_t host[254]{};
};

struct HelperResponse {
    std::uint32_t magic{kIpcMagic};
    std::uint32_t bytes{sizeof(HelperResponse)};
    std::uint32_t version{kIpcVersion};
    std::uint32_t stage{};
    std::uint32_t flags{}; // error, HRESULT present, ReturnValue present, verified
    std::int32_t hresult{};
    std::uint32_t wmiReturnValue{};
    wchar_t message[256]{};
};

static_assert(sizeof(HelperRequest) < 1024);
static_assert(sizeof(HelperResponse) < 1024);

[[nodiscard]] bool ValidateRequest(const HelperRequest& request, std::size_t actualBytes);
[[nodiscard]] bool ValidateResponse(const HelperResponse& response, std::size_t actualBytes);
[[nodiscard]] HelperResponse MakeResponse(const ActivationResult& result);
[[nodiscard]] std::optional<ActivationError> ResponseError(const HelperResponse& response);
[[nodiscard]] bool IsValidPipeName(std::wstring_view name) noexcept;

}  // namespace winactivate
