#pragma once

#include "core/KmsWorkflow.h"

namespace winactivate {

class WmiActivationBackend final : public IKmsBackend {
public:
    [[nodiscard]] SnapshotQueryResult QuerySnapshot() override;
    [[nodiscard]] BackendCallResult InstallProductKey(std::wstring_view key) override;
    [[nodiscard]] BackendCallResult ActivateProduct(std::wstring_view path) override;
    [[nodiscard]] BackendCallResult RefreshLicenseStatus() override;
    [[nodiscard]] BackendCallResult ConfigureKms(
        std::wstring_view path, const KmsOptions& options) override;
};

}  // namespace winactivate
