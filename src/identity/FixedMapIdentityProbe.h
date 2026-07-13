#pragma once

#include "hook/FixedMapLoadHook.h"
#include "identity/FixedMapCaptureState.h"
#include "identity/MapPathValidator.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace campaign_completion {

class FixedMapIdentityProbe final : public IRawCaptureSink {
public:
    using RecordSink = std::function<void(std::string)>;

    FixedMapIdentityProbe(std::filesystem::path gameRoot, RecordSink recordSink);

    void Observe(CapturedWidePath capture, std::uint64_t sequence,
                 std::uint64_t nowMs) override;
    void ObserveListKind(FixedMapListKind listKind, std::uint64_t nowMs);
    void ObserveBack();
    void ObserveTopLevelPage(DWORD page);
    void ObserveMapInit(std::uint64_t nowMs);
    void Disable();

private:
    struct RawCapture final {
        CapturedWidePath capture;
        std::uint64_t sequence = 0;
        std::uint64_t capturedAtMs = 0;
    };

    void Emit(std::string record);

    std::mutex mutex_;
    MapPathValidator validator_;
    FixedMapCaptureState state_;
    RecordSink recordSink_;
    std::vector<RawCapture> pending_;
    std::string lastRecord_;
    FixedMapListKind currentKind_ = FixedMapListKind::Unknown;
    bool rawOverflow_ = false;
    bool disabled_ = false;
};

}  // namespace campaign_completion
