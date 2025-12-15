# Docker Build Environment

This directory contains Docker configuration for building Kdenlive with RPC support.

## Quick Start

```bash
# From repo root (using Justfile)
just build

# Or directly with docker compose
cd docker
docker compose up builder
```

## Services

| Service | Purpose | Command |
|---------|---------|---------|
| `builder` | Full build | `docker compose up builder` |
| `shell` | Interactive shell | `docker compose run --rm shell` |
| `lint` | Clang-format check | `docker compose run --rm lint` |
| `test` | Run C++ tests | `docker compose run --rm test` |
| `gdb` | Debug with GDB | `docker compose run --rm gdb` |

## Build Output

Built files are in `docker/build-output/`:
- `bin/kdenlive` - Main executable
- `compile_commands.json` - For IDE integration

## Dependencies

The Dockerfile builds:
- Fedora 42 base
- All Kdenlive dependencies via `dnf builddep`
- Qt6 WebSockets (for RPC)
- KDDockWidgets 2.4.0 (newer than Fedora's 1.7.0)

## Rebuilding

```bash
# Rebuild without cache (e.g., after Dockerfile changes)
cd docker
docker compose build --no-cache
```

## Caching

A Docker volume `ccache` is used for compiler caching across builds.
