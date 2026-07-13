#include "hook/FixedMapLoadHook.h"

#include <windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

campaign_completion::HookSiteAdmission ApprovedAdmission() {
    return {0x004FEFA5u, 0x004B1310u,
            campaign_completion::HookSiteFailure::None};
}

campaign_completion::HookSiteAdmission FailedAdmission() {
    return {0u, 0u, campaign_completion::HookSiteFailure::BytesMismatch};
}

#pragma pack(push, 1)
struct TestWideString32 final {
    union Storage {
        wchar_t inlineText[8];
        std::uint32_t pointer32;

        Storage() : inlineText{} {}
    } storage;
    std::uint32_t length = 0;
    std::uint32_t capacity = 7;
};
#pragma pack(pop)

static_assert(sizeof(TestWideString32) == 24);

TestWideString32 InlinePath(const wchar_t* value) {
    TestWideString32 fixture{};
    const auto length = std::wcslen(value);
    Require(length < 8u, "adapter fixture must fit inline storage");
    std::memcpy(fixture.storage.inlineText, value,
                (length + 1u) * sizeof(wchar_t));
    fixture.length = static_cast<std::uint32_t>(length);
    return fixture;
}

class FakeBackend final : public campaign_completion::ICallPatchBackend {
public:
    campaign_completion::PatchFailure Install(
        std::uintptr_t callSite, std::uintptr_t destination,
        const std::array<std::uint8_t, 5>& expectedBytes) override {
        ++installCalls;
        lastCallSite = callSite;
        lastDestination = destination;
        lastExpected = expectedBytes;
        if (installFailure == campaign_completion::PatchFailure::None &&
            markInstalled) {
            state = campaign_completion::PatchState::InstalledByUs;
        }
        return installFailure;
    }

    campaign_completion::PatchState Inspect() override {
        ++inspectCalls;
        return state;
    }

    campaign_completion::PatchFailure Restore() override {
        ++restoreCalls;
        if (restoreFailure == campaign_completion::PatchFailure::None &&
            markRestored) {
            state = campaign_completion::PatchState::Original;
        }
        return restoreFailure;
    }

    std::atomic<int> installCalls{0};
    std::atomic<int> inspectCalls{0};
    std::atomic<int> restoreCalls{0};
    std::uintptr_t lastCallSite = 0;
    std::uintptr_t lastDestination = 0;
    std::array<std::uint8_t, 5> lastExpected{};
    campaign_completion::PatchState state =
        campaign_completion::PatchState::Original;
    campaign_completion::PatchFailure installFailure =
        campaign_completion::PatchFailure::None;
    campaign_completion::PatchFailure restoreFailure =
        campaign_completion::PatchFailure::None;
    bool markInstalled = true;
    bool markRestored = true;
};

class FakeInvoker final : public campaign_completion::IOriginalLoadInvoker {
public:
    void Invoke(void* mapFile, const void* path, DWORD mode,
                DWORD validate) override {
        std::unique_lock<std::mutex> lock(mutex);
        ++calls;
        lastMapFile = mapFile;
        lastPath = path;
        lastMode = mode;
        lastValidate = validate;
        entered = true;
        condition.notify_all();
        condition.wait(lock, [this] { return !block || released; });
    }

    void WaitUntilEntered() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { return entered; });
    }

    void Release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }

    std::mutex mutex;
    std::condition_variable condition;
    int calls = 0;
    void* lastMapFile = nullptr;
    const void* lastPath = nullptr;
    DWORD lastMode = 0;
    DWORD lastValidate = 0;
    bool block = false;
    bool entered = false;
    bool released = false;
};

class FakeSink final : public campaign_completion::IRawCaptureSink {
public:
    void Observe(campaign_completion::CapturedWidePath capture,
                 std::uint64_t sequence, std::uint64_t nowMs) override {
        std::lock_guard<std::mutex> lock(mutex);
        records.push_back(std::move(capture));
        sequences.push_back(sequence);
        times.push_back(nowMs);
    }

    std::size_t Size() {
        std::lock_guard<std::mutex> lock(mutex);
        return records.size();
    }

    std::mutex mutex;
    std::vector<campaign_completion::CapturedWidePath> records;
    std::vector<std::uint64_t> sequences;
    std::vector<std::uint64_t> times;
};

}  // namespace

