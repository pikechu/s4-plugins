# Research evidence

This directory contains reproducible evidence for the Campaign Completion
plugin investigation.

## Safety boundary

- The installed game and Settlers United directories are read-only inputs.
- Collection scripts write only beneath `research/evidence` or transient
  `research/vendor` directories in this repository.
- Launching the game may cause the game or Settlers United to write their own
  logs, configuration, and cache files.
- Phase 1 does not deploy an ASI, inject a DLL, install a hook, or write
  completion data.
- Generated evidence may contain machine paths. Normalized committed evidence
  replaces installation prefixes and Windows account names with tokens.

## Regeneration

Run commands from the repository root:

```bash
bash research/scripts/collect_pe_metadata.sh
bash research/scripts/collect_s4modapi.sh
powershell.exe -NoProfile -File "$(wslpath -w research/scripts/collect_pdb_symbols.ps1)"
powershell.exe -NoProfile -File "$(wslpath -w research/scripts/collect_loader_state.ps1)" -ObserveOnly
```

The latter two scripts are added by their respective investigation tasks.
