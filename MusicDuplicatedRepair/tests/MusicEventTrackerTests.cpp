#include "music_repair/MusicEventTracker.h"

#include <stdexcept>

namespace {

using namespace music_loop_repair;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestSameStreamRepeat() {
    MusicEventTracker tracker(15'000u);
    tracker.ObserveOpen(0x100u, "Snd/Music/roman01.mp3");
    auto first = tracker.ObserveStart(0x100u, 1'000u);
    Require(first.repeat == RepeatKind::None && first.streamStarts == 1u,
            "first stream start was classified as a repeat");
    auto repeated = tracker.ObserveStart(0x100u, 2'500u);
    Require(repeated.repeat == RepeatKind::SameStream &&
                repeated.intervalMs == 1'500u &&
                repeated.streamStarts == 2u,
            "same stream repeat was not classified");
}

void TestSamePathReplacement() {
    MusicEventTracker tracker(15'000u);
    tracker.ObserveOpen(0x100u, "Snd/Music/roman01.mp3");
    tracker.ObserveStart(0x100u, 1'000u);
    tracker.ObserveClose(0x100u);
    tracker.ObserveOpen(0x200u, "Snd/Music/roman01.mp3");
    const auto repeated = tracker.ObserveStart(0x200u, 5'000u);
    Require(repeated.repeat == RepeatKind::SamePath &&
                repeated.intervalMs == 4'000u,
            "same path replacement was not classified");
}

void TestWindowAndUnknownStream() {
    MusicEventTracker tracker(5'000u);
    tracker.ObserveOpen(0x100u, "Snd/Music/viking01.mp3");
    tracker.ObserveStart(0x100u, 1'000u);
    const auto later = tracker.ObserveStart(0x100u, 8'000u);
    Require(later.repeat == RepeatKind::None,
            "start outside duplicate window was classified as repeat");
    const auto unknown = tracker.ObserveStart(0x999u, 9'000u);
    Require(unknown.repeat == RepeatKind::UnknownStream,
            "unknown stream was not identified");
}

void TestStartAtSystemBoot() {
    MusicEventTracker tracker(5'000u);
    tracker.ObserveOpen(0x100u, "Snd/Music/maya01.mp3");
    tracker.ObserveStart(0x100u, 0u);
    const auto repeated = tracker.ObserveStart(0x100u, 100u);
    Require(repeated.repeat == RepeatKind::SameStream &&
                repeated.intervalMs == 100u,
            "zero timestamp prevented same stream classification");
}

}  // namespace

int RunMusicEventTrackerTests() {
    TestSameStreamRepeat();
    TestSamePathReplacement();
    TestWindowAndUnknownStream();
    TestStartAtSystemBoot();
    return 0;
}

#ifdef MUSIC_EVENT_TRACKER_STANDALONE
int main() {
    return RunMusicEventTrackerTests();
}
#endif
