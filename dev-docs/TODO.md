# Kdenlive WebSocket Fork - Development Status

> Human-readable overview. For AI coordination, see `.octopus/`.

## Legend

- `[x]` Complete
- `[~]` Partial / In Progress
- `[ ]` Not Started

---

## Phase 1: RPC Implementation

### Core Infrastructure
- [x] WebSocket server (`rpcmanager.cpp`)
- [x] JSON-RPC 2.0 dispatcher
- [x] Schema definition (`src/rpc/rpc-schema.json5`)
- [x] Event subscription system
- [x] Authentication (optional token)

### Handlers (86 methods total)
- [x] `rpc.*` (5 methods) - Connection, discovery, events
- [x] `project.*` (7 methods) - Project lifecycle, undo/redo
- [x] `timeline.*` (17 methods) - Clip manipulation, tracks, playhead
- [x] `bin.*` (14 methods) - Media import, organization
- [x] `effect.*` (14 methods) - Effects, keyframes
- [x] `asset.*` (9 methods) - Effect discovery, presets
- [x] `render.*` (10 methods) - Render jobs, monitoring
- [x] `transition.*` (5 methods) - Same-track transitions
- [x] `composition.*` (5 methods) - Cross-track compositions

### MCP Integration
- [x] MCP server implementation (external repo)
- [~] Tool coverage - 54/72 tools working (75%)
- [ ] Fix parameter mismatches (transition.add, asset.savePreset)
- [ ] Fix list handling bugs

---

## Phase 2: Cleanup & CI (Current)

### Documentation
- [x] Update README.md with fork section
- [x] Move RPC docs to `dev-docs/rpc/`
- [x] Document schema as source of truth
- [x] Create debugging workflow guide
- [x] Create QA strategy doc
- [x] Create upstream PR plan

### Build Infrastructure
- [x] Docker setup in repo (`docker/`)
- [x] Justfile with dev commands
- [ ] Fix Linux CI (Qt6 cache issue)
- [ ] Fix Windows CI
- [ ] Artifact upload to GitHub releases

### Git Cleanup
- [x] Remove stale worktrees (rpc-effects, rpc-infra, rpc-timeline)
- [x] Squash merge to master
- [x] Tag v25.12-ws.1

---

## Phase 3: Upstream & Polish (Planned)

### Upstream PRs
- [ ] MR-001: ClipCreator null checks
- [ ] MR-002: Clip Properties null checks
- [ ] MR-003: Timeline null checks
- [ ] MR-004: Monitor null checks
- [ ] MR-005: GuidesList null check
- [ ] MR-006: QML binding guards

### Server-Level Fixes
- [ ] `timeline.setTrackProperty` not applying changes
- [ ] `effect.getProperty` empty for some properties
- [ ] `composition.add` failing at Kdenlive level

### Future Enhancements
- [ ] Guides management methods
- [ ] Subtitle clip support
- [ ] Proxy generation control

---

## Quick Reference

| Resource | Location |
|----------|----------|
| Schema (source of truth) | `src/rpc/rpc-schema.json5` |
| API documentation | `dev-docs/rpc/api.md` |
| Debugging guide | `dev-docs/rpc/debugging.md` |
| QA checklist | `dev-docs/rpc/qa-strategy.md` |
| Upstream PR plan | `dev-docs/upstream-pr-plan.md` |
| CI status | `dev-docs/ci-status.md` |
| Octopus coordination | `.octopus/` |

---

*Last updated: December 2024*
