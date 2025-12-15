# CI Status

## Current State

| Platform | Status | Notes |
|----------|--------|-------|
| Linux | Failing | Qt6 not in cache, build from source fails |
| Windows | Failing | Same cache issue |

## Issue Details

### Problem
CraftMaster cannot find Qt6 in the KDE binary cache and falls back to building from source. The source build fails because Qt6 configure can't find `xkbcommon-x11` even though the package is installed.

### Error (Linux)
```
ERROR: Feature "xcb": Forcing to "ON" breaks its condition:
    QT_FEATURE_xkbcommon_x11 = "OFF"
```

### Root Cause
The KDE Craft cache at `https://files.kde.org/craft/Qt6/25.09/` doesn't have pre-built binaries for the current configuration. When Craft falls back to building Qt6 from source, it fails because the GitHub runner's system libraries aren't properly found by Qt's cmake configure.

### Attempted Fixes
1. `e1d004d31a` - Added Qt6 build dependencies (libxkbcommon-x11-dev etc.)
2. `2c5074ff24` - Let Craft use default CacheVersion (25.09)
3. `09ee0f4474` - Fixed Android env vars that confused platform detection
4. `92dfb0d5a7` - Tried Qt6/25.12 cache path
5. `fc657b1e94` - Various cache URL fixes

### Potential Solutions

#### Option 1: Use a different Craft cache version
Try different cache versions that might have pre-built Qt6:
- `25.09`, `25.12`, `master`

#### Option 2: Use system Qt6
Instead of CraftMaster, use the system Qt6 packages:
```yaml
- name: Install Qt6
  run: sudo apt-get install qt6-base-dev qt6-websockets-dev ...
```

#### Option 3: Use aqtinstall
Use the `jurplel/install-qt-action` to install Qt6 from qt.io:
```yaml
- uses: jurplel/install-qt-action@v3
  with:
    version: '6.5.3'
```

#### Option 4: Wait for KDE cache update
The KDE build infrastructure may update the cache. Check:
- https://invent.kde.org/kde/craftmaster

### Workaround

For now, local builds using Docker work:
```bash
just build
```

The CI failure doesn't block development. Artifacts can be built locally and uploaded manually if needed.

## Action Items

- [ ] Investigate which Craft cache version has Qt6 binaries
- [ ] Consider switching to `jurplel/install-qt-action` for Qt6
- [ ] Test with system Qt6 packages on Ubuntu
- [ ] Monitor KDE Craft infrastructure for updates

---

*Last updated: December 2024*
