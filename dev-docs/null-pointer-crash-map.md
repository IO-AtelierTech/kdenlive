# Potential Null Pointer Crashes: `pCore->currentDoc()` Access

## Overview

When a project is closed during async operations (RPC imports, background tasks, etc.),
`pCore->currentDoc()` returns `nullptr`. Code that accesses `currentDoc()->...` without
null checks will crash.

**Total occurrences:** 235 locations across 41 files

## Risk Categories

### HIGH RISK - Async/Background Tasks (jobs/)
These run in background threads and can complete after project close:

| File | Line | Code Pattern |
|------|------|--------------|
| `jobs/cliploadtask.cpp` | 491 | `pCore->currentDoc()->loading` |
| `jobs/cliploadtask.cpp` | 542 | `pCore->currentDoc()->useExternalProxy()` |
| `jobs/cliploadtask.cpp` | 546 | `pCore->currentDoc()->documentRoot()` |
| `jobs/proxytask.cpp` | 106 | `pCore->currentDoc()->getAutoProxyAlphaProfile()` |
| `jobs/proxytask.cpp` | 109 | `pCore->currentDoc()->getDocumentProperty()` |
| `jobs/proxytask.cpp` | 112 | `pCore->currentDoc()->getAutoProxyProfile()` |
| `jobs/proxytask.cpp` | 122 | `pCore->currentDoc()->getDocumentProperty()` |
| `jobs/proxytask.cpp` | 263 | `pCore->currentDoc()->getAutoProxyAlphaProfile()` |
| `jobs/proxytask.cpp` | 274 | `pCore->currentDoc()->getDocumentProperty()` |
| `jobs/proxytask.cpp` | 277 | `pCore->currentDoc()->getAutoProxyProfile()` |
| `jobs/proxytask.cpp` | 280 | `pCore->currentDoc()->getDocumentProperty()` |
| `jobs/transcodetask.cpp` | 96 | `pCore->currentDoc()->url()` |

### HIGH RISK - Destructors
Called during cleanup when project may already be destroyed:

| File | Line | Code Pattern | Status |
|------|------|--------------|--------|
| `bin/projectclip.cpp` | 216 | `pCore->currentDoc()->closing` | **FIXED** |

### MEDIUM RISK - Signal/Slot Callbacks
Can fire after project close:

| File | Lines | Count |
|------|-------|-------|
| `bin/projectclip.cpp` | various | 23 |
| `bin/model/subtitlemodel.cpp` | various | 22 |
| `monitor/monitor.cpp` | various | 17 |
| `timeline2/view/timelinecontroller.cpp` | various | 10 |
| `bin/clipcreator.cpp` | various | 10 |

### MEDIUM RISK - Preview/Render Operations
Long-running operations that may outlive project:

| File | Lines | Count |
|------|-------|-------|
| `timeline2/view/previewmanager.cpp` | various | 10 |
| `dialogs/renderwidget.cpp` | various | 12 |
| `render/renderrequest.cpp` | various | 2 |

### LOWER RISK - UI/Dialog Code
Usually only active when project is open:

| File | Lines | Count |
|------|-------|-------|
| `mainwindow.cpp` | various | 20 |
| `bin/sequenceclip.cpp` | various | 12 |
| `dialogs/timeremap.cpp` | various | 6 |
| `project/dialogs/archivewidget.cpp` | various | 4 |

## Files by Occurrence Count

```
23 bin/projectclip.cpp
22 bin/model/subtitlemodel.cpp
20 mainwindow.cpp
17 monitor/monitor.cpp
12 dialogs/renderwidget.cpp
12 bin/sequenceclip.cpp
10 timeline2/view/timelinecontroller.cpp
10 timeline2/view/previewmanager.cpp
10 bin/clipcreator.cpp
 8 jobs/proxytask.cpp
 7 timeline2/view/timelinetabs.cpp
 6 timeline2/model/timelinefunctions.cpp
 6 dialogs/timeremap.cpp
 5 timeline2/model/timelinemodel.cpp
 5 mltcontroller/clipcontroller.cpp
 4 pythoninterfaces/otioconvertions.cpp
 4 project/dialogs/archivewidget.cpp
 4 otio/otioimport.cpp
 4 bin/projectitemmodel.cpp
 4 assets/keyframes/model/automask/automaskhelper.cpp
 3 project/projectmanager.cpp
 3 jobs/cliploadtask.cpp
 3 effects/effectstack/view/maskmanager.cpp
 3 doc/kdenlivedoc.cpp
 3 bin/bin.cpp
 2 timeline2/view/timelinewidget.cpp
 2 timeline2/model/timelineitemmodel.cpp
 2 render/renderrequest.cpp
 2 project/dialogs/projectsettings.cpp
 2 otio/otioexport.cpp
 2 monitor/monitorproxy.cpp
 2 mltcontroller/clippropertiescontroller.cpp
 2 dialogs/settings/kdenlivesettingsdialog.cpp
 2 bin/projectsubclip.cpp
 2 bin/playlistsubclip.cpp
 2 assets/view/widgets/urllistparamwidget.cpp
 1 jobs/transcodetask.cpp
 1 dialogs/proxytest.cpp
 1 core.cpp
 1 bin/playlistclip.cpp
 1 assets/model/assetparametermodel.cpp
```

## Recommended Fix Pattern

```cpp
// BEFORE (crash-prone):
if (pCore->currentDoc()->closing) { ... }

// AFTER (safe):
auto *doc = pCore->currentDoc();
if (doc && doc->closing) { ... }
```

For functions that need the doc, early return:
```cpp
auto *doc = pCore->currentDoc();
if (!doc) {
    qWarning() << "No document available";
    return;
}
// ... use doc safely
```

## Priority for Fixing

1. **Immediate:** `jobs/` folder - background tasks most likely to race with project close
2. **High:** Destructors in `bin/` classes
3. **Medium:** Signal handlers in timeline/bin code
4. **Low:** UI dialogs (usually modal, project can't close while open)

## Related Patterns

Also check for:
- `pCore->projectManager()->current()->...` (3 occurrences)
- Direct `m_document->` access in classes that cache the pointer
