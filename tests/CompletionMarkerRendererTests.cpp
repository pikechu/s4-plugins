#include "marker/CompletionMarkerRenderer.h"

#include <stdexcept>
#include <string>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeSurface final : public campaign_completion::IMarkerDrawingSurface {
public:
    bool Describe(LPDIRECTDRAWSURFACE7,
                  campaign_completion::MarkerSurfaceExtent& output) noexcept
        override {
        ++describeCalls;
        output = extent;
        return describeResult;
    }

    bool Begin(LPDIRECTDRAWSURFACE7) noexcept override {
        ++beginCalls;
        return beginResult;
    }

    bool DrawOutlinedCheck(
        const campaign_completion::MarkerCheckGeometry&) noexcept override {
        ++drawCalls;
        return drawResult;
    }

    bool End() noexcept override {
        ++endCalls;
        return endResult;
    }

    campaign_completion::MarkerSurfaceExtent extent{800u, 600u};
    bool describeResult = true;
    bool beginResult = true;
    bool drawResult = true;
    bool endResult = true;
    int describeCalls = 0;
    int beginCalls = 0;
    int drawCalls = 0;
    int endCalls = 0;
};

class BlockingSurface final
    : public campaign_completion::IMarkerDrawingSurface {
public:
    bool Describe(LPDIRECTDRAWSURFACE7,
                  campaign_completion::MarkerSurfaceExtent& extent) noexcept
        override {
        extent = {800u, 600u};
        return true;
    }
    bool Begin(LPDIRECTDRAWSURFACE7) noexcept override {
        std::unique_lock<std::mutex> lock(mutex);
        began = true;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
        return true;
    }
    bool DrawOutlinedCheck(
        const campaign_completion::MarkerCheckGeometry&) noexcept override {
        return true;
    }
    bool End() noexcept override { return true; }

    std::mutex mutex;
    std::condition_variable changed;
    bool began = false;
    bool released = false;
};

campaign_completion::MarkerDrawCommand Row(std::size_t slot) {
    campaign_completion::MarkerDrawCommand row{};
    row.logicalSurfaceWidth = 800u;
    row.logicalSurfaceHeight = 600u;
    row.x = 115u;
    row.y = static_cast<WORD>(142u + 31u * slot);
    row.width = 271u;
    row.height = 30u;
    return row;
}

campaign_completion::MarkerFrameCommands Frame(std::size_t count) {
    campaign_completion::MarkerFrameCommands frame{};
    frame.count = count;
    for (std::size_t index = 0u; index < count; ++index) {
        frame.commands[index] = Row(index);
    }
    return frame;
}

}  // namespace

