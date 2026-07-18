# The Settlers IV plugins workspace

This repository is organized as a multi-plugin workspace.

`CampaignMarker/` contains campaign and fixed-map completion tracking,
in-game markers, and the classified completion manager. Its source, tests,
configuration, documentation, and tools are all self-contained below that
directory. Shared external SDK/import libraries remain under `third_party/`.

Additional plugins can coexist as sibling directories without mixing their
source or tests into Campaign Marker. Campaign Marker development and CI must
use `CampaignMarker/` as the project source root.
