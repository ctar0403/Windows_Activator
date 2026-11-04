#include "core/KmsWorkflow.h"

namespace winactivate {
namespace {

ActivationError Error(ActivationStage stage, std::wstring message,
                      const BackendCallResult& call = {}) {
    return {stage, call.hresult, call.wmiReturnValue, std::move(message),
        L"https://learn.microsoft.com/windows-server/get-started/activation-troubleshoot-kms-general"};
}

ActivationResult Finish(IKmsBackend& backend, ActivationResult result,
                        std::optional<ActivationError> error = {}) {
    const auto refresh = backend.RefreshLicenseStatus();
    if (!refresh.succeeded && !error) {
        error = Error(ActivationStage::Refresh, L"Windows could not refresh licensing status.", refresh);
    }
    auto final = backend.QuerySnapshot();
    result.finalSnapshot = std::move(final.snapshot);
    result.verifiedActivated = !final.error && !result.finalSnapshot.selectionIsAmbiguous &&
        result.finalSnapshot.os.isSupported && result.finalSnapshot.chosen &&
        result.finalSnapshot.chosen->licenseStatus == 1;
    if (final.error) result.error = std::move(final.error);
    else if (error) result.error = std::move(error);
    else if (!result.verifiedActivated) {
        result.error = Error(ActivationStage::Verify,
            L"Windows has not confirmed activation. Connect to the company network or VPN and contact IT.");
    }
    return result;
}

}  // namespace

ActivationResult ActivateWithKms(IKmsBackend& backend, const KmsOptions& options) {
    ActivationResult result;
    if (!IsValidKmsHost(options.host) || options.port == 0 || options.port > 65535 ||
        (options.host.empty() && options.port != 1688)) {
        result.error = Error(ActivationStage::Validate, L"Enter a hostname and a port from 1 to 65535, or leave the server blank for the current configuration.");
        return result;
    }
    auto initial = backend.QuerySnapshot();
    result.finalSnapshot = initial.snapshot;
    if (initial.error) {
        result.error = std::move(initial.error);
        return result;
    }
    if (!initial.snapshot.os.isSupported || initial.snapshot.selectionIsAmbiguous ||
        !initial.snapshot.chosen) {
        result.error = Error(ActivationStage::Validate,
            L"Windows must be supported and its installed license identified unambiguously before activation.");
        return result;
    }
    auto product = *initial.snapshot.chosen;
    if (product.licenseStatus == 1 && !IsKmsClient(product)) {
        // Never replace a working retail/OEM/MAK entitlement for a KMS request.
        result.verifiedActivated = true;
        return result;
    }
    if (!IsKmsClient(product) || !product.partialKey) {
        const auto setupKey = KmsClientSetupKey(initial.snapshot.os.sku);
        if (setupKey.empty()) {
            result.error = Error(ActivationStage::Validate,
                L"This edition has no supported KMS setup key. Windows Home cannot use KMS. Contact IT.");
            return result;
        }
        result.installAttempted = true;
        const auto install = backend.InstallProductKey(setupKey);
        result.installReportedSuccess = install.succeeded;
        if (!install.succeeded) return Finish(backend, std::move(result),
            Error(ActivationStage::Install, L"Windows rejected the edition's public KMS client setup key.", install));
        const auto refreshed = backend.RefreshLicenseStatus();
        if (!refreshed.succeeded) return Finish(backend, std::move(result),
            Error(ActivationStage::Refresh, L"Windows could not refresh the installed KMS client status.", refreshed));
        auto installed = backend.QuerySnapshot();
        if (installed.error) return Finish(backend, std::move(result), std::move(installed.error));
        if (!installed.snapshot.os.isSupported ||
            installed.snapshot.os.sku != initial.snapshot.os.sku ||
            installed.snapshot.selectionIsAmbiguous || !installed.snapshot.chosen ||
            !IsKmsClient(*installed.snapshot.chosen) || !installed.snapshot.chosen->partialKey) {
            return Finish(backend, std::move(result), Error(ActivationStage::Verify,
                L"The installed KMS client could not be identified safely; activation was stopped."));
        }
        product = *installed.snapshot.chosen;
    }
    if (!options.host.empty()) {
        const auto configure = backend.ConfigureKms(product.wmiObjectPath, options);
        if (!configure.succeeded) return Finish(backend, std::move(result),
            Error(ActivationStage::Activate,
                L"Windows could not configure the company KMS server. Configuration may be partially changed; contact IT.", configure));
    }
    result.activationAttempted = true;
    const auto activation = backend.ActivateProduct(product.wmiObjectPath);
    result.activationReportedSuccess = activation.succeeded;
    if (!activation.succeeded) return Finish(backend, std::move(result),
        Error(ActivationStage::Activate, L"KMS activation failed. Check the company network or VPN, server name, and licensing with IT.", activation));
    return Finish(backend, std::move(result));
}

}  // namespace winactivate
