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
# Tentacle Parallel Development
# ============================================================================

# List available tentacles and their statuses
tentacles:
    #!/usr/bin/env bash
    echo "=== Tentacle Status ==="
    echo ""
    if [ -f .octopus/master-todo.md ]; then
        echo "ACTIVE:"
        active=$(awk '/^## Active Tentacles/,/^## Ready to Spawn/' .octopus/master-todo.md | grep -E '^\| t[0-9]+-' | sed 's/|//g' | awk '{$1=$1};1')
        if [ -n "$active" ]; then
            echo "$active" | sed 's/^/  /'
        else
            echo "  (none)"
        fi
        echo ""
        echo "READY TO SPAWN:"
        grep -E '^### t[0-9]+-' .octopus/master-todo.md | sed 's/### /  /'
        echo ""
        echo "COMPLETED:"
        completed=$(awk '/^## Completed Tentacles/,/^---/' .octopus/master-todo.md | grep -E '^\| [^-]' | grep -v 'ID.*Description' | sed 's/|//g' | awk '{$1=$1};1')
        if [ -n "$completed" ]; then
            echo "$completed" | sed 's/^/  /'
        else
            echo "  (none)"
        fi
    else
        echo "No .octopus/master-todo.md found"
    fi

# Show details for a specific tentacle
tentacle-info id:
    #!/usr/bin/env bash
    if [ ! -f .octopus/master-todo.md ]; then
        echo "No .octopus/master-todo.md found"
        exit 1
    fi
    awk '/^### {{id}}:/,/^---$/' .octopus/master-todo.md | head -n -1

# Spawn a tentacle session (uses shared script from ~/.claude/skills/octopus-dev)
tentacle-spawn id:
    bash ~/.claude/skills/octopus-dev/scripts/spawn-tentacle.sh {{id}}

# Merge a completed tentacle back to feature/websocket
tentacle-merge id:
    bash ~/.claude/skills/octopus-dev/scripts/merge-tentacle.sh {{id}}

# Remove a tentacle worktree without merging
tentacle-remove id:
    #!/usr/bin/env bash
    worktree_path=".worktrees/{{id}}"
    branch="tentacle/{{id}}"
    if [ -d "$worktree_path" ]; then
        git worktree remove "$worktree_path" --force
        echo "Removed worktree at $worktree_path"
    fi
    if git show-ref --verify --quiet "refs/heads/$branch"; then
        git branch -D "$branch"
        echo "Deleted branch $branch"
    fi

# List all worktrees
worktrees:
    git worktree list
