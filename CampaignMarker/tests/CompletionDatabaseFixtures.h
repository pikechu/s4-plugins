#pragma once

#include <stdexcept>
#include <string>

namespace campaign_completion::test_fixtures {

inline std::string LiveThreeRecordDatabase() {
    return R"json({"schema_version":1,"records":[{"stable_id":"map:map\\singleplayer\\aeneas.map","relative_id":"Map\\Singleplayer\\Aeneas.map","display_name":"Aeneas","map_kind":"fixed","launch_source":"single-player-map","completed_at_utc":"2026-07-14T03:53:50Z","record_source":"native-event-609","game_version":"2.50.1516.0","plugin_version":"0.4.0"},{"stable_id":"map:map\\singleplayer\\thecross.map","relative_id":"Map\\Singleplayer\\TheCross.map","display_name":"TheCross","map_kind":"fixed","launch_source":"single-player-map","completed_at_utc":"2026-07-14T11:13:02Z","record_source":"native-event-609","game_version":"2.50.1516.0","plugin_version":"0.5.0"},{"stable_id":"map:map\\user\\antares.map","relative_id":"Map\\User\\Antares.map","display_name":"Antares","map_kind":"fixed","launch_source":"single-player-custom-map","completed_at_utc":"2026-07-14T03:56:57Z","record_source":"native-event-609","game_version":"2.50.1516.0","plugin_version":"0.4.0"}]})json"
           "\n";
}

inline std::string UnsupportedVersionDatabase() {
    auto bytes = LiveThreeRecordDatabase();
    const std::string accepted = "\"plugin_version\":\"0.5.0\"";
    const auto position = bytes.find(accepted);
    if (position == std::string::npos) {
        throw std::runtime_error("historical version fixture is missing");
    }
    bytes.replace(position, accepted.size(),
                  "\"plugin_version\":\"0.5.1\"");
    return bytes;
}

}  // namespace campaign_completion::test_fixtures
