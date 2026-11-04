#pragma once

#include "common/IpcProtocol.h"
#include <Windows.h>
#include <string>

namespace winactivate {

[[nodiscard]] std::wstring ApplicationDirectory();
[[nodiscard]] HRESULT TransferFrame(HANDLE pipe, void* frame, DWORD bytes,
                                     bool writing, DWORD timeoutMs);
[[nodiscard]] HRESULT CompletePipeOperation(HANDLE pipe, OVERLAPPED& operation,
                                            BOOL started, DWORD timeoutMs,
                                            DWORD& transferred);
[[nodiscard]] bool ProcessMatchesApplication(HANDLE process, const wchar_t* filename);
// Runs on a GUI worker, never the window thread. Launches the sibling helper only.
[[nodiscard]] std::optional<ActivationError> RunElevatedHelper(
    const HelperRequest& request, HWND owner);

}  // namespace winactivate
