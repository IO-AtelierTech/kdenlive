# Kdenlive WebSocket RPC - Implementation Status

## Project Status: Functional

The JSON-RPC WebSocket API is implemented and working. The schema at `src/rpc/rpc-schema.json5` is the **single source of truth** for the API specification.

---

## Implementation Summary

| Metric | Count |
|--------|-------|
| Total Methods | 86 |
| Handlers | 9 |
| Events | 10 |
| MCP Tools Working | 54/72 (75%) |

### Handlers Overview

| Handler | Methods | Status |
|---------|---------|--------|
| `rpc.*` | 5 | Complete |
| `project.*` | 7 | Complete |
| `timeline.*` | 17 | Complete |
| `bin.*` | 14 | Complete |
| `effect.*` | 14 | Complete |
| `asset.*` | 9 | Complete |
| `render.*` | 10 | Complete |
| `transition.*` | 5 | Complete |
| `composition.*` | 5 | Complete |

---

## What's Working (MCP Integration)

The following operations are verified working via MCP testing:

### Project Management
- Create, open, save, close projects
- Undo/redo operations
- Get project info

### Bin Operations
- Import single/multiple clips
- Create/delete folders
- List clips and folders
- Get clip info (with async loading support)
- Add/get/delete clip markers
- Rename and move items

### Timeline Editing
- Get timeline info and tracks
- Insert, move, delete clips
- Split and resize clips
- Seek and get position
- Add/delete tracks
- Selection management

### Effects
- List available effects
- Add/remove effects from clips
- Enable/disable effects
- Reorder effects
- Copy effects to multiple clips
- Keyframe operations (set, get, delete)

### Rendering
- List render presets
- Start render jobs
- Monitor render status

---

## Known Issues

### API-Level Issues (MCP layer)

| Issue | Tools Affected | Description |
|-------|----------------|-------------|
| List handling | `transition.list`, `composition.list`, `asset.*`, `render.getJobs` | Returns list, code expects dict |
| Parameter mismatch | `transition.add` | Uses `from_clip_id`/`to_clip_id`, server expects `clipId1`/`clipId2` |
| Parameter mismatch | `asset.savePreset` | Uses `effect_id`/`preset_name`, server expects `effectId`/`name` |
| Parameter mismatch | `asset.getEffectsByCategory` | Parameter name truncated in error |

### Server-Level Issues (Kdenlive RPC)

| Issue | Description | Status |
|-------|-------------|--------|
| `timeline.setTrackProperty` | Returns success but muted/locked/hidden don't actually change | Investigating |
| `effect.getProperty` | Returns empty value for some properties | May be effect-specific |
| `composition.add` | Passes validation but fails at Kdenlive level | Investigating |

---

## Bug Fixes Outside `src/rpc/`

These fixes were made to core Kdenlive code to prevent crashes discovered during RPC testing. They should be submitted as separate upstream PRs.

### Null Pointer Fixes

| File | Description | Commit |
|------|-------------|--------|
| `src/bin/projectclip.cpp:216` | Check `pCore->currentDoc()` before accessing `closing` | Fixed |
| `src/bin/clipcreator.cpp` | Null checks during project close | Fixed |
| `src/monitor/monitor.cpp` | Guards in `buildBackgroundedProducer` and `buildSplitEffect` | Fixed |
| `src/timeline2/view/timelinecontroller.cpp` | Null checks for timeline access | Fixed |
| `src/utils/guideinfo.cpp` (GuidesList) | Null check in `updateFilter` | Fixed |

### Reference

See `dev-docs/null-pointer-crash-map.md` for full analysis of potential crash locations.

---

## Testing

### C++ Unit Tests
```bash
cd build && ctest -R rpctest
```

### Python E2E Tests
```bash
cd tests/rpc
pip install -r requirements.txt
python test_rpc_client.py
```

### Schema Validation
```bash
python tests/rpc/validate_schema.py
```

---

## Documentation

| Document | Purpose |
|----------|---------|
| `dev-docs/rpc/api.md` | Full API reference with examples |
| `dev-docs/rpc/debugging.md` | Debugging workflow |
| `dev-docs/rpc/qa-strategy.md` | QA checklist |
| `src/rpc/rpc-schema.json5` | **Source of truth** - schema definition |
| `README.md` | Quick start and overview |

---

## Next Steps

1. **Fix MCP parameter mismatches** - Align Python client with server expectations
2. **Investigate false positives** - `timeline.setTrackProperty` not changing state
3. **Submit upstream PRs** - Cherry-pick null-pointer fixes to KDE GitLab
4. **Windows CI** - Fix GitHub Actions for Windows builds
5. **Future enhancements** - Guides management, subtitle clips, proxy generation

---

## Architecture

```
src/rpc/
├── rpcmanager.h/cpp        # WebSocket server, JSON-RPC dispatcher
├── rpc-schema.json5        # API schema (source of truth)
└── handlers/
    ├── rpchandler.cpp      # rpc.* methods
    ├── projecthandler.cpp  # project.* methods
    ├── timelinehandler.cpp # timeline.* methods
    ├── binhandler.cpp      # bin.* methods
    ├── effectshandler.cpp  # effect.* methods
    ├── assetshandler.cpp   # asset.* methods
    ├── renderhandler.cpp   # render.* methods
    ├── transitionhandler.cpp # transition.* methods
    └── compositionhandler.cpp # composition.* methods
```

---

*Last updated: December 2024*
