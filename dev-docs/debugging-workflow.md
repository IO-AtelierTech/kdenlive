# Debugging Workflow

This document describes the workflow for debugging crashes discovered during MCP/RPC testing.

## Overview

When testing the RPC API via MCP, crashes can occur in Kdenlive. The debugging cycle is:

```
MCP Session → Discovers Crash → Triage → GDB Session → Fix → Verify → Iterate
```

## Step 1: Triage the Error

When MCP reports an error, first determine if it's:

### API Error (MCP/Python side)
- Parameter validation failures
- Network/connection issues
- Python client bugs

**Indicators:**
- Error message from MCP tool itself
- No Kdenlive crash
- Response contains clear API error code

**Action:** Fix in MCP Python client, not Kdenlive.

### Server Crash (Kdenlive side)
- Null pointer access
- Race condition
- Assertion failure

**Indicators:**
- Kdenlive process terminates
- Connection drops unexpectedly
- No response from server

**Action:** Debug in Kdenlive with GDB.

## Step 2: Start GDB Session

### Build with Debug Symbols

```bash
# Using Justfile (recommended)
just build-debug

# Or manually
cmake -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build --target kdenlive
```

### Launch with GDB

```bash
# Using Justfile
just gdb

# Or manually
gdb ./build/bin/kdenlive
```

### GDB Setup

```gdb
# Set breakpoints on common crash points
break abort
break __assert_fail

# Catch signals
catch signal SIGSEGV
catch signal SIGABRT

# Run
run
```

## Step 3: Reproduce the Crash

With Kdenlive running under GDB:

1. Start the MCP server (or Python test client)
2. Execute the problematic sequence of operations
3. Wait for the crash

### Using Test Suite

```bash
# In another terminal
cd tests/rpc
python test_rpc_client.py
```

### Using MCP Session

```bash
# In another terminal
cd ~/Working/playground/video-edit-template
# Run problematic MCP workflow
```

## Step 4: Capture Backtrace

When GDB stops at the crash:

```gdb
# Full backtrace
bt full

# Shorter version
bt

# See specific frame
frame 5

# Print variables
print pCore->currentDoc()
print m_document
```

### Common Patterns

**Null pointer in pCore->currentDoc():**
```
#0  0x... in KdenliveDoc::... ()
#1  0x... in SomeClass::someMethod()
    doc = pCore->currentDoc()  # Returns null
    doc->something()           # SIGSEGV
```

**Race condition in async task:**
```
#0  0x... in ProjectClip::...
#1  0x... in ClipLoadTask::run()
    # Task completes after project closed
```

## Step 5: Copy Backtrace to Fix Session

Format the backtrace for sharing:

```markdown
## Crash Report

**Error:** SIGSEGV in SomeClass::someMethod

**Backtrace:**
```
#0 0x... in KdenliveDoc::property() at kdenlivedoc.cpp:123
#1 0x... in SomeClass::someMethod() at somefile.cpp:456
#2 0x... in RPCHandler::handle() at rpchandler.cpp:78
```

**Analysis:**
- pCore->currentDoc() returns null when project is closing
- someMethod() doesn't check for null before calling doc->property()

**Proposed Fix:**
```cpp
auto *doc = pCore->currentDoc();
if (!doc) {
    return;  // or return error
}
doc->property();
```
```

## Step 6: Implement Fix

### Common Fix Patterns

**Null check pattern:**
```cpp
// BEFORE (crash-prone)
if (pCore->currentDoc()->closing) { ... }

// AFTER (safe)
auto *doc = pCore->currentDoc();
if (doc && doc->closing) { ... }
```

**Early return pattern:**
```cpp
auto *doc = pCore->currentDoc();
if (!doc) {
    qWarning() << "No document available";
    return QJsonObject{{QStringLiteral("error"), QStringLiteral("No document")}};
}
// ... use doc safely
```

**Closing state check:**
```cpp
if (pCore->closing || (pCore->currentDoc() && pCore->currentDoc()->closing)) {
    return;
}
```

## Step 7: Verify Fix

```bash
# Rebuild
just build

# Or
cmake --build build --target kdenlive

# Test without GDB first
./build/bin/kdenlive &
cd tests/rpc && python test_rpc_client.py

# If still issues, go back to GDB
just gdb
```

## Step 8: Document and Commit

If the fix is outside `src/rpc/`:

1. Add to `dev-docs/null-pointer-crash-map.md` if it's a common pattern
2. Add to `dev-docs/upstream-pr-plan.md` for upstream submission
3. Commit with clear message:

```bash
git commit -m "Add null checks to prevent crash in <location>

<Description of bug>
<Backtrace summary>
<Fix explanation>"
```

## Quick Reference

### Justfile Commands

```bash
just build        # Build Kdenlive
just build-debug  # Build with debug symbols
just gdb          # Launch GDB session
just test         # Run C++ tests
just e2e          # Run Python E2E tests
```

### Common GDB Commands

| Command | Description |
|---------|-------------|
| `run` | Start program |
| `continue` / `c` | Continue after breakpoint |
| `bt` | Backtrace |
| `bt full` | Backtrace with local variables |
| `frame N` | Switch to frame N |
| `print VAR` | Print variable |
| `info threads` | List threads |
| `thread N` | Switch to thread N |
| `quit` | Exit GDB |

### Common Crash Locations

See `dev-docs/null-pointer-crash-map.md` for:
- High-risk async/background tasks
- Destructor patterns
- Signal/slot callbacks
- Preview/render operations

---

*Created: December 2024*