int RunCompletionMarkerRendererTests() {
    using namespace campaign_completion;
    auto* destination = reinterpret_cast<LPDIRECTDRAWSURFACE7>(1u);

    {
        FakeSurface surface;
        std::vector<std::pair<LogLevel, std::string>> logs;
        CompletionMarkerRenderer renderer(
            surface, [&logs](LogLevel level, std::string line) {
                logs.emplace_back(level, std::move(line));
            });
        Require(renderer.Render(destination, 0, Frame(0u), 0u) ==
                    MarkerRenderStatus::Skipped &&
                    surface.describeCalls == 0 && surface.beginCalls == 0 &&
                    surface.drawCalls == 0 && surface.endCalls == 0 &&
                    logs.empty(),
                "an empty frame makes no backend call and emits no log");

        Require(renderer.Render(destination, 0, Frame(2u), 1u) ==
                    MarkerRenderStatus::Drawn &&
                    surface.describeCalls == 1 && surface.beginCalls == 1 &&
                    surface.drawCalls == 2 && surface.endCalls == 1 &&
                    logs.empty(),
                "two checks share one describe, begin, and end pass");
    }

    {
        FakeSurface surface;
        CompletionMarkerRenderer renderer(surface, {});
        auto invalid = Frame(2u);
        invalid.commands[1].logicalSurfaceWidth = 100u;
        Require(renderer.Render(destination, 0, invalid, 0u) ==
                    MarkerRenderStatus::Failed &&
                    surface.describeCalls == 1 && surface.beginCalls == 0,
                "invalid geometry drops the whole frame before Begin");
    }

    {
        FakeSurface surface;
        surface.drawResult = false;
        CompletionMarkerRenderer renderer(surface, {});
        Require(renderer.Render(destination, 0, Frame(1u), 0u) ==
                    MarkerRenderStatus::Failed &&
                    surface.drawCalls == 1 && surface.endCalls == 1,
                "a draw failure still closes the backend pass");
        surface.drawResult = true;
        Require(renderer.Render(destination, 0, Frame(1u), 1u) ==
                    MarkerRenderStatus::Drawn,
                "a later successful frame resets consecutive failures");
        surface.beginResult = false;
        Require(renderer.Render(destination, 0, Frame(1u), 2u) ==
                    MarkerRenderStatus::Failed &&
                    renderer.Render(destination, 0, Frame(1u), 3u) ==
                        MarkerRenderStatus::Failed,
                "two failures after a success do not disable rendering");
    }

    {
        FakeSurface surface;
        surface.beginResult = false;
        std::vector<std::pair<LogLevel, std::string>> logs;
        CompletionMarkerRenderer renderer(
            surface, [&logs](LogLevel level, std::string line) {
                logs.emplace_back(level, std::move(line));
            });
        Require(renderer.Render(destination, 0, Frame(1u), 0u) ==
                    MarkerRenderStatus::Failed &&
                    renderer.Render(destination, 0, Frame(1u), 1000u) ==
                        MarkerRenderStatus::Failed &&
                    renderer.Render(destination, 0, Frame(1u), 6000u) ==
                        MarkerRenderStatus::Disabled,
                "three consecutive backend failures disable rendering");
        const int callsAtDisable = surface.describeCalls;
        Require(renderer.Render(destination, 0, Frame(1u), 7000u) ==
                    MarkerRenderStatus::Disabled &&
                    surface.describeCalls == callsAtDisable,
                "a disabled renderer makes no later backend calls");

        std::size_t transient = 0u;
        std::size_t terminal = 0u;
        for (const auto& log : logs) {
            transient += log.second ==
                                 "completion-marker-renderer frame-failed"
                             ? 1u
                             : 0u;
            terminal += log.second ==
                                "completion-marker-renderer disabled failures=3"
                            ? 1u
                            : 0u;
        }
        Require(transient == 2u && terminal == 1u,
                "transient logs are rate-limited and terminal disable logs once");
    }

    {
        BlockingSurface surface;
        CompletionMarkerRenderer renderer(surface, {});
        MarkerRenderStatus renderStatus = MarkerRenderStatus::Skipped;
        std::thread rendering([&] {
            renderStatus = renderer.Render(destination, 0, Frame(1u), 0u);
        });
        {
            std::unique_lock<std::mutex> lock(surface.mutex);
            surface.changed.wait(lock, [&surface] { return surface.began; });
        }
        std::mutex disableMutex;
        std::condition_variable disableChanged;
        bool disableFinished = false;
        std::thread disabling([&] {
            renderer.Disable();
            {
                std::lock_guard<std::mutex> lock(disableMutex);
                disableFinished = true;
            }
            disableChanged.notify_all();
        });
        bool disableRacedWithRender = false;
        {
            std::unique_lock<std::mutex> lock(disableMutex);
            disableRacedWithRender = disableChanged.wait_for(
                lock, std::chrono::milliseconds(100),
                [&disableFinished] { return disableFinished; });
        }
        {
            std::lock_guard<std::mutex> lock(surface.mutex);
            surface.released = true;
        }
        surface.changed.notify_all();
        rendering.join();
        disabling.join();
        Require(!disableRacedWithRender &&
                    renderStatus == MarkerRenderStatus::Drawn &&
                    renderer.Render(destination, 0, Frame(1u), 1u) ==
                        MarkerRenderStatus::Disabled,
                "Disable waits for an in-flight render before disabling");
    }

    return 0;
}
