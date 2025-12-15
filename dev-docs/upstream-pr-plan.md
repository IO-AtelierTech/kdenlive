# Upstream PR Plan

This document tracks bug fixes made outside `src/rpc/` that should be submitted to upstream Kdenlive via KDE GitLab.

## Strategy

Cherry-pick individual bug fixes as separate merge requests to upstream. Each fix should be:
1. Self-contained and focused
2. Have a clear commit message explaining the bug
3. Include the crash backtrace or reproduction steps in the MR description

## Target Repository

- **Upstream**: https://invent.kde.org/multimedia/kdenlive
- **MR Process**: https://community.kde.org/Infrastructure/GitLab

## Bug Fixes to Submit

### Priority 1: Null Pointer Crashes

These fixes prevent crashes when project closes during async operations.

#### MR-001: ClipCreator null checks

| Field | Value |
|-------|-------|
| Commit | `951f433275` |
| Files | `src/bin/clipcreator.cpp` |
| Bug | SIGSEGV when ClipCreator accesses pCore->currentDoc() during project close |
| Fix | Added null checks before accessing document methods |

```bash
git cherry-pick 951f433275
```

---

#### MR-002: Clip Properties null checks

| Field | Value |
|-------|-------|
| Commit | `9d3056d4ae` |
| Files | `src/bin/bin.cpp`, `src/mltcontroller/clippropertiescontroller.cpp` |
| Bug | SIGSEGV in KdenliveDoc::useProxy() when showClipProperties triggered during close |
| Fix | Early return if document is null/closing |

```bash
git cherry-pick 9d3056d4ae
```

---

#### MR-003: Timeline null checks

| Field | Value |
|-------|-------|
| Commit | `ff8c26c40a` |
| Files | `src/timeline2/model/timelineitemmodel.cpp`, `src/timeline2/view/timelinecontroller.cpp`, `src/timeline2/view/timelinetabs.cpp`, `src/timeline2/view/timelinewidget.cpp` |
| Bug | SIGSEGV when timeline code accesses document properties during project close |
| Fix | Added null checks in multiple timeline functions |

```bash
git cherry-pick ff8c26c40a
```

---

#### MR-004: Monitor null checks

| Field | Value |
|-------|-------|
| Commits | `9d5f5f2556`, `45f56e0b6e` |
| Files | `src/monitor/monitor.cpp` |
| Bug | SIGSEGV in Monitor::buildBackgroundedProducer and buildSplitEffect during close |
| Fix | Added pCore->closing and doc->closing guards |

```bash
git cherry-pick 45f56e0b6e
git cherry-pick 9d5f5f2556
```

---

#### MR-005: GuidesList null check

| Field | Value |
|-------|-------|
| Commit | `7f49202202` |
| Files | `src/utils/guideinfo.cpp` |
| Bug | Null pointer access in GuidesList::updateFilter |
| Fix | Added null check for clip |

```bash
git cherry-pick 7f49202202
```

---

### Priority 2: QML Null Guards

These prevent binding errors in QML when timeline state is transitioning.

#### MR-006: QML Timeline binding guards

| Field | Value |
|-------|-------|
| Commit | `7646230f5e` |
| Files | Various QML files in `src/timeline2/view/qml/` |
| Bug | QML binding warnings and potential crashes |
| Fix | Added null guards to QML bindings |

```bash
git cherry-pick 7646230f5e
```

---

## Submission Checklist

For each MR:

- [ ] Create branch from upstream master: `git checkout -b fix/null-pointer-clipcreator upstream/master`
- [ ] Cherry-pick commit: `git cherry-pick <hash>`
- [ ] Verify build: `cmake --build build`
- [ ] Verify clang-format: `git clang-format --diff`
- [ ] Push to fork: `git push origin fix/null-pointer-clipcreator`
- [ ] Create MR on KDE GitLab
- [ ] Add description with:
  - Bug description
  - Steps to reproduce (if known)
  - Backtrace
  - Fix explanation
- [ ] Wait for CI and review

## Notes

- These fixes were discovered while testing RPC functionality but are general bugs
- The RPC code itself (`src/rpc/`) is NOT submitted upstream (it's fork-specific)
- Reference `dev-docs/null-pointer-crash-map.md` for full analysis

## Status

| MR | Status | Link |
|----|--------|------|
| MR-001 | Not submitted | - |
| MR-002 | Not submitted | - |
| MR-003 | Not submitted | - |
| MR-004 | Not submitted | - |
| MR-005 | Not submitted | - |
| MR-006 | Not submitted | - |

---

*Created: December 2024*
