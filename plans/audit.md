# Project Audit Report

**Date:** 2026-07-28
**Scope:** Full project audit — bugs, security, performance, UX, build system

---

## Executive Summary

| Category | Critical | High | Medium | Low |
|----------|----------|------|--------|-----|
| Security (XSS) | 2 | — | 1 | — |
| Bugs / Logic Errors | 2 | 3 | 5 | 3 |
| Error Handling | 2 | 2 | 4 | 2 |
| API Compatibility | — | 3 | 5 | — |
| Performance | — | 1 | 3 | 1 |
| Build System | — | 2 | 2 | 3 |
| UX | — | 1 | 3 | 2 |
| Accessibility | — | — | 4 | 4 |

---

## P0 — Must Fix (Security / Data Loss)

### XSS via `marked.parse()` + `innerHTML`

**File:** `web/index.html:636, 776, 784, 787, 805, 811, 820`

`marked.parse()` passes raw HTML through by default. In marked v12 there is no built-in sanitizer. If the LLM returns `<img src=x onerror=alert(document.cookie)>`, it executes in the user's browser.

Every call to `formatMessage()` → `marked.parse()` followed by `.innerHTML = ...` is vulnerable.

**Fix:** Sanitize with DOMPurify before inserting:
```js
const clean = DOMPurify.sanitize(marked.parse(content));
```

---

### XSS via `escapeForJs` in HTML attribute context

**File:** `web/index.html:627, 648–649`

The escaped content is interpolated into an `onclick` attribute:
```html
onclick="app.copyToClipboard('ESCAPED_CONTENT', 'copy-0')"
```

`escapeForJs` escapes `"` to `\"`, but HTML attribute parsers don't recognize `\` as an escape character. A `"` in AI output (e.g., `he said "hello"`) terminates the attribute, breaking out of the `onclick`.

**Fix:** Don't use inline `onclick`. Use event delegation or store content in `data-*` attributes:
```js
// Store: data-content="base64..."
// Read: atob(el.dataset.content)
```

---

### SSE chunk splitting across TCP boundaries

**File:** `web/index.html:757`

```js
const lines = chunk.split('\n').filter(line => line.trim());
```

SSE messages can span multiple TCP chunks. A single `data: {...}` line arriving in two chunks will be split and `JSON.parse` will fail silently, losing streaming data.

**Fix:** Maintain a line buffer across chunks:
```js
// In the outer scope:
let sseBuffer = '';
// In the loop:
sseBuffer += chunk;
const lines = sseBuffer.split('\n');
sseBuffer = lines.pop(); // incomplete line stays in buffer
```

---

### `stopRequested_` not reset before non-streaming `chat()`

**File:** `src/main.cpp:100–112`

`stopRequested_` is set to `true` by `stopStreaming()`, then only reset to `false` in `chatStreaming()` (line 121). If a non-streaming `chat()` runs after a streaming cancellation, the flag is stale. If `response()` ever checks it, output will be silently truncated.

**Fix:** Reset at the start of `chat()`:
```cpp
stopRequested_ = false;
```

---

### `getExeDir()` double `dirname()` — undefined behavior

**File:** `src/main.cpp:271–273`

`dirname()` may mutate its argument in place or return a pointer to a static internal buffer. Calling it twice on the same `buf` can return a dangling pointer or the grandparent directory.

**Fix:** Use `std::filesystem::path`:
```cpp
std::string getExeDir() {
    return std::filesystem::path(program_invocation_name).parent_path().string();
}
```

---

## P1 — Should Fix

### `std::stoi` without try/catch

**File:** `src/main.cpp:291`

If a user passes `-p abc`, `std::stoi` throws `std::invalid_argument`. The `catch` at line 403 only covers the `/v1/chat/completions` handler — this crashes `main()`.

**Fix:** Wrap in try/catch:
```cpp
try { port = std::stoi(portArg); }
catch (...) { port = 8080; }
```

---

### No body size limit — OOM via JSON

**File:** `src/main.cpp:341`

