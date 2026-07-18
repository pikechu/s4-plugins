#include "victory/VictoryEventProbe.h"

#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int RunVictoryEventProbeTests() {
    using namespace campaign_completion;

    VictoryEventProbe wonProbe;
    wonProbe.BeginSession(7u);
    Require(wonProbe.Observe({609u, 1u, 0u, 123u}),
            "609/1 is observed");
    const auto won = wonProbe.ConsumePending();
    Require(won.has_value() && won->sessionId == 7u &&
                won->result == NativeLocalResult::Won &&
                won->fields.eventId == 609u && won->fields.wParam == 1u &&
                won->fields.lParam == 0u && won->fields.gameTick == 123u,
            "609/1 is a local win");
    Require(!wonProbe.ConsumePending().has_value(),
            "pending event evidence is one shot");
    Require(!wonProbe.Observe({600u, 1u, 0u, 124u}),
            "unrelated events are ignored");

    VictoryEventProbe lostProbe;
    lostProbe.BeginSession(8u);
    Require(lostProbe.Observe({609u, 0u, 4u, 200u}),
            "609/0 is observed");
    const auto lost = lostProbe.ConsumePending();
    Require(lost.has_value() && lost->result == NativeLocalResult::Lost &&
                lost->fields.lParam == 4u,
            "609/0 is a local loss");

    VictoryEventProbe malformed;
    malformed.BeginSession(9u);
    Require(malformed.Observe({609u, 2u, 0u, 300u}),
            "invalid terminal parameter is observed");
    Require(malformed.ConsumePending()->result ==
                NativeLocalResult::Malformed,
            "invalid terminal parameter fails closed");

    VictoryEventProbe duplicate;
    duplicate.BeginSession(10u);
    Require(duplicate.Observe({609u, 1u, 0u, 400u}),
            "first duplicate-control event is observed");
    duplicate.ConsumePending();
    Require(duplicate.Observe({609u, 1u, 0u, 400u}),
            "identical repeat updates bounded evidence");
    const auto repeated = duplicate.ConsumePending();
    Require(repeated.has_value() && repeated->duplicates == 1u &&
                repeated->result == NativeLocalResult::Won,
            "identical repeat is counted without changing result");

    VictoryEventProbe conflict;
    conflict.BeginSession(11u);
    Require(conflict.Observe({609u, 1u, 0u, 500u}),
            "conflict control begins with win");
    conflict.ConsumePending();
    Require(conflict.Observe({609u, 0u, 0u, 501u}),
            "opposite terminal result is observed");
    Require(conflict.ConsumePending()->result ==
                NativeLocalResult::Conflict,
            "opposite terminal result fails closed as conflict");

    VictoryEventProbe orphan;
    Require(orphan.Observe({609u, 1u, 0u, 600u}),
            "terminal event without a session is bounded evidence");
    const auto orphaned = orphan.ConsumePending();
    Require(orphaned.has_value() && orphaned->sessionId == 0u &&
                orphaned->orphans == 1u &&
                orphaned->result == NativeLocalResult::None,
            "orphan cannot become a result");

    VictoryEventProbe reset;
    reset.BeginSession(12u);
    reset.Observe({609u, 1u, 0u, 700u});
    reset.BeginSession(13u);
    Require(!reset.ConsumePending().has_value(),
            "new map session clears old pending evidence");
    reset.Observe({609u, 0u, 0u, 701u});
    Require(reset.ConsumePending()->sessionId == 13u,
            "new map session owns later evidence");
    reset.BeginSession(0u);
    reset.Observe({609u, 1u, 0u, 702u});
    Require(reset.ConsumePending()->sessionId == 0u,
            "zero session produces only orphan evidence");

    VictoryEventProbe disabled;
    disabled.BeginSession(14u);
    disabled.Disable();
    Require(!disabled.Observe({609u, 1u, 0u, 800u}) &&
                !disabled.ConsumePending().has_value(),
            "disabled probe is inert");
    return 0;
}
