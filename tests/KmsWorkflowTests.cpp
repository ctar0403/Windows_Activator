#include "TestFramework.h"
#include "common/KmsOptions.h"
#include "core/KmsWorkflow.h"
#include "core/MockLicensingBackend.h"
#include "core/OsDetector.h"
#include "core/ProductSelection.h"

namespace {
winactivate::SnapshotQueryResult Snapshot(unsigned status, bool kms = true) {
    winactivate::LicenseRecord record;
    record.applicationId = winactivate::kWindowsApplicationId;
    record.activationId = L"mock-id";
    record.name = L"Windows Professional";
    record.description = kms ? L"Windows, VOLUME_KMSCLIENT channel" : L"Windows, RETAIL channel";
    record.partialKey = L"ABCDE";
    record.wmiObjectPath = L"mock-kms-product";
    record.licenseStatus = status;
    return {winactivate::SelectApplicableProduct(
        winactivate::ClassifyWindows(L"10.0", L"26100", L"Windows 11 Pro", 48, 1), {record}), {}};
}
}

TEST_CASE("KMS host validation rejects commands URLs malformed labels and oversized hosts") {
    REQUIRE(winactivate::IsValidKmsHost(L""));
    REQUIRE(winactivate::IsValidKmsHost(L"kms.corp.example"));
    REQUIRE(winactivate::IsValidKmsHost(L"10.0.0.2"));
    for (const auto* value : {L"https://kms.example", L"kms:1688", L"kms & whoami", L"-kms", L"kms-", L"a..b", L"kms\n", L"kms."}) {
        REQUIRE(!winactivate::IsValidKmsHost(value));
    }
    REQUIRE(!winactivate::IsValidKmsHost(std::wstring(254, L'a')));
    REQUIRE(!winactivate::IsValidKmsHost(std::wstring(64, L'a') + L".example"));
}

TEST_CASE("KMS automatic configuration preserves the current server and verifies status") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(0), Snapshot(1)};
    const auto result = winactivate::ActivateWithKms(backend, {});
    REQUIRE(result.verifiedActivated);
    REQUIRE(!result.error);
    REQUIRE(backend.configureCalls == 0);
    REQUIRE(backend.installCalls == 0);
    REQUIRE(backend.activationCalls == 1);
}

TEST_CASE("KMS server is configured only when explicitly supplied") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(0), Snapshot(1)};
    const auto result = winactivate::ActivateWithKms(backend, {L"kms.corp.example", 1689});
    REQUIRE(result.verifiedActivated);
    REQUIRE(backend.configureCalls == 1);
    REQUIRE(backend.lastKmsOptions.port == 1689);
}

TEST_CASE("unactivated retail Pro becomes a KMS client before activation") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(0, false), Snapshot(0), Snapshot(1)};
    const auto result = winactivate::ActivateWithKms(backend, {});
    REQUIRE(result.verifiedActivated);
    REQUIRE(backend.installCalls == 1);
    REQUIRE(backend.receivedNormalizedKey);
    REQUIRE(backend.lastActivatedPath == L"mock-kms-product");
}

TEST_CASE("KMS never replaces working retail activation") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(1, false)};
    const auto result = winactivate::ActivateWithKms(backend, {L"kms.corp.example", 1688});
    REQUIRE(result.verifiedActivated);
    REQUIRE(backend.installCalls == 0);
    REQUIRE(backend.configureCalls == 0);
    REQUIRE(backend.activationCalls == 0);
}

TEST_CASE("already activated KMS clients request renewal") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(1), Snapshot(1)};
    const auto result = winactivate::ActivateWithKms(backend, {});
    REQUIRE(result.verifiedActivated);
    REQUIRE(backend.activationCalls == 1);
}

TEST_CASE("KMS does not activate after installation or configuration failure") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(0, false), Snapshot(0, false)};
    backend.installResult = {false, {}, 0xC004F050};
    const auto install = winactivate::ActivateWithKms(backend, {});
    REQUIRE(install.error);
    REQUIRE(backend.activationCalls == 0);
    REQUIRE(backend.queryCalls == 2);
    backend.snapshots = {Snapshot(0), Snapshot(0)};
    backend.configureResult = {false, {}, 0x80070005};
    const auto configure = winactivate::ActivateWithKms(backend, {L"kms.corp.example", 1688});
    REQUIRE(configure.error);
    REQUIRE(backend.activationCalls == 0);
}

TEST_CASE("KMS method success does not imply activation") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(0), Snapshot(0)};
    const auto result = winactivate::ActivateWithKms(backend, {});
    REQUIRE(result.activationReportedSuccess);
    REQUIRE(!result.verifiedActivated);
    REQUIRE(result.error);
}

TEST_CASE("KMS activation errors remain visible even when license is still active") {
    winactivate::MockLicensingBackend backend;
    backend.snapshots = {Snapshot(1), Snapshot(1)};
    backend.activationResult = {false, {}, 0xC004F074};
    const auto result = winactivate::ActivateWithKms(backend, {});
    REQUIRE(result.verifiedActivated);
    REQUIRE(result.error);
    REQUIRE(result.error->wmiReturnValue == 0xC004F074);
}

TEST_CASE("KMS refuses ambiguous unsupported or changed edition records") {
    for (int failure = 0; failure < 3; ++failure) {
        winactivate::MockLicensingBackend backend;
        auto snapshot = Snapshot(0);
        if (failure == 0) snapshot.snapshot.selectionIsAmbiguous = true;
        if (failure == 1) snapshot.snapshot.os.isSupported = false;
        if (failure == 2) snapshot.snapshot.chosen.reset();
        backend.snapshots = {snapshot};
        REQUIRE(winactivate::ActivateWithKms(backend, {}).error);
        REQUIRE(backend.activationCalls == 0);
        REQUIRE(backend.installCalls == 0);
    }
    winactivate::MockLicensingBackend backend;
    auto wrong = Snapshot(0);
    wrong.snapshot.os.sku = 49;
    backend.snapshots = {Snapshot(0, false), wrong, Snapshot(0)};
    REQUIRE(winactivate::ActivateWithKms(backend, {}).error);
    REQUIRE(backend.activationCalls == 0);
}

TEST_CASE("KMS rejects invalid ports without touching licensing") {
    winactivate::MockLicensingBackend backend;
    REQUIRE(winactivate::ActivateWithKms(backend, {L"kms.example", 0}).error);
    REQUIRE(winactivate::ActivateWithKms(backend, {L"kms.example", 65536}).error);
    REQUIRE(winactivate::ActivateWithKms(backend, {L"", 1234}).error);
    REQUIRE(backend.queryCalls == 0);
}

TEST_CASE("KMS never maps Home or Server to a different edition") {
    REQUIRE(winactivate::KmsClientSetupKey(101).empty());
    REQUIRE(winactivate::KmsClientSetupKey(7).empty());
    REQUIRE(winactivate::KmsClientSetupKey(125).empty());
}

TEST_CASE("KMS remaining time comes from Windows rather than a hardcoded 180 days") {
    auto record = *Snapshot(1).snapshot.chosen;
    REQUIRE(winactivate::KmsRemainingText(record) == L"Not reported by Windows");
    record.gracePeriodMinutes = 1441;
    REQUIRE(winactivate::KmsRemainingText(record) == L"1 days, 0 hours remaining");
    record.gracePeriodMinutes = 0;
    REQUIRE(winactivate::KmsRemainingText(record) == L"0 days, 0 hours remaining");
}
