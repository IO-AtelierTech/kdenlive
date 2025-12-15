# Octopus Master TODO

## Active Tentacles

| ID | Description | Scope | Status | Worktree |
|----|-------------|-------|--------|----------|
| - | - | - | - | - |

## Ready to Spawn

### t1-ci: CI/Build Infrastructure

**Priority:** High

**Scope:**
- `.github/workflows/build.yml`
- `docker/`
- `Justfile`
- `dev-docs/ci-status.md`

**Tasks:**
1. Fix Qt6 cache issue in CraftMaster
2. Get Windows build producing exe
3. Get Linux build producing AppImage
4. Set up artifact upload to releases
5. Add build status badge to README

**Reference:**
- Current issue: Qt6 not in KDE Craft cache, fallback build fails
- See `dev-docs/ci-status.md` for details

**Spawn command:**
```bash
just tentacle-spawn t1-ci
```

---

### t2-mcp-fixes: MCP Python Client Fixes

**Priority:** Medium

**Scope:**
- MCP Python client code (external repo)
- Parameter name fixes
- List handling bug

**Tasks:**
1. Fix `transition.add` parameter names (`from_clip_id` -> `clipId1`)
2. Fix `asset.savePreset` parameter names
3. Fix list handling in `_common.py`
4. Update MCP tests

---

### t3-upstream: Upstream Bug Fix PRs

**Priority:** Medium

**Scope:**
- `dev-docs/upstream-pr-plan.md`
- KDE GitLab MRs

**Tasks:**
1. Submit MR-001: ClipCreator null checks
2. Submit MR-002: Clip Properties null checks
3. Submit MR-003: Timeline null checks
4. Submit MR-004: Monitor null checks
5. Submit MR-005: GuidesList null check
6. Submit MR-006: QML binding guards

---

## Completed Tentacles

| ID | Description | Merged | Date |
|----|-------------|--------|------|
| rpc-effects | Effects/render handlers | feature/websocket | 2024-12 |
| rpc-infra | RPC infrastructure | feature/websocket | 2024-12 |
| rpc-timeline | Timeline/bin handlers | feature/websocket | 2024-12 |

---

## Log

### 2024-12-15
- Epic: Phase 2 defined
- Cleaned up stale worktrees from Phase 1
- Prepared t1-ci tentacle spec

---

*Last updated: December 2024*
