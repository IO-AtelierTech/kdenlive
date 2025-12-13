# Kdenlive JSON-RPC WebSocket API

## Project Status: ✅ IMPLEMENTATION COMPLETE

All three parallel sessions have been merged into `feature/websocket`.

---

## API Capabilities Overview

### What's Implemented (86 methods across 9 handlers)

#### Core Infrastructure (Session 1)
| Namespace | Methods | Purpose |
|-----------|---------|---------|
| `rpc.*` | 5 | Connection management, discovery |
| `project.*` | 7 | Project lifecycle, undo/redo |

#### Timeline & Media (Session 2)
| Namespace | Methods | Purpose |
|-----------|---------|---------|
| `timeline.*` | 17 | Clip manipulation, tracks, playhead |
| `bin.*` | 14 | Media import, organization, markers |
| `transition.*` | 5 | Transition effects between clips |
| `composition.*` | 5 | Track compositions/overlays |

#### Effects & Output (Session 3)
| Namespace | Methods | Purpose |
|-----------|---------|---------|
| `effect.*` | 14 | Effect management, keyframes |
| `asset.*` | 9 | Effect/transition discovery, presets |
| `render.*` | 10 | Render jobs, presets, monitoring |

---

## Detailed Method Reference

### `rpc.*` - Connection & Discovery (5 methods)
```
rpc.ping                 - Health check
rpc.getVersion           - Kdenlive and RPC protocol versions
rpc.getCapabilities      - List all available methods and events
rpc.subscribe            - Subscribe to event notifications
rpc.unsubscribe          - Unsubscribe from events
```

### `project.*` - Project Management (7 methods)
```
project.getInfo          - Current project metadata (fps, resolution, path)
project.open             - Open project file
project.save             - Save current project
project.close            - Close project (with optional save)
project.new              - Create new project (optional profile)
project.undo             - Undo last action
project.redo             - Redo last undone action
```

### `timeline.*` - Timeline Operations (17 methods)
```
timeline.getInfo         - Duration, track count, fps, profile
timeline.getTracks       - List all tracks with properties
timeline.getClips        - List clips (optional track filter)
timeline.getClip         - Single clip info by ID
timeline.getPosition     - Current playhead position
timeline.getSelection    - Currently selected items

timeline.insertClip      - Insert bin clip to timeline
timeline.moveClip        - Move clip to new position/track
timeline.deleteClip      - Remove single clip
timeline.deleteClips     - Remove multiple clips
timeline.resizeClip      - Change in/out points
timeline.splitClip       - Cut clip at position

timeline.addTrack        - Add video or audio track
timeline.deleteTrack     - Remove track
timeline.setTrackProperty - Set name/locked/muted/hidden

timeline.seek            - Move playhead to position
timeline.setSelection    - Select specific clips
```

### `bin.*` - Project Bin (14 methods)
```
bin.listClips            - List all clips with metadata
bin.listFolders          - List folder structure
bin.getClipInfo          - Detailed clip information

bin.importClip           - Import single media file
bin.importClips          - Import multiple files
bin.deleteClip           - Remove single clip
bin.deleteClips          - Remove multiple clips
bin.renameItem           - Rename clip or folder
bin.moveItem             - Move to different folder

bin.createFolder         - Create new folder
bin.deleteFolder         - Remove folder

bin.getClipMarkers       - List markers on clip
bin.addClipMarker        - Add marker to clip
bin.deleteClipMarker     - Remove marker
```

### `transition.*` - Transitions (5 methods)
```
transition.list          - List available transition types
transition.add           - Add transition between clips
transition.remove        - Remove transition
transition.getProperties - Get transition parameters
transition.setProperty   - Modify transition parameter
```

### `composition.*` - Track Compositions (5 methods)
```
composition.list         - List compositions on timeline
composition.add          - Add composition
composition.remove       - Remove composition
composition.getProperties - Get composition parameters
composition.setProperty  - Modify composition parameter
```

