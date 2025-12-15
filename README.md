![](data/pics/kdenlive-logo.png)

# Kdenlive WebSocket Fork

[![Build Status](https://github.com/IO-AtelierTech/kdenlive/actions/workflows/build.yml/badge.svg)](https://github.com/IO-AtelierTech/kdenlive/actions/workflows/build.yml)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

> **This is a fork of Kdenlive** that adds a JSON-RPC 2.0 WebSocket API for automation. The goal is to enable AI-powered and script-based video editing workflows via an MCP (Model Context Protocol) server.

## WebSocket RPC API

This fork exposes **86 methods** across 9 handlers for programmatic control of Kdenlive:

| Namespace | Methods | Purpose |
|-----------|---------|---------|
| `rpc.*` | 5 | Connection, discovery, events |
| `project.*` | 7 | Project lifecycle, undo/redo |
| `timeline.*` | 17 | Clip manipulation, tracks, playhead |
| `bin.*` | 14 | Media import, organization |
| `effect.*` | 14 | Effects, keyframes |
| `asset.*` | 9 | Effect discovery, presets |
| `render.*` | 10 | Render jobs, monitoring |
| `transition.*` | 5 | Same-track transitions |
| `composition.*` | 5 | Cross-track compositions |

### Quick Start

```python
import asyncio
import websockets
import json

async def main():
    async with websockets.connect("ws://localhost:9876") as ws:
        await ws.send(json.dumps({
            "jsonrpc": "2.0",
            "method": "rpc.ping",
            "id": 1
        }))
        print(await ws.recv())  # {"jsonrpc":"2.0","result":{"pong":true},"id":1}

asyncio.run(main())
```

### Documentation

- **API Reference**: [`dev-docs/rpc-api.md`](dev-docs/rpc-api.md)
- **Schema (Source of Truth)**: [`src/rpc/rpc-schema.json5`](src/rpc/rpc-schema.json5)
- **Implementation Status**: [`TODO.md`](TODO.md)

### Configuration

RPC settings in Kdenlive: **Settings > Configure Kdenlive > RPC**

| Setting | Default | Description |
|---------|---------|-------------|
| `rpcEnabled` | `true` | Enable/disable server |
| `rpcPort` | `9876` | WebSocket port |
| `rpcAuthToken` | (empty) | Optional auth token |

Compile-time disable: `cmake -DENABLE_RPC=OFF ..`

---

# Kdenlive

Kdenlive is a powerful, free and open-source video editor that brings professional-grade video editing capabilities to everyone. Whether you're creating a simple family video or working on a complex project, Kdenlive provides the tools you need to bring your vision to life.

For more information about Kdenlive's features, tutorials, and community, please visit our [official website](https://kdenlive.org).

There you can also find downloads for both stable releases and experimental daily builds for Kdenlive.

## Contributing to Kdenlive

Kdenlive is a community-driven project, and we welcome contributions from everyone! There are many ways to contribute beyond coding:

- Help translate Kdenlive into your language
- Report and triage bugs
- Write documentation
- Create tutorials
- Help other users on forums and bug trackers

Visit [kdenlive.org](https://kdenlive.org) to learn more about non-code contributions.

## Developer Information

### Technology Stack

Kdenlive is written in C++ and is using these technologies and frameworks:

- **Core Framework**: MLT for video editing functionality
- **GUI Framework**: Qt and KDE Frameworks 6
- **Additional Libraries**: frei0r (video effects), LADSPA (audio effects)

### Getting Started

1. Check out our [build instructions](dev-docs/build.md) to set up your development environment
2. Familiarize yourself with the [architecture](dev-docs/architecture.md) and [coding guidelines](dev-docs/coding.md)
4. If the MLT library is new to you check out [MLT Introduction](dev-docs/mlt-intro.md)
3. Join our Matrix channel `#kdenlive-dev:kde.org` for developer discussions and support

### Contributing Code

Kdenlive's primary development happens on [KDE Invent](https://invent.kde.org/multimedia/kdenlive). While we maintain a GitHub mirror, all code contributions should be submitted through KDE's GitLab instance. For more information about KDE's development infrastructure, visit the [KDE GitLab documentation](https://community.kde.org/Infrastructure/GitLab).

### Finding Things to Work On

- Browse open issues on [KDE Invent](https://invent.kde.org/multimedia/kdenlive/-/issues)
- Check the [KDE Bug Tracker](https://bugs.kde.org) for reported issues
- Look for issues tagged with "good first issue" or "help wanted"

Need help getting started? Join our Matrix channel `#kdenlive-dev:kde.org` - our community is friendly and always ready to help new contributors!
