#pragma once

#include "common/KmsOptions.h"
#include "common/LicensingBackend.h"

namespace winactivate {

class IKmsBackend : public IActivationBackend {
public:
    [[nodiscard]] virtual BackendCallResult ConfigureKms(
        std::wstring_view productPath, const KmsOptions& options) = 0;
};

// No host mutations unless a host was explicitly supplied by the user.
[[nodiscard]] ActivationResult ActivateWithKms(IKmsBackend& backend,
                                               const KmsOptions& options);

}  // namespace winactivate
