#pragma once

#include "common/Types.h"

#include <string>
#include <string_view>

namespace winactivate {

struct KmsOptions {
    // Empty means preserve Windows' current configuration / DNS discovery.
    std::wstring host;
    std::uint32_t port{1688};
};

[[nodiscard]] bool IsValidKmsHost(std::wstring_view host) noexcept;
[[nodiscard]] bool IsKmsClient(const LicenseRecord& record);
[[nodiscard]] std::wstring_view KmsClientSetupKey(std::uint32_t sku) noexcept;
[[nodiscard]] std::wstring KmsRemainingText(const LicenseRecord& record);

}  // namespace winactivate
