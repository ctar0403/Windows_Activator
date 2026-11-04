#include "common/KmsOptions.h"

#include <algorithm>
#include <cwctype>

namespace winactivate {

bool IsValidKmsHost(std::wstring_view host) noexcept {
    if (host.empty()) return true;
    if (host.size() > 253) return false;
    std::size_t length = 0;
    wchar_t previous = 0;
    for (wchar_t character : host) {
        if (character == L'.') {
            if (length == 0 || previous == L'-') return false;
            length = 0;
        } else {
            const bool alphanumeric = (character >= L'a' && character <= L'z') ||
                (character >= L'A' && character <= L'Z') ||
                (character >= L'0' && character <= L'9');
            if (!alphanumeric && character != L'-') return false;
            if (length == 0 && character == L'-') return false;
            if (++length > 63) return false;
        }
        previous = character;
    }
    return length != 0 && previous != L'-';
}

bool IsKmsClient(const LicenseRecord& record) {
    std::wstring description = record.description;
    std::transform(description.begin(), description.end(), description.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return description.find(L"VOLUME_KMSCLIENT") != std::wstring::npos;
}

std::wstring_view KmsClientSetupKey(std::uint32_t sku) noexcept {
    // Public Microsoft GVLKs, not purchased keys or license entitlements.
    // https://learn.microsoft.com/windows-server/get-started/kms-client-activation-keys
    // Only exact mapped editions: never switch Home, Server or LTSC editions.
    switch (sku) {
        case 48: return L"W269N-WFGWX-YVC9B-4J6C9-T83GX";
        case 49: return L"MH37W-N47XK-V7XM9-C7227-GCQG9";
        case 4: return L"NPPR9-FWDCX-D2C8J-H872K-2YT43";
        case 27: return L"DPH2V-TTNVB-4X9Q3-TJR4H-KHJW4";
        case 121: return L"NW6C2-QMPVW-D7KKK-3GKT6-VCFB2";
        case 122: return L"2WH4N-8QGBV-H22JP-CT43Q-MDWWJ";
        case 161: return L"NRG8B-VKK3Q-CXVCJ-9G2XF-6Q84J";
        case 162: return L"9FNHH-K3HBT-3W4TD-6383H-6XYWF";
        default: return {};
    }
}

std::wstring KmsRemainingText(const LicenseRecord& record) {
    if (!IsKmsClient(record)) return L"Not a KMS client";
    if (!record.gracePeriodMinutes) return L"Not reported by Windows";
    const auto minutes = *record.gracePeriodMinutes;
    return std::to_wstring(minutes / 1440) + L" days, " +
        std::to_wstring((minutes % 1440) / 60) + L" hours remaining";
}

}  // namespace winactivate