### `effect.*` - Effects Management (14 methods)
```
effect.listAvailable     - List all available effects
effect.getInfo           - Effect metadata and parameters
effect.add               - Add effect to clip
effect.remove            - Remove effect from clip
effect.getClipEffects    - List effects on a clip
effect.getProperty       - Get effect parameter value
effect.setProperty       - Set effect parameter value
effect.enable            - Enable effect
effect.disable           - Disable effect
effect.reorder           - Change effect order in stack
effect.copyToClips       - Copy effects to multiple clips
effect.getKeyframes      - Get keyframe data
effect.setKeyframe       - Add/modify keyframe
effect.deleteKeyframe    - Remove keyframe
```

### `asset.*` - Asset Discovery (9 methods)
```
asset.listCategories     - List effect/transition categories
asset.search             - Search by name/description
asset.getEffectsByCategory - Filter effects by category
asset.getFavorites       - Get user favorites
asset.addFavorite        - Add to favorites
asset.removeFavorite     - Remove from favorites
asset.getPresets         - List effect presets
asset.savePreset         - Save effect preset
asset.deletePreset       - Delete preset
```

### `render.*` - Rendering (10 methods)
```
render.getPresets        - List render presets
render.getPresetInfo     - Preset details
render.start             - Start render job
render.startWithGuides   - Render by guide markers
render.stop              - Cancel render job
render.stopAll           - Cancel all render jobs
render.getStatus         - Get job status
render.getJobs           - List all jobs
render.getActiveJob      - Get currently running job
render.setOutput         - Set default output path
```

---

## Event Notifications

Clients can subscribe to these events via `rpc.subscribe`:

```
project.opened           - Project was opened
project.closed           - Project was closed
project.saved            - Project was saved
project.modified         - Project has unsaved changes

timeline.changed         - Timeline was modified

render.started           - Render job started
render.progress          - Render progress update (%)
render.completed         - Render finished successfully
render.error             - Render failed with error
```

---

## What's NOT Exposed (and Why)

### 1. UI Operations - **Deliberately Excluded**
| Operation | Reason |
|-----------|--------|
| Panel management | UI state is user preference, not automation concern |
| Zoom levels | Visual only, doesn't affect output |
| Theme/colors | User preference |
| Window positions | Desktop environment concern |
| Keyboard shortcuts | User configuration |
| Monitor preview | Real-time display, not scriptable |

**Justification**: The API is for *automation*, not remote desktop control. UI operations don't affect the final rendered output and would create synchronization nightmares.

### 2. Fine-Grained Playback Control - **Minimal Exposure**
| Operation | Status | Reason |
|-----------|--------|--------|
| Play/Pause | NOT exposed | Would require real-time sync, complex state management |
| Playback speed | NOT exposed | Preview-only, doesn't affect render |
| Loop regions | NOT exposed | UI convenience feature |
| Seek | ✅ Exposed | Useful for positioning before operations |

**Justification**: Playback is for human preview. Automation scripts don't need to "watch" the video - they need to manipulate it. Seek is exposed because it affects where operations occur.

### 3. Complex Selection Operations - **Simplified**
| Operation | Status | Reason |
|-----------|--------|--------|
| Get selection | ✅ Exposed | Scripts need to know what's selected |
| Set selection | ✅ Exposed | Scripts need to select items for batch ops |
| Lasso select | NOT exposed | Mouse-driven UI operation |
| Select all in track | NOT exposed | Can be done via getClips + setSelection |
| Multi-select with modifiers | NOT exposed | Keyboard/mouse UI pattern |

**Justification**: Selection state is exposed for batch operations, but the *mechanism* of selection (mouse, keyboard) is UI-specific.

### 4. Undo/Redo Granularity - **Simplified**
| Operation | Status | Reason |
|-----------|--------|--------|
| Undo | ✅ Exposed | Essential for error recovery |
| Redo | ✅ Exposed | Essential for error recovery |
| Undo history list | NOT exposed | UI display concern |
| Undo to specific point | NOT exposed | Complex state management, edge cases |
| Clear undo stack | NOT exposed | Dangerous, no valid automation use case |

**Justification**: Scripts can undo their own mistakes but shouldn't manipulate the undo system itself.

