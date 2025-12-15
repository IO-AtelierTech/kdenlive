# Upstream Sync Workflow

This document describes how to keep the fork synchronized with upstream Kdenlive.

## Repository Structure

### Remotes

| Remote | URL | Purpose |
|--------|-----|---------|
| `upstream` | https://invent.kde.org/multimedia/kdenlive.git | Original Kdenlive (read-only) |
| `origin` | https://invent.kde.org/danyielcolin/kdenlive-websocket.git | KDE GitLab fork (for upstream PRs) |
| `github` | https://github.com/IO-AtelierTech/kdenlive.git | **Primary push target** (CI/artifacts) |

> **Note:** Default pushes go to `github`. The `origin` remote is used for submitting PRs to upstream Kdenlive.

### Branches

| Branch | Purpose | Sync Strategy |
|--------|---------|---------------|
| `master` | Base branch, mirrors upstream | Merge from upstream |
| `feature/websocket` | RPC development branch | Rebase on master |

## Sync Workflow

### Step 1: Fetch Upstream

```bash
git fetch upstream
git fetch origin
```

### Step 2: Update Master

```bash
git checkout master
git merge upstream/master --ff-only
```

If fast-forward fails (shouldn't happen if master is clean):
```bash
git reset --hard upstream/master
```

### Step 3: Rebase Feature Branch

```bash
git checkout feature/websocket
git rebase master
```

### Step 4: Resolve Conflicts (if any)

If conflicts occur:

1. **src/rpc/ conflicts**: Our code, keep ours
2. **Other src/ conflicts**: Evaluate carefully - may need to update our null-pointer fixes
3. **CMakeLists.txt conflicts**: Merge both changes

```bash
# During rebase
git status                    # See conflicted files
# Edit files to resolve
git add <resolved-files>
git rebase --continue
```

### Step 5: Push Updates

```bash
# Push to GitHub (primary)
git push github master
git push github feature/websocket --force-with-lease

# Optionally push to GitLab (for upstream PRs)
git push origin master
git push origin feature/websocket --force-with-lease
```

## Handling Breaking Changes

### If Upstream Changes Files We Modified

Check `dev-docs/upstream-pr-plan.md` for files we've touched outside `src/rpc/`.

```bash
# See what upstream changed in files we care about
git diff master..upstream/master -- src/bin/bin.cpp src/bin/clipcreator.cpp
```

If upstream fixed the same bugs:
- Remove from our upstream-pr-plan.md
- Verify our changes are no longer needed

If upstream changed code we depend on:
- Update our fix to match new code structure
- Test thoroughly

### If Our PRs Get Merged Upstream

When an upstream PR is merged:

1. Fetch upstream: `git fetch upstream`
2. Check the commit: `git log upstream/master --oneline -- <file>`
3. If merged, remove from `upstream-pr-plan.md`
4. Test that our RPC code still works

## Periodic Sync Schedule

Recommended: Sync with upstream **weekly** or before major development.

```bash
# Quick sync script
git fetch upstream
git checkout master && git merge upstream/master --ff-only
git checkout feature/websocket && git rebase master
# Test build
cmake --build build --target kdenlive
```

## Conflict Resolution Guidelines

### Priority Order

1. **Keep RPC functionality working** - Never break the API
2. **Preserve crash fixes** - Our null-pointer fixes are important
3. **Adopt upstream improvements** - If upstream improves code we touch, adapt

### Common Conflict Scenarios

#### Scenario 1: Upstream changed function we added null checks to

```cpp
// Upstream changed function signature
void SomeClass::method(int newParam)

// We had added null checks
void SomeClass::method()
{
    auto *doc = pCore->currentDoc();
    if (!doc) return;  // Our fix
    ...
}
```

**Resolution:** Add our null check to the new version:
```cpp
void SomeClass::method(int newParam)
{
    auto *doc = pCore->currentDoc();
    if (!doc) return;  // Keep our fix
    ... // Use new upstream code
}
```

#### Scenario 2: Upstream added RPC-like feature

If upstream adds similar functionality:
- Evaluate if we can use their implementation
- Consider making our RPC a thin wrapper around their code
- Or keep ours if significantly different

## Automation (Justfile)

```bash
# Add to Justfile
sync-upstream:
    git fetch upstream
    git checkout master
    git merge upstream/master --ff-only
    git checkout feature/websocket
    git rebase master
    @echo "Sync complete. Test build and push manually."
```

## Quick Reference

```bash
# Full sync
git fetch upstream
git checkout master && git merge upstream/master --ff-only
git push github master  # Primary push target
git checkout feature/websocket && git rebase master
git push github feature/websocket --force-with-lease

# Check upstream changes to our files
git diff master..upstream/master -- src/bin/ src/monitor/ src/timeline2/

# See when upstream was last synced
git log master --oneline -5

# Abort failed rebase
git rebase --abort
```

---

*Created: December 2024*