`json::parse(req.body)` allocates memory proportional to request body size. An attacker can send a multi-gigabyte body to cause OOM.

**Fix:** Add body size limit:
```cpp
svr.set_payload_max_length(1024 * 1024); // 1MB
```

---

### No `JSON.parse` error handling in localStorage loads

**File:** `web/index.html:288, 297, 320`

If localStorage contains corrupted JSON, `JSON.parse` throws an uncaught exception that halts the entire `init()` flow — the app becomes non-functional.

**Fix:** Wrap each in try/catch with fallback:
```js
try { stored = JSON.parse(raw); }
catch { stored = default; }
```

---

### `confirmRename` null dereference

**File:** `web/index.html:545`

```js
const conv = this.conversations.find(c => c.id === this.renameConversationId);
conv.title = newTitle; // throws if conv is undefined
```

**Fix:** Guard:
```js
if (!conv) return;
```

---

### Streaming writes to detached DOM after conversation switch

**File:** `web/index.html:719–824`

If the user switches conversations while streaming, `renderMessages()` destroys the DOM container. The streaming closure still holds references to detached elements and continues writing to invisible nodes.

**Fix:** Check if the stream's conversation is still active before writing to DOM:
```js
if (this.currentConversationId !== convId) break;
```

---

### `LLM_LIBRARY` found but never linked

**File:** `src/CMakeLists.txt:39`

`find_library(LLM_LIBRARY llm)` assigns a result, but `target_link_libraries` only links `llm` by name. If the fallback path is taken, linking may fail silently.

**Fix:** Use the found library:
```cmake
target_link_libraries(mnn-server MNN ${LLM_LIBRARY} Threads::Threads)
```

---

### `local` outside function scope in start.sh

**File:** `scripts/start.sh:519, 534`

`local` in the main `while` loop is a bashism that produces warnings in some shells.

**Fix:** Move to functions or remove `local` keyword.

---

## P2 — Nice to Fix

### API Compatibility: Missing `finish_reason` in final streaming chunk

**File:** `src/main.cpp:246–265`

The OpenAI SSE spec requires `finish_reason: "stop"` in the final chunk before `[DONE]`. Its absence confuses some clients.

---

### API Compatibility: No `usage` stats in streaming

OpenAI's streaming API sends `usage` in the final chunk when `stream_options: {"include_usage": true}` is set.

---

### API Compatibility: No `temperature`, `top_p`, `frequency_penalty` support

**File:** `src/main.cpp:358`

These parameters are silently ignored. Should either be passed to the model or rejected.

---

### `scanModels()` re-reads directory on every call

**File:** `src/main.cpp:143`

Called on every `GET /v1/models`. Cache the result and invalidate on model load.

---

### `GET /` re-reads `index.html` on every request

**File:** `src/main.cpp:324–328`

The web UI is static — serve it once or cache in memory.

---

### Mutex serializes all inference

**File:** `src/main.cpp:120`

`chatStreaming()` holds the mutex for the entire generation duration. All other requests are serialized. On a multi-core server, this is a throughput bottleneck.

---

### `source "$CONFIG_FILE"` is code injection

**File:** `scripts/start.sh:20`

The config file is sourced directly. If it contains arbitrary bash commands, they execute.

**Fix:** Parse with `grep`/`read` instead of sourcing.

---

### Identical if/else branches in start.sh

**File:** `scripts/start.sh:125–128`

Both branches run the identical command. The condition is pointless.

---

### Hardcoded repo name in CI

**File:** `.github/workflows/build.yml:123`

`gh release upload --repo "abelbour/mnn-llm-server"` should use `${{ github.repository }}`.

---

### No CI for PRs or pushes

**File:** `.github/workflows/build.yml`

The workflow only runs on `release` events. Broken code can be merged without checks.

---

## P3 — Low Priority / Cosmetic

### Mixed naming conventions in main.cpp

- `llm_`, `modelPath_`, `modelsDir_` — trailing underscore
- `modelList`, `stream_buffer` — snake_case
- `parseMessages`, `buildChatCompletion` — camelCase

