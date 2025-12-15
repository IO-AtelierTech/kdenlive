# Kdenlive WebSocket Fork - Development Commands
# Usage: just <command>
# Run 'just --list' to see all commands

# Default recipe: show help
default:
    @just --list

# ============================================================================
# Build Commands
# ============================================================================

# Build Kdenlive using Docker
build:
    cd docker && docker compose run --rm builder

# Build with local CMake (if dependencies installed)
build-local:
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --target kdenlive --parallel

# Build with debug symbols
build-debug:
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --target kdenlive --parallel

# Clean build directory
clean:
    rm -rf build docker/build-output

# Rebuild Docker image (no cache)
docker-rebuild:
    cd docker && docker compose build --no-cache

# ============================================================================
# Test Commands
# ============================================================================

# Run C++ unit tests
test:
    cd build && ctest -R rpctest --output-on-failure

# Run Python E2E tests (requires running Kdenlive)
e2e:
    cd tests/rpc && python test_rpc_client.py

# Validate RPC schema
schema-validate:
    python tests/rpc/validate_schema.py

# ============================================================================
# Code Quality
# ============================================================================

# Run clang-format check on RPC code
lint:
    find src/rpc -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror

# Format RPC code with clang-format
format:
    find src/rpc -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Full pre-commit check (lint + build + test)
check: lint build-local test

# ============================================================================
# Debug Commands
# ============================================================================

# Start GDB debugging session
gdb:
    gdb ./build/bin/kdenlive

# Start GDB with Docker
gdb-docker:
    cd docker && docker compose run --rm gdb

# Interactive Docker shell
shell:
    cd docker && docker compose run --rm shell

# Run Kdenlive from build
run:
    ./build/bin/kdenlive

# ============================================================================
# Git Commands
# ============================================================================

# Push to GitHub (primary target) - default push
push branch="":
    #!/usr/bin/env bash
    if [ -z "{{branch}}" ]; then
        git push github
    else
        git push github {{branch}}
    fi

# Push to GitHub with tags
push-tags:
    git push github --tags

# Push all (master + feature/websocket + tags)
push-all:
    git push github master
    git push github feature/websocket
    git push github --tags

# Sync with upstream Kdenlive
sync-upstream:
    git fetch upstream
    git checkout master
    git merge upstream/master --ff-only
    git checkout feature/websocket
    git rebase master
    @echo "Sync complete. Run 'just push' to push changes."

# ============================================================================
# Release Commands
# ============================================================================

# Tag a new release
tag version:
    git tag -a {{version}} -m "Release {{version}}"
    @echo "Created tag {{version}}. Run 'just push-tags' to push."

# ============================================================================
# Octopus Parallel Development
# ============================================================================

# Show octopus status
octopus-status:
    @bash ~/.claude/skills/octopus-dev/scripts/status.sh 2>/dev/null || echo "Octopus scripts not found"

# Spawn a new tentacle worktree
octopus-spawn id scope description="":
    bash ~/.claude/skills/octopus-dev/scripts/spawn-tentacle.sh {{id}} "{{scope}}" "{{description}}"

# Merge a completed tentacle
octopus-merge id:
    bash ~/.claude/skills/octopus-dev/scripts/merge-tentacle.sh {{id}}

# Check for stale tentacles
octopus-stale hours="2":
    bash ~/.claude/skills/octopus-dev/scripts/check-stale.sh {{hours}}

# List worktrees
worktrees:
    git worktree list
