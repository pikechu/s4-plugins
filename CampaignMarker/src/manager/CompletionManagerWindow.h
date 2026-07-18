#pragma once

#include "completion/CompletionStore.h"
#include "manager/CompletionManagerCatalog.h"
#include "manager/FixedMapDiscovery.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace campaign_completion {

struct CompletionManagerView final {
    CompletionManagerCatalogResult catalog;
    std::uint64_t revision = 0u;
    CompletionStoreMode storeMode = CompletionStoreMode::Uninitialized;
    FixedMapDiscoveryStatus discoveryStatus =
        FixedMapDiscoveryStatus::Invalid;
};

class CompletionManagerWindow final {
public:
    using ViewProvider = std::function<CompletionManagerView()>;
    using ApplyHandler = std::function<ManualCompletionApplyResult(
        const ManualCompletionRequest&)>;

    CompletionManagerWindow(HMODULE module, ViewProvider view,
                            ApplyHandler apply);
    CompletionManagerWindow(const CompletionManagerWindow&) = delete;
    CompletionManagerWindow& operator=(const CompletionManagerWindow&) =
        delete;
    ~CompletionManagerWindow();

    bool Start() noexcept;
    bool RequestOpen() noexcept;
    void SetMainMenuAvailable(bool available) noexcept;
    bool Stop(std::chrono::milliseconds timeout) noexcept;

private:
    static constexpr UINT kOpenMessage = WM_APP + 71u;
    static constexpr UINT kAvailabilityMessage = WM_APP + 72u;

    static LRESULT CALLBACK WindowProcedure(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    void Run() noexcept;
    LRESULT HandleMessage(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    bool CreateManagerWindow() noexcept;
    void CreateControls();
    void RebuildTree();
    void RefreshView();
    void PopulateList();
    void ApplyChanges();
    void UpdateEnabledState();
    bool EntryVisible(const CompletionManagerEntry& entry) const noexcept;
    CompletionRecord BuildManualRecord(
        const CompletionManagerEntry& entry) const;
    void SetStatus(const wchar_t* text);

    HMODULE module_ = nullptr;
    ViewProvider view_;
    ApplyHandler apply_;
    std::thread thread_;
    DWORD threadId_ = 0u;
    HANDLE readyEvent_ = nullptr;
    HWND window_ = nullptr;
    HWND tree_ = nullptr;
    HWND list_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND applyButton_ = nullptr;
    HWND closeButton_ = nullptr;
    HWND status_ = nullptr;
    CompletionManagerView current_{};
    std::vector<bool> initialChecks_;
    std::vector<bool> checks_;
    LPARAM filter_ = -1;
    bool populating_ = false;
    std::atomic<bool> mainMenuAvailable_{false};
    std::atomic<bool> stopping_{false};
};

}  // namespace campaign_completion