---

### `getModelName()` not const

**File:** `src/main.cpp:88` — doesn't modify state.

---

### `configPath.back()` on potentially empty string

**File:** `src/main.cpp:52` — undefined behavior if `modelPath` is empty.

---

### No modal keyboard handling (Escape to close)

**File:** `web/index.html` — all modals lack Escape key support.

---

### No ARIA roles on modals

Modals lack `role="dialog"`, `aria-modal="true"`, and `aria-labelledby`.

---

### Emoji-only buttons lack accessible labels

Buttons like `✏️`, `💾`, `🗑️`, `⚙` have no `aria-label`.

---

### Conversation items not keyboard-accessible

**File:** `web/index.html:518` — divs with `onclick` have no `tabindex`.

---

### Color contrast failure

**File:** `web/index.html:14` — `#6e7681` on `#0d1117` has ~2.8:1 ratio (WCAG AA requires 4.5:1).

---

### `URL.revokeObjectURL` called synchronously after `click()`

**File:** `web/index.html:946` — may cause download to fail in some browsers. Use `setTimeout`.

---

### `pthread` hardcoded in CMake

**File:** `src/CMakeLists.txt:53` — not portable to macOS. Use `find_package(Threads)`.

---

### `add_definitions()` scoped to entire project

**File:** `src/CMakeLists.txt:7` — should use `target_compile_definitions`.

---

### `local` outside function in start.sh

**File:** `scripts/start.sh:519, 534` — bashism.

---

### `grep -c "^"` on empty string returns 1

**File:** `scripts/start.sh:253` — `|| echo 0` fallback never triggers.

---

### Binary copied to project root on every start

**File:** `scripts/start.sh:107` — run from `bin/` directly instead.

---

### Missing `libs/*.a` in .gitignore

**File:** `.gitignore:14` — only matches `.so`.

---

### Hardcoded `/root/MNN` fallback in CMake

**File:** `src/CMakeLists.txt:13` — should default to `$ENV{HOME}/MNN`.

---

### `MNN_BACKEND=cpu` set in postinst (wrong phase)

**File:** `.github/workflows/build.yml:104` — has no effect at install time.

---

### No error handling on API fetch in CI

**File:** `.github/workflows/build.yml:22` — if GitHub API fails, `VERSION` is empty.

---

## New Feature Suggestions

| Feature | Description | Priority |
|---------|-------------|----------|
| **Context window management** | Truncate oldest messages when approaching model's max context. Prevents garbage output on long conversations. | High |
| **Model switching API** | `POST /v1/models/load` endpoint to hot-swap models without restart. | Medium |
| **Request timeout** | Configurable per-request timeout to prevent runaway generation. | Medium |
| **Streaming abort via API** | `POST /v1/chat/completions/{id}/cancel` — client-side cancellation endpoint. | Medium |
| **Conversation export to Markdown** | Export individual conversations as `.md` files with thinking blocks. | Low |
| **System prompt configuration** | Allow setting default system prompt via settings instead of first message. | Low |
| **Rate limiting** | Per-IP or per-key rate limiting to prevent abuse. | Low |
| **Health check endpoint** | `GET /health` with model status, memory usage, uptime. | Low |
| **Usage statistics** | Track tokens per request for monitoring and budgeting. | Low |
| **Dark/light theme toggle** | Currently hardcoded dark theme. | Low |

---

## File Inventory

| File | Lines | Status |
|------|-------|--------|
| `src/main.cpp` | 427 | 3 high, 4 medium, 5 low |
| `web/index.html` | 969 | 2 critical, 3 high, 5 medium, 6 low |
| `src/CMakeLists.txt` | 55 | 2 high, 2 medium, 3 low |
| `scripts/start.sh` | ~600 | 2 high, 2 medium, 4 low |
| `.github/workflows/build.yml` | ~130 | 1 high, 1 medium, 2 low |
| `.gitignore` | ~20 | 1 low |
