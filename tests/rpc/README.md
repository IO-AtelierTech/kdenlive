# Kdenlive RPC WebSocket Tests

Python test suite for the Kdenlive JSON-RPC 2.0 WebSocket API.

## Setup

```bash
cd tests/rpc
pip install -r requirements.txt
```

## Running Tests

1. **Start Kdenlive** with a project open (or create a new one)

2. **Run all tests:**
   ```bash
   python test_rpc_client.py
   ```

3. **Run specific test class:**
   ```bash
   python test_rpc_client.py TestConnection
   python test_rpc_client.py TestTimeline
   ```

4. **Run specific test:**
   ```bash
   python test_rpc_client.py TestConnection.test_ping
   ```

## Interactive Mode

For manual testing and exploration:

```bash
python test_rpc_client.py --interactive
```

Example commands:
```
>>> rpc.ping
>>> rpc.getVersion
>>> rpc.getCapabilities
>>> project.getInfo
>>> timeline.getTracks
>>> timeline.getClips
>>> effect.listAvailable
>>> render.getPresets
>>> quit
```

With parameters:
```
>>> project.open {"path": "/path/to/project.kdenlive"}
>>> timeline.seek {"position": 100}
>>> asset.search {"query": "blur"}
```

## Test Coverage

| Namespace | Tests |
|-----------|-------|
| `rpc.*` | ping, getVersion, getCapabilities, subscribe/unsubscribe |
| `project.*` | getInfo, undo, redo |
| `timeline.*` | getInfo, getTracks, getClips, getPosition |
| `bin.*` | listClips, listFolders |
| `effect.*` | listAvailable |
| `asset.*` | listCategories, search |
| `render.*` | getPresets, getJobs |

## Connection Details

- **Default port:** 9876
- **Protocol:** WebSocket + JSON-RPC 2.0
- **Host:** localhost (local connections only)
