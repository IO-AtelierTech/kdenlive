# Kdenlive JSON-RPC WebSocket API

External automation interface for Kdenlive using JSON-RPC 2.0 over WebSocket.

> **Note:** The canonical API specification is defined in [`src/rpc/rpc-schema.json5`](../../src/rpc/rpc-schema.json5). This document provides human-readable documentation with examples.

## Table of Contents

- [Quick Start](#quick-start)
- [Connection](#connection)
- [Authentication](#authentication)
- [Request Format](#request-format)
- [Response Format](#response-format)
- [Error Codes](#error-codes)
- [Event Subscriptions](#event-subscriptions)
- [API Reference](#api-reference)
- [Examples](#examples)
- [Configuration](#configuration)

---

## Quick Start

```python
import asyncio
import websockets
import json

async def main():
    async with websockets.connect("ws://localhost:9876") as ws:
        # Ping the server
        await ws.send(json.dumps({
            "jsonrpc": "2.0",
            "method": "rpc.ping",
            "id": 1
        }))
        response = json.loads(await ws.recv())
        print(response)  # {"jsonrpc": "2.0", "result": {"pong": true}, "id": 1}

asyncio.run(main())
```

---

## Connection

| Property     | Value                      |
| ------------ | -------------------------- |
| Protocol     | WebSocket                  |
| Default Port | 9876                       |
| Host         | localhost (127.0.0.1 only) |
| URI          | `ws://localhost:9876`      |

The server only accepts connections from localhost for security. Remote connections are rejected.

**Single Client Model**: Only one client can be connected at a time. New connections will disconnect the existing client.

---

## Authentication

Authentication is optional and configured via Kdenlive settings.

If an auth token is configured, the first message must be an authentication request:

```json
{
  "jsonrpc": "2.0",
  "method": "rpc.auth",
  "params": { "token": "your-secret-token" },
  "id": 1
}
```

**Success response:**

```json
{ "jsonrpc": "2.0", "result": { "authenticated": true }, "id": 1 }
```

**Failure response:**

```json
{
  "jsonrpc": "2.0",
  "error": { "code": -32008, "message": "Invalid authentication token" },
  "id": 1
}
```

If no token is configured in settings, authentication is not required.

---

## Request Format

All requests follow JSON-RPC 2.0 specification:

```json
{
  "jsonrpc": "2.0",
  "method": "namespace.methodName",
  "params": {},
  "id": 1
}
```

| Field   | Type       | Required | Description                                               |
| ------- | ---------- | -------- | --------------------------------------------------------- |
| jsonrpc | string     | Yes      | Must be "2.0"                                             |
| method  | string     | Yes      | Method name in "namespace.method" format                  |
| params  | object     | No       | Method parameters (defaults to {})                        |
| id      | int/string | No       | Request identifier. Omit for notifications (no response). |

---

## Response Format

**Success:**

```json
{
    "jsonrpc": "2.0",
    "result": { ... },
    "id": 1
}
```

**Error:**

```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32601,
        "message": "Method not found",
        "data": { ... }
    },
    "id": 1
}
```

---

## Error Codes

### Standard JSON-RPC Errors

| Code   | Name             | Description                |
| ------ | ---------------- | -------------------------- |
| -32700 | Parse Error      | Invalid JSON               |
| -32600 | Invalid Request  | Not a valid request object |
| -32601 | Method Not Found | Method does not exist      |
| -32602 | Invalid Params   | Invalid method parameters  |
| -32603 | Internal Error   | Internal server error      |

### Kdenlive-Specific Errors

| Code   | Name                | Description                             |
| ------ | ------------------- | --------------------------------------- |
| -32001 | Project Not Open    | No project is currently open            |
| -32002 | Clip Not Found      | Referenced clip not found               |
| -32003 | Track Not Found     | Referenced track not found              |
| -32004 | Effect Not Found    | Referenced effect not found             |
| -32005 | Render In Progress  | Cannot start render while one is active |
| -32006 | Invalid Path        | Invalid file path                       |
| -32007 | Operation Failed    | Generic operation failure               |
| -32008 | Unauthorized        | Authentication required or failed       |
| -32009 | Application Closing | Application is shutting down            |
| -32010 | Timeline Not Ready  | Timeline not initialized or transitioning |
| -32011 | Window Not Available| Main window not available               |

---

## Event Subscriptions

Subscribe to events to receive real-time notifications:

```json
{
  "jsonrpc": "2.0",
  "method": "rpc.subscribe",
  "params": { "events": ["project.saved", "render.progress"] },
  "id": 1
}
```

### Available Events

| Event              | Description                 | Data                                         |
| ------------------ | --------------------------- | -------------------------------------------- |
| `project.opened`   | Project was opened          | `{path: string}`                             |
| `project.closed`   | Project was closed          | `{}`                                         |
| `project.saved`    | Project was saved           | `{path: string}`                             |
| `project.modified` | Project has unsaved changes | `{modified: bool}`                           |
| `timeline.changed` | Timeline was modified       | `{}`                                         |
| `render.started`   | Render job started          | `{jobId: string, outputPath: string}`        |
| `render.progress`  | Render progress update      | `{jobId: string, progress: int, frame: int}` |
| `render.completed` | Render finished             | `{jobId: string, outputPath: string}`        |
| `render.error`     | Render failed               | `{jobId: string, error: string}`             |

### Notification Format

Notifications are sent without an `id` field:

```json
{
  "jsonrpc": "2.0",
  "method": "render.progress",
  "params": { "jobId": "abc123", "progress": 45, "frame": 1350 }
}
```

---

## API Reference

### `rpc.*` - Connection & Discovery

#### rpc.ping

Health check.

**Request:** `{}`
**Response:** `{"pong": true}`

#### rpc.getVersion

Get version information.

**Request:** `{}`
**Response:**

```json
{
  "kdenlive": "24.12.0",
  "rpcVersion": "1.0",
  "mlt": "7.24.0"
}
```

#### rpc.getCapabilities

List all available methods and events.

**Request:** `{}`
**Response:**

```json
{
    "methods": ["rpc.ping", "rpc.getVersion", "project.getInfo", ...],
    "events": ["project.opened", "render.progress", ...]
}
```

#### rpc.subscribe

Subscribe to event notifications.

**Request:** `{"events": ["project.saved", "render.progress"]}`
**Response:** `{"subscribed": true}`

#### rpc.unsubscribe

Unsubscribe from events.

**Request:** `{"events": ["render.progress"]}`
**Response:** `{"unsubscribed": true}`

---

### `project.*` - Project Management

#### project.getInfo

Get current project information.

**Request:** `{}`
**Response:**

```json
{
  "path": "/home/user/project.kdenlive",
  "name": "project",
  "modified": false,
  "fps": 29.97,
  "width": 1920,
  "height": 1080,
  "duration": 36000
}
```

#### project.open

Open a project file.

**Request:** `{"path": "/path/to/project.kdenlive"}`
**Response:** `{"opened": true}`

#### project.save

Save current project.

**Request:** `{}` or `{"path": "/path/to/save.kdenlive"}`
**Response:** `{"saved": true, "path": "..."}`

#### project.close

Close current project.

**Request:** `{"save": true}` (optional, defaults to prompting)
**Response:** `{"closed": true}`

#### project.new

Create a new project.

**Request:** `{"profile": "atsc_1080p_25"}` (optional)
**Response:** `{"created": true}`

#### project.undo

Undo last action.

**Request:** `{}`
**Response:** `{"undone": true}` or error if nothing to undo

#### project.redo

Redo last undone action.

**Request:** `{}`
**Response:** `{"redone": true}` or error if nothing to redo

---

### `timeline.*` - Timeline Operations

#### timeline.getInfo

Get timeline information.

**Request:** `{}`
**Response:**

```json
{
  "duration": 36000,
  "videoTracks": 3,
  "audioTracks": 2,
  "fps": 29.97,
  "position": 1500
}
```

#### timeline.getTracks

List all tracks.

**Request:** `{}`
**Response:**

```json
{
  "tracks": [
    {
      "id": 1,
      "name": "Video 1",
      "type": "video",
      "locked": false,
      "muted": false,
      "hidden": false
    },
    {
      "id": 2,
      "name": "Audio 1",
      "type": "audio",
      "locked": false,
      "muted": false,
      "hidden": false
    }
  ]
}
```

#### timeline.getClips

List clips on timeline.

**Request:** `{}` or `{"trackId": 1}`
**Response:**

```json
{
  "clips": [
    {
      "id": 100,
      "binId": "3",
      "trackId": 1,
      "position": 0,
      "duration": 150,
      "in": 0,
      "out": 149,
      "name": "clip.mp4"
    }
  ]
}
```

#### timeline.insertClip

Insert a clip from bin to timeline.

**Request:**

```json
{
  "binId": "3",
  "trackId": 1,
  "position": 1000,
  "in": 0,
  "out": 149
}
```

**Response:** `{"clipId": 100, "inserted": true}`

#### timeline.moveClip

Move a clip.

**Request:** `{"clipId": 100, "trackId": 2, "position": 500}`
**Response:** `{"moved": true}`

#### timeline.deleteClip

Delete a clip.

**Request:** `{"clipId": 100}`
**Response:** `{"deleted": true}`

#### timeline.resizeClip

Resize a clip.

**Request:** `{"clipId": 100, "in": 10, "out": 140}`
**Response:** `{"resized": true}`

#### timeline.splitClip

Split a clip at position.

**Request:** `{"clipId": 100, "position": 75}`
**Response:** `{"newClipId": 101, "split": true}`

#### timeline.seek

Move playhead to position.

**Request:** `{"position": 1500}`
**Response:** `{"position": 1500}`

#### timeline.getPosition

Get current playhead position.

**Request:** `{}`
**Response:** `{"position": 1500, "fps": 29.97}`

#### timeline.addTrack

Add a new track.

**Request:** `{"type": "video", "name": "Video 4"}`
**Response:** `{"trackId": 4, "added": true}`

#### timeline.deleteTrack

Delete a track.

**Request:** `{"trackId": 4}`
**Response:** `{"deleted": true}`

---

### `bin.*` - Project Bin

#### bin.listClips

List clips in project bin.

**Request:** `{}` or `{"folderId": "2"}`
**Response:**

```json
{
  "clips": [
    {
      "id": "3",
      "name": "video.mp4",
      "type": "video",
      "duration": 300,
      "folderId": "1",
      "path": "/path/to/video.mp4"
    }
  ]
}
```

#### bin.listFolders

List folders in project bin.

**Request:** `{}`
**Response:**

```json
{
  "folders": [
    { "id": "1", "name": "Root", "parentId": null },
    { "id": "2", "name": "Footage", "parentId": "1" }
  ]
}
```

#### bin.importClip

Import a single media file.

**Request:** `{"url": "/path/to/video.mp4", "folderId": "2"}`
**Response:** `{"clipId": "5"}`

| Parameter | Type   | Default  | Description            |
| --------- | ------ | -------- | ---------------------- |
| url       | string | required | Path to the media file |
| folderId  | string | root     | Target folder ID       |

**Async Loading:** The server returns immediately with the clipId. The clip loads asynchronously in the background (thumbnail, waveform generation). Poll `bin.getClipInfo` and check the `loading` field to know when the clip is fully ready.

```python
# Python client example with waiting
clip_id = await client.bin.import_clip(path)  # client polls until ready

# Or check loading status manually
clip_id = await client.bin.import_clip(path, wait_for_ready=False)
while True:
    info = await client.bin.get_clip_info(clip_id)
    if not info.loading:
        break
    await asyncio.sleep(0.1)
```

#### bin.importClips

Import multiple media files.

**Request:** `{"urls": ["/path/to/video1.mp4", "/path/to/video2.mp4"], "folderId": "2"}`
**Response:** `{"clipIds": ["5", "6"]}`

| Parameter | Type     | Default  | Description          |
| --------- | -------- | -------- | -------------------- |
| urls      | string[] | required | Paths to media files |
| folderId  | string   | root     | Target folder ID     |

**Async Loading:** All clips are imported immediately and load in parallel. Poll `bin.getClipInfo` for each clip to check loading status.

**Limitations:**

- **Do not import the same file multiple times** in a single call - causes clips to hang. Use different files.

#### bin.deleteClip

Delete a single clip from bin.

**Request:** `{"clipId": "3"}`
**Response:** `{"deleted": true}`

#### bin.deleteClips

Delete multiple clips from bin.

**Request:** `{"clipIds": ["3", "4", "5"]}`
**Response:** `{"count": 3}`

#### bin.createFolder

Create a new folder.

**Request:** `{"name": "B-Roll", "parentId": "1"}`
**Response:** `{"folderId": "3", "created": true}`

#### bin.getClipMarkers

Get markers on a bin clip.

**Request:** `{"clipId": "3"}`
**Response:**

```json
{
  "markers": [{ "position": 100, "comment": "Good take", "type": 0 }]
}
```

#### bin.addClipMarker

Add a marker to a bin clip.

**Request:** `{"clipId": "3", "position": 150, "comment": "Sync point", "type": 1}`
**Response:** `{"added": true}`

---

### `effect.*` - Effects Management

#### effect.listAvailable

List all available effects.

**Request:** `{}` or `{"type": "video"}` or `{"category": "blur"}`
**Response:**

```json
{
  "effects": [
    { "id": "frei0r.blur", "name": "Blur", "type": "video", "category": "blur" }
  ]
}
```

#### effect.add

Add effect to a clip.

**Request:** `{"clipId": 100, "effectId": "frei0r.blur"}`
**Response:** `{"effectIndex": 0, "added": true}`

#### effect.remove

Remove effect from clip.

**Request:** `{"clipId": 100, "effectIndex": 0}`
**Response:** `{"removed": true}`

#### effect.getClipEffects

List effects on a clip.

**Request:** `{"clipId": 100}`
**Response:**

```json
{
  "effects": [
    { "index": 0, "id": "frei0r.blur", "name": "Blur", "enabled": true }
  ]
}
```

#### effect.setProperty

Set effect parameter.

**Request:** `{"clipId": 100, "effectIndex": 0, "property": "amount", "value": 0.5}`
**Response:** `{"set": true}`

#### effect.enable / effect.disable

Enable or disable effect.

**Request:** `{"clipId": 100, "effectIndex": 0}`
**Response:** `{"enabled": true}` / `{"disabled": true}`

---

### `render.*` - Rendering

#### render.getPresets

List available render presets.

**Request:** `{}`
**Response:**

```json
{
  "presets": [
    { "id": "x264", "name": "H.264/AAC", "extension": "mp4" },
    { "id": "webm", "name": "VP9/Opus", "extension": "webm" }
  ]
}
```

#### render.start

Start a render job.

**Request:**

```json
{
  "preset": "x264",
  "output": "/path/to/output.mp4",
  "in": 0,
  "out": -1
}
```

**Response:** `{"jobId": "abc123", "started": true}`

#### render.getStatus

Get render job status.

**Request:** `{"jobId": "abc123"}`
**Response:**

```json
{
  "jobId": "abc123",
  "status": "running",
  "progress": 45,
  "frame": 1350,
  "totalFrames": 3000
}
```

#### render.stop

Cancel a render job.

**Request:** `{"jobId": "abc123"}`
**Response:** `{"stopped": true}`

#### render.getJobs

List all render jobs.

**Request:** `{}`
**Response:**

```json
{
  "jobs": [
    { "jobId": "abc123", "status": "running", "progress": 45 },
    {
      "jobId": "def456",
      "status": "completed",
      "output": "/path/to/output.mp4"
    }
  ]
}
```

---

## Examples

### Python: Batch Import and Organize

```python
import asyncio
import websockets
import json
from pathlib import Path

async def batch_import(folder_path):
    async with websockets.connect("ws://localhost:9876") as ws:
        # Create folder in bin
        await ws.send(json.dumps({
            "jsonrpc": "2.0",
            "method": "bin.createFolder",
            "params": {"name": "Imported"},
            "id": 1
        }))
        response = json.loads(await ws.recv())
        folder_id = response["result"]["folderId"]

        # Import all video files
        for video in Path(folder_path).glob("*.mp4"):
            await ws.send(json.dumps({
                "jsonrpc": "2.0",
                "method": "bin.importClip",
                "params": {"path": str(video), "folderId": folder_id},
                "id": 2
            }))
            result = json.loads(await ws.recv())
            print(f"Imported: {video.name} -> {result}")

asyncio.run(batch_import("/path/to/videos"))
```

### Python: Render with Progress Monitoring

```python
import asyncio
import websockets
import json

async def render_with_progress(output_path):
    async with websockets.connect("ws://localhost:9876") as ws:
        # Subscribe to render events
        await ws.send(json.dumps({
            "jsonrpc": "2.0",
            "method": "rpc.subscribe",
            "params": {"events": ["render.progress", "render.completed", "render.error"]},
            "id": 1
        }))
        await ws.recv()  # subscription confirmation

        # Start render
        await ws.send(json.dumps({
            "jsonrpc": "2.0",
            "method": "render.start",
            "params": {"preset": "x264", "output": output_path},
            "id": 2
        }))
        result = json.loads(await ws.recv())
        job_id = result["result"]["jobId"]
        print(f"Render started: {job_id}")

        # Monitor progress
        while True:
            msg = json.loads(await ws.recv())
            if "method" in msg:  # notification
                if msg["method"] == "render.progress":
                    print(f"Progress: {msg['params']['progress']}%")
                elif msg["method"] == "render.completed":
                    print(f"Render complete: {msg['params']['outputPath']}")
                    break
                elif msg["method"] == "render.error":
                    print(f"Render failed: {msg['params']['error']}")
                    break

asyncio.run(render_with_progress("/home/user/output.mp4"))
```

### JavaScript/Node.js: Interactive Timeline Control

```javascript
const WebSocket = require("ws");

const ws = new WebSocket("ws://localhost:9876");
let requestId = 0;

function call(method, params = {}) {
  return new Promise((resolve, reject) => {
    const id = ++requestId;
    ws.send(JSON.stringify({ jsonrpc: "2.0", method, params, id }));

    const handler = (data) => {
      const msg = JSON.parse(data);
      if (msg.id === id) {
        ws.off("message", handler);
        msg.error ? reject(msg.error) : resolve(msg.result);
      }
    };
    ws.on("message", handler);
  });
}

ws.on("open", async () => {
  // Get project info
  const info = await call("project.getInfo");
  console.log("Project:", info.name);

  // List timeline clips
  const clips = await call("timeline.getClips");
  console.log("Clips:", clips.clips.length);

  // Seek to middle
  const duration = (await call("timeline.getInfo")).duration;
  await call("timeline.seek", { position: Math.floor(duration / 2) });

  ws.close();
});
```

---

## Implementation Guidelines

### Async Operations: Fire-and-Forget Pattern

When implementing RPC handlers for operations that trigger background tasks (like `ClipLoadTask`, `TranscodeTask`, etc.), **never wait on the main thread**.

**The Problem:** Kdenlive uses `Qt::BlockingQueuedConnection` for worker threads to call back to the main thread. If the RPC handler tries to wait using `qApp->processEvents()`, it creates a deadlock:

```
1. Handler calls operation that starts background task
2. Handler calls processEvents() to wait
3. Background task tries to call main thread via BlockingQueuedConnection
4. BlockingQueuedConnection blocks waiting for main thread
5. Main thread is stuck in processEvents() → DEADLOCK
```

**The Solution:** Return immediately from the handler and let clients poll for status:

```cpp
// WRONG - causes deadlock
auto BinHandler::handleImportClip(const QJsonObject &params) -> QJsonObject
{
    QString clipId = ClipCreator::createClipFromFile(...);

    // DON'T DO THIS - deadlock with ClipLoadTask
    while (clip->clipStatus() == FileStatus::StatusWaiting) {
        qApp->processEvents(QEventLoop::AllEvents, 100);
    }

    return result;
}

// CORRECT - fire and forget
auto BinHandler::handleImportClip(const QJsonObject &params) -> QJsonObject
{
    QString clipId = ClipCreator::createClipFromFile(...);

    // Return immediately - client will poll getClipInfo
    return QJsonObject{{QStringLiteral("result"),
        QJsonObject{{QStringLiteral("clipId"), clipId}}}};
}

// Provide status in getClipInfo
auto BinHandler::handleGetClipInfo(const QJsonObject &params) -> QJsonObject
{
    FileStatus::ClipStatus status = clip->clipStatus();
    if (status == FileStatus::StatusWaiting) {
        clipInfo[QStringLiteral("loading")] = true;
        // Use placeholder values for producer-dependent properties
    } else {
        clipInfo[QStringLiteral("loading")] = false;
        clipInfo[QStringLiteral("duration")] = clip->frameDuration();
    }
    return QJsonObject{{QStringLiteral("result"), clipInfo}};
}
```

**Client-side:** The Python client handles waiting with async polling:

```python
async def import_clip(self, url, wait_for_ready=True, timeout=10.0):
    result = await self._client.call("bin.importClip", {"url": url})
    clip_id = result.get("clipId", "")

    if clip_id and wait_for_ready:
        start = asyncio.get_event_loop().time()
        while asyncio.get_event_loop().time() - start < timeout:
            info = await self.get_clip_info(clip_id)
            if not info.loading:
                break
            await asyncio.sleep(0.1)  # Properly yields control

    return clip_id
```

This pattern keeps the API simple for users while avoiding Qt threading issues.

---

## Configuration

RPC settings can be configured in Kdenlive:

**Settings > Configure Kdenlive > RPC** (or via `kdenlivesettings.kcfg`):

| Setting        | Description                            | Default |
| -------------- | -------------------------------------- | ------- |
| `rpcEnabled`   | Enable/disable RPC server              | `true`  |
| `rpcPort`      | WebSocket port number                  | `9876`  |
| `rpcAuthToken` | Authentication token (empty = no auth) | ``      |

### CMake Build Option

The RPC feature can be disabled at compile time:

```bash
cmake -DENABLE_RPC=OFF ..
```

---

## Troubleshooting

### Connection refused

- Ensure Kdenlive is running
- Check if RPC is enabled in settings
- Verify the port number (default: 9876)
- Only localhost connections are allowed

### Authentication failed

- Check `rpcAuthToken` setting in Kdenlive
- First message must be `rpc.auth` if token is set

### Method not found

- Use `rpc.getCapabilities` to list available methods
- Check method name spelling and namespace

### Project not open error (-32001)

- Many methods require an open project
- Use `project.open` or create a new project first
