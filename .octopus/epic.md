# Epic: Kdenlive WebSocket - Phase 2

## Goal

Continue RPC development with improved CI/CD, fix remaining MCP issues, and submit upstream bug fixes.

## Acceptance Criteria

- [ ] Windows CI producing artifacts
- [ ] Linux CI producing AppImage
- [ ] MCP integration at 90%+ functionality
- [ ] Upstream PRs submitted for null-pointer fixes

## Exit Criteria

- [ ] All ACs demonstrated
- [ ] All tentacles merged to `feature/websocket`
- [ ] Full test suite passes
- [ ] Tag `v25.12-ws.2` pushed

## Out of Scope

- New RPC methods (separate epic)
- Major Kdenlive UI changes

## Technical Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Default push target | GitHub | CI artifacts, GitLab for upstream PRs only |
| Build system | Docker + Justfile | Consistent local builds |
| Schema source | `src/rpc/rpc-schema.json5` | Single source of truth |

## Tentacle Breakdown

| ID | Scope | Dependencies | Status |
|----|-------|--------------|--------|
| t1-ci | `.github/`, Docker, CI | none | **Ready to spawn** |
| t2-mcp-fixes | MCP Python client | none | pending |
| t3-upstream | Bug fix PRs | none | pending |

---

*Created: December 2024*