int RunFixedMapLoadHookTests() {
    using campaign_completion::CampaignCompletionFixedMapAdapter;
    using campaign_completion::FixedMapLoadHook;
    using campaign_completion::PatchFailure;
    using campaign_completion::PatchState;

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        Require(!hook.Start(FailedAdmission(), backend, invoker, sink),
                "failed layout cannot patch");
        Require(backend.installCalls == 0, "failed layout performs no install");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        backend.installFailure = PatchFailure::InstallFailed;
        Require(!hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "backend install failure rejected");
        Require(backend.installCalls == 1, "install attempted once");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        backend.markInstalled = false;
        backend.state = PatchState::Conflict;
        Require(!hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "post-install ownership failure rejected");
        Require(backend.restoreCalls == 0,
                "unknown post-install state is never overwritten");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        Require(hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "approved layout patches once");
        Require(!hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "second start rejected");
        Require(backend.installCalls == 1, "only one install occurs");
        Require(backend.lastCallSite == 0x004FEFA5u,
                "admitted call site forwarded");
        Require(backend.lastDestination != 0u,
                "adapter destination forwarded");
        Require(backend.lastExpected ==
                    campaign_completion::kApprovedFixedMapHookSite
                        .expectedCallBytes,
                "strict original bytes forwarded");

        auto fixture = InlinePath(L"Tiny");
        auto* fakeMapFile = reinterpret_cast<void*>(0x12345678u);
        CampaignCompletionFixedMapAdapter(fakeMapFile, nullptr, &fixture, 2u,
                                          1u);
        Require(invoker.calls == 1, "original invoked exactly once");
        Require(invoker.lastMapFile == fakeMapFile &&
                    invoker.lastPath == &fixture,
                "original object and path preserved");
        Require(invoker.lastMode == 2u && invoker.lastValidate == 1u,
                "original numeric arguments preserved");
        Require(sink.Size() == 1u, "owned capture published");
        Require(sink.records.front().value == L"Tiny",
                "published capture owns exact text");

        CampaignCompletionFixedMapAdapter(fakeMapFile, nullptr, nullptr, 3u,
                                          0u);
        Require(invoker.calls == 2,
                "malformed capture still invokes original exactly once");
        Require(sink.Size() == 2u,
                "malformed capture publishes a bounded failure event");
        Require(sink.records.back().failure ==
                    campaign_completion::WideCaptureFailure::NullObject,
                "malformed capture preserves the exact failure reason");
        Require(sink.sequences.back() == sink.sequences.front() + 1u,
                "every adapter entry receives a monotonic sequence");

        Require(hook.Stop() == PatchFailure::None,
                "owned patch restores cleanly");
        Require(backend.restoreCalls == 1, "restore occurs once");
        CampaignCompletionFixedMapAdapter(fakeMapFile, nullptr, &fixture, 2u,
                                          1u);
        Require(sink.Size() == 2u,
                "closed adapter cannot publish a new capture");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        invoker.block = true;
        Require(hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "blocking fixture starts");
        auto fixture = InlinePath(L"Wait");
        std::thread adapter([&] {
            CampaignCompletionFixedMapAdapter(nullptr, nullptr, &fixture, 2u,
                                              1u);
        });
        invoker.WaitUntilEntered();
        auto stopping = std::async(std::launch::async, [&] {
            return hook.Stop();
        });
        Require(stopping.wait_for(std::chrono::milliseconds(50)) ==
                    std::future_status::timeout,
                "stop waits for in-flight adapter");
        Require(backend.restoreCalls == 0,
                "restore starts only after adapter drain");
        invoker.Release();
        adapter.join();
        Require(stopping.get() == PatchFailure::None,
                "drained adapter permits restore");
        Require(backend.restoreCalls == 1,
                "restore follows adapter drain");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        Require(hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "verification fixture starts");
        backend.markRestored = false;
        Require(hook.Stop() == PatchFailure::VerificationFailed,
                "restored-state verification required");
        backend.markRestored = true;
        backend.state = PatchState::InstalledByUs;
        Require(hook.Stop() == PatchFailure::None,
                "verification fixture can finish after state correction");
    }

    {
        FixedMapLoadHook hook;
        FakeBackend backend;
        FakeInvoker invoker;
        FakeSink sink;
        Require(hook.Start(ApprovedAdmission(), backend, invoker, sink),
                "conflict fixture starts");
        backend.state = PatchState::Conflict;
        Require(hook.Stop() == PatchFailure::Conflict,
                "third-party state reports conflict");
        Require(backend.restoreCalls == 0,
                "conflict prevents restore overwrite");
        backend.state = PatchState::InstalledByUs;
        Require(hook.Stop() == PatchFailure::None,
                "conflict fixture can finish after ownership returns");
    }

    return 0;
}
