# QA Strategy - Kdenlive JSON-RPC WebSocket API

## Goal

Ensure the WebSocket/JSON-RPC addition:
1. Doesn't create debugging nightmares
2. Follows Kdenlive upstream conventions exactly
3. Has comprehensive test coverage
4. Is acceptable as a PR to upstream (for bug fixes only)

---

## 1. Kdenlive Coding Conventions (MUST FOLLOW)

### 1.1 Code Style
- **Run clang-format** before every commit using repo's `.clang-format`
- **Indentation**: 4 spaces (no tabs)
- **Column limit**: 160 characters
- **Pointer alignment**: Right (`Type *ptr`)
- **Brace style**: Break after class/function, not after namespace

### 1.2 Naming Conventions
- **Classes**: PascalCase (`RpcServer`, `TimelineHandler`)
- **Methods**: camelCase (`handleGetInfo`, `dispatch`)
- **Member variables**: m_ prefix (`m_server`, `m_handlers`)
- **Constants**: UPPER_SNAKE_CASE or constexpr

### 1.3 Qt Conventions
- Use `QStringLiteral()` for string literals
- Prefer signals/slots over callbacks
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)

---

## 2. Testing Strategy

### 2.1 C++ Unit Tests
- Location: `tests/rpctest.cpp`
- Framework: Catch2 (existing in project)
- Coverage: RpcTypes, RpcDispatcher, RpcNotifier
- Run: `just test` or `cd build && ctest -R rpctest`

### 2.2 Python E2E Tests
- Location: `tests/rpc/`
- Tests all 86 RPC methods
- Interactive mode for manual testing
- Run: `just e2e` or `cd tests/rpc && python test_rpc_client.py`

### 2.3 Schema Validation
- Source of truth: `src/rpc/rpc-schema.json5`
- Validation script: `tests/rpc/validate_schema.py`
- Run: `just schema-validate`

### 2.4 Build Verification
- Docker build: `just build`
- Local build: `just build-local`
- Pre-commit check: `just check` (lint + build + test)

---

## 3. Pre-Commit Checklist

```bash
# Run all checks
just check
```

Or manually:
- [ ] `just lint` - clang-format clean
- [ ] `just build-local` - Builds without errors
- [ ] `just test` - C++ tests pass
- [ ] `just e2e` - Python E2E tests pass (requires running Kdenlive)

---

## 4. MCP Integration Testing

The MCP integration (Python client) tests the API from an end-user perspective.

### Current Status
- 54/72 tools working (75%)
- See `TODO.md` for known issues

### Testing Workflow
1. Build and run Kdenlive: `just run`
2. Run MCP session in separate terminal
3. Test problematic workflows
4. If crash: follow `dev-docs/debugging-workflow.md`

---

## 5. Upstream Submission Strategy

### Bug Fixes (Submit to Upstream)
Bug fixes outside `src/rpc/` should be cherry-picked and submitted to upstream:
- See `dev-docs/upstream-pr-plan.md` for tracking
- Submit as individual MRs to KDE GitLab
- Include crash backtrace in MR description

### RPC Feature (Fork Only)
The RPC feature itself (`src/rpc/`) stays in this fork. Upstream submission would require:
1. Engaging maintainers on Matrix #kdenlive-dev:kde.org
2. Reference issue #1615 (command-line rendering request)
3. Compelling use cases (MCP, batch processing)

---

## 6. Release Checklist

Before tagging a release:

- [ ] All C++ tests pass
- [ ] All Python E2E tests pass
- [ ] clang-format clean
- [ ] No compiler warnings in RPC code
- [ ] API documentation up to date (`dev-docs/rpc-api.md`)
- [ ] Schema reflects implementation (`src/rpc/rpc-schema.json5`)
- [ ] TODO.md reflects current status
- [ ] CI builds successfully (Linux and Windows)

---

*Last updated: December 2024*
