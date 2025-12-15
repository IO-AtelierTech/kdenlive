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

# Spawn a tentacle session (creates worktree from spec in master-todo.md)
tentacle-spawn id:
    #!/usr/bin/env bash
    set -e
    if [ ! -f .octopus/master-todo.md ]; then
        echo "Error: .octopus/master-todo.md not found"
        exit 1
    fi
    # Check if tentacle spec exists
    if ! grep -q "^### {{id}}:" .octopus/master-todo.md; then
        echo "Error: Tentacle '{{id}}' not found in master-todo.md"
        echo "Available tentacles:"
        grep -E '^### t[0-9]+-' .octopus/master-todo.md | sed 's/### /  /'
        exit 1
    fi
    # Extract description from header line
    desc=$(grep "^### {{id}}:" .octopus/master-todo.md | sed 's/^### {{id}}: //')
    # Extract scope (lines between **Scope:** and next **)
    scope=$(awk '/^### {{id}}:/,/^---$/' .octopus/master-todo.md | awk '/^\*\*Scope:\*\*/,/^\*\*/' | grep -E '^- ' | sed 's/^- //' | tr '\n' ',' | sed 's/,$//')
    # Create worktree
    worktree_path=".worktrees/{{id}}"
    branch="tentacle/{{id}}"
    if [ -d "$worktree_path" ]; then
        echo "Worktree already exists at $worktree_path"
        echo "To enter: cd $worktree_path"
        exit 0
    fi
    echo "Creating tentacle worktree..."
    echo "  ID: {{id}}"
    echo "  Description: $desc"
    echo "  Scope: $scope"
    echo "  Branch: $branch"
    echo "  Path: $worktree_path"
    git worktree add -b "$branch" "$worktree_path" feature/websocket
    # Create TODO.md in worktree
    cat > "$worktree_path/TODO.md" << EOF
    # Tentacle: {{id}}
    ## $desc

    **Scope:** $scope

    ## Tasks
    $(awk '/^### {{id}}:/,/^---$/' .octopus/master-todo.md | awk '/^\*\*Tasks:\*\*/,/^\*\*|^---/' | grep -E '^[0-9]+\.' | sed 's/^/- [ ] /')

    ---
    *Auto-generated from .octopus/master-todo.md*
    EOF
    # Create marker file
    touch "$worktree_path/.octopus-tentacle"
    echo ""
    echo "Tentacle spawned! To start working:"
    echo "  cd $worktree_path"

# Merge a completed tentacle back to feature/websocket
tentacle-merge id:
    #!/usr/bin/env bash
    set -e
    worktree_path=".worktrees/{{id}}"
    branch="tentacle/{{id}}"
    if [ ! -d "$worktree_path" ]; then
        echo "Error: Worktree not found at $worktree_path"
        exit 1
    fi
    echo "Merging tentacle {{id}}..."
    git checkout feature/websocket
    git merge --no-ff "$branch" -m "Merge tentacle/{{id}}"
    echo "Removing worktree..."
    git worktree remove "$worktree_path"
    git branch -d "$branch"
    echo "Tentacle {{id}} merged and cleaned up."

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