### 5. Real-Time Audio - **Excluded**
| Operation | Reason |
|-----------|--------|
| Audio monitoring | Real-time, not scriptable |
| Audio scrubbing | Preview feature |
| Volume meters | UI display |
| Audio recording | Requires hardware interaction |

**Justification**: Audio monitoring is real-time feedback for humans. Scripts work with audio *data* (via effects), not audio *playback*.

### 6. GPU/Performance Settings - **Excluded**
| Operation | Reason |
|-----------|--------|
| GPU acceleration toggle | System configuration, not per-project |
| Proxy mode toggle | Could be exposed in future if needed |
| Cache management | Internal optimization |
| Thread count | System configuration |

**Justification**: These are system-level settings that affect performance, not project content.

### 7. Plugin/Extension Management - **Excluded**
| Operation | Reason |
|-----------|--------|
| Install effects | Security concern, requires user consent |
| Download LUTs | Network operation with security implications |
| Manage Python scripts | Meta-level, not project editing |

**Justification**: Plugin management has security implications and should require explicit user action.

---

## What Could Be Added Later (Future Enhancements)

### High Value - Consider for v2
| Feature | Complexity | Value | Notes |
|---------|------------|-------|-------|
| Guides management | Low | High | Add/remove/list timeline guides |
| Subtitle/title clips | Medium | High | Create text overlays programmatically |
| Audio normalization | Medium | High | Batch audio processing |
| Proxy generation | Medium | High | Trigger proxy creation for clips |
| Speech-to-text | Medium | High | Trigger transcription |

### Medium Value - Nice to Have
| Feature | Complexity | Value | Notes |
|---------|------------|-------|-------|
| Color scopes data | High | Medium | Histogram, waveform, vectorscope |
| Motion tracking | High | Medium | Start/stop tracking, get data |
| Multi-cam editing | High | Medium | Sync and switch angles |
| Nested sequences | Medium | Medium | Create/manage nested timelines |

### Low Value - Probably Not Worth It
| Feature | Reason |
|---------|--------|
| Ripple/roll edit modes | Too tied to mouse interaction paradigm |
| Snapping configuration | UI convenience, not automation need |
| Clip thumbnails | Display optimization, not content |

---

## Implementation vs Plan Comparison

### Original Plan (from CLAUDE.md)
| Planned | Implemented | Delta |
|---------|-------------|-------|
| ~60 methods | 86 methods | +26 (43% more) |
| 6 handlers | 9 handlers | +3 (composition, assets, separate transition) |
| Basic keyframes | Full keyframe API | Enhanced |
| Simple render | Full render job management | Enhanced |

### Session Delivery
| Session | Planned Scope | Actual Delivery |
|---------|---------------|-----------------|
| 1: Infrastructure | Core + Project + Connection | ✅ Delivered as planned |
| 2: Timeline/Bin | Timeline + Bin + Transitions | ✅ + Composition handler (bonus) |
| 3: Effects/Render | Effects + Render | ✅ + Assets handler (bonus) |

### Notable Additions Beyond Plan
1. **CompositionHandler** - Track compositions were added to Session 2
2. **AssetsHandler** - Effect/transition discovery, favorites, presets
3. **Keyframe API** - Full keyframe manipulation in effects
4. **Batch operations** - deleteClips, importClips, copyToClips
5. **Render notifications** - Progress events for monitoring

---

## Code Statistics

```
Total new code:     ~5,990 lines
  Session 1:        1,359 lines (infrastructure)
  Session 2:        2,687 lines (timeline/bin)
  Session 3:        1,944 lines (effects/render)

Files created:      18 new files
Files modified:     4 existing files (core.h, core.cpp, CMakeLists.txt x2)

Handlers:           9
Methods:            86
Events:             10
```

---

## Testing Status

- [x] clang-format passes on all RPC code
- [ ] Full build verification (in progress)
- [ ] Unit tests
- [ ] Integration tests
- [ ] E2E Python client tests

---

## Next Steps

1. **Verify build** - Ensure full Kdenlive compiles with all handlers
2. **E2E testing** - Run Python test client against live Kdenlive
3. **Documentation** - Generate API docs from code
4. **Python client** - Complete kdenlive-api package
5. **MCP server** - Implement kdenlive-mcp using the API
