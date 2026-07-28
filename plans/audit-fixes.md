# Audit Fix Plan

**Source:** `plans/audit.md`
**Created:** 2026-07-28

---

## Phase 1 — P0 Security & Data Loss Fixes

**Goal:** Eliminate XSS vulnerabilities and critical bugs.

### 1a. Sanitize `marked.parse()` output

**File:** `web/index.html`

Add DOMPurify via CDN `<script>` tag. Wrap all `marked.parse()` calls through a sanitize helper.

- Add `<script src="https://cdn.jsdelivr.net/npm/dompurify@3/dist/purify.min.js"></script>` after `marked.js`
- Add helper: `function safeMarkdown(content) { return DOMPurify.sanitize(marked.parse(content)); }`
- Replace all `this.formatMessage(...)` innerHTML assignments to use `safeMarkdown`
- Affects lines: 636, 776, 784, 787, 805, 811, 820

### 1b. Fix `escapeForJs` XSS in HTML attributes

**File:** `web/index.html`

Replace inline `onclick` with event delegation using `data-*` attributes.

- Change copy button from `onclick="app.copyToClipboard('ESCAPED', 'id')"` to `data-copy="BASE64_ID"`
- Add event delegation on message container: `container.addEventListener('click', e => { if (e.target.dataset.copy) ... })`
- Decode via `atob(el.dataset.copy)` to get original text
- Remove `escapeForJs` function entirely
- Affects lines: 627, 648–649, 651–660

### 1c. Fix SSE chunk splitting

**File:** `web/index.html`

Add line buffer across SSE chunks.

- Add `let sseBuffer = ''` in scope before the while loop (line 739)
- In the chunk processing: `sseBuffer += chunk; const lines = sseBuffer.split('\n'); sseBuffer = lines.pop();`
- Filter and process `lines` as before
- Affects lines: 739, 757

### 1d. Fix `[DONE]` break scope

**File:** `web/index.html`

The `[DONE]` break only exits the inner for loop. Add a flag.

- Add `let done = false;` before the while loop
- Replace `if (data === '[DONE]') break;` with `if (data === '[DONE]') { done = true; break; }`
- Add `if (done) break;` after the for loop
- Affects lines: 739, 762

### 1e. Reset `stopRequested_` in `chat()`

**File:** `src/main.cpp`

Add `stopRequested_ = false;` at the start of `chat()`.

- Affects line: ~100

### 1f. Fix `getExeDir()` undefined behavior

**File:** `src/main.cpp`

Replace `dirname()` with `std::filesystem::path`.

- Add `#include <filesystem>` at top
- Replace `getExeDir()` body with: `return std::filesystem::path(program_invocation_name).parent_path().string();`
- Remove `#include <libgen.h>` if no longer needed
- Affects lines: 5, 271–276

### 1g. Add body size limit

**File:** `src/main.cpp`

Add `svr.set_payload_max_length(1024 * 1024);` before `svr.listen`.

- Affects line: ~420

### 1h. Fix `configPath.back()` on empty string

**File:** `src/main.cpp`

Add guard before `configPath.back()`:

```cpp
if (configPath.empty()) {
    configPath = getExeDir() + "/config.json";
}
```

- Affects lines: 52–54

---

## Phase 2 — P1 Critical Fixes

**Goal:** Fix crashes, data corruption, and build issues.

### 2a. Wrap `std::stoi` in try/catch

**File:** `src/main.cpp`

```cpp
try { port = std::stoi(portArg); }
catch (...) { std::cerr << "Invalid port, using 8080\n"; port = 8080; }
```

- Affects line: 291

### 2b. Fix `messages` type validation

**File:** `src/main.cpp`

In POST handler, after parsing body:

```cpp
if (!body.contains("messages") || !body["messages"].is_array()) {
    return sendJsonError(res, 400, "messages must be a JSON array");
}
```

- Affects line: ~357

### 2c. Clamp `max_tokens`

**File:** `src/main.cpp`

```cpp
int maxTokens = body.value("max_tokens", 256);
maxTokens = std::clamp(maxTokens, 1, 8192);
```

- Affects line: 358

### 2d. Fix `confirmRename` null guard

**File:** `web/index.html`

```js
const conv = this.conversations.find(c => c.id === this.renameConversationId);
if (!conv) { this.closeModal('renameModal'); return; }
```

- Affects line: 545

### 2e. Add `JSON.parse` error handling in localStorage

**File:** `web/index.html`

Wrap all `JSON.parse(stored)` calls in try/catch with default fallback.

- Affects lines: 288, 297, 320

### 2f. Fix streaming detached DOM writes

**File:** `web/index.html`

In the streaming token callback, check if conversation is still active:

```js
if (this.currentConversationId !== convId) { /* stop updating DOM */ }
```

- Affects lines: 776–820

### 2g. Fix CMake `LLM_LIBRARY` linking

**File:** `src/CMakeLists.txt`

```cmake
target_link_libraries(mnn-server MNN ${LLM_LIBRARY} Threads::Threads)
```

Replace `find_package(Threads REQUIRED)` for portability.

- Affects lines: 39, 49–53

### 2h. Fix `local` outside function in start.sh

**File:** `scripts/start.sh`

Move `local model=""` and `local lines="${1:-50}"` into functions, or remove `local`.

- Affects lines: 519, 534

### 2i. Fix identical if/else branches in start.sh

**File:** `scripts/start.sh`

Remove the redundant condition. Both branches run the same command.

- Affects lines: 125–128

### 2j. Fix `stop_server` status messages

**File:** `scripts/start.sh`

Check `pgrep` BEFORE `pkill`, then report based on the kill result.

- Affects lines: 150–156

---

## Phase 3 — P2 API & Performance

**Goal:** Improve OpenAI compatibility and server performance.

### 3a. Add `finish_reason: "stop"` in final streaming chunk

**File:** `src/main.cpp`

After `llm_->response()` completes in `chatStreaming()`, send one final SSE chunk:

```cpp
json finishChunk = buildChatCompletionChunk(id, "", "stop");
onToken("[SSE]" + finishChunk.dump() + "\n\n", false);
```

- Affects line: ~136

### 3b. Add `usage` stats in non-streaming response

**File:** `src/main.cpp`

Estimate tokens from output length (rough approximation) or leave as 0 with a comment.

- Affects lines: 237–241

### 3c. Warn on unsupported parameters

**File:** `src/main.cpp`

Log a warning for `temperature`, `top_p`, `frequency_penalty`, `presence_penalty` if present:

```cpp
for (auto param : {"temperature", "top_p", "frequency_penalty", "presence_penalty"}) {
    if (body.contains(param)) {
        std::cerr << "Warning: " << param << " is not supported, ignoring\n";
    }
}
```

- Affects line: ~358

### 3d. Cache `scanModels()` result

**File:** `src/main.cpp`

Add a `cachedModels_` string and `modelsCached_` bool. Only re-scan on model load.

- Affects line: 143

### 3e. Cache `index.html` in memory

**File:** `src/main.cpp`

Read and cache the web file once at startup, serve from memory on `GET /`.

- Affects lines: 324–328

### 3f. Add context window truncation

**File:** `src/main.cpp`

In `parseMessages()`, estimate token count (chars / 4) and truncate oldest messages when approaching a limit.

- Affects lines: 182–215

### 3g. Fix `config.json` source injection in start.sh

**File:** `scripts/start.sh`

Parse config with `grep`/`read` instead of `source`:

```bash
while IFS='=' read -r key value; do
    case "$key" in
        model_path) model_path="$value" ;;
        port) port="$value" ;;
    esac
done < "$CONFIG_FILE"
```

- Affects line: 20

---

## Phase 4 — P3 Polish

**Goal:** Code quality, accessibility, and portability.

### 4a. Rename `getModelName()` to const

**File:** `src/main.cpp`

Change `std::string getModelName()` to `std::string getModelName() const`.

- Affects line: 88

### 4b. Fix mixed naming conventions

**File:** `src/main.cpp`

Standardize on:
- Member variables: `camelCase_` with trailing underscore (keep as-is)
- Free functions: `camelCase` (keep as-is)
- Local variables: `camelCase` (fix `modelList` → `modelList`, `stream_buffer` → `streamBuffer`, `output_ostream` → `outputOstream`)

### 4c. Fix `GET /` 404 path leak

**File:** `src/main.cpp`

Return generic 404 instead of exposing `webPath`:

```cpp
return sendJsonError(res, 404, "Not found");
```

- Affects line: 331

### 4d. Add Escape key to close modals

**File:** `web/index.html`

```js
document.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
        document.querySelectorAll('.modal.active').forEach(m => m.classList.remove('active'));
    }
});
```

- Add in `app.init()` or inline script

### 4e. Add ARIA roles to modals

**File:** `web/index.html`

Add to all modal containers: `role="dialog" aria-modal="true" aria-labelledby="..."`.

### 4f. Add `aria-label` to icon-only buttons

**File:** `web/index.html`

Add `aria-label` attributes to all emoji buttons (edit, save, delete, settings, etc.).

### 4g. Add `tabindex="0"` to conversation items

**File:** `web/index.html`

Add `tabindex="0"` and keyboard event handlers (Enter/Space to select).

### 4h. Fix color contrast

**File:** `web/index.html`

Change `#6e7681` to `#8b949e` (better contrast on `#0d1117`).

- Affects line: 14

### 4i. Fix `URL.revokeObjectURL` timing

**File:** `web/index.html`

```js
setTimeout(() => URL.revokeObjectURL(url), 1000);
```

- Affects line: 946

### 4j. Fix CMake portability

**File:** `src/CMakeLists.txt`

- Change fallback from `/root/MNN` to `$ENV{HOME}/MNN`
- Use `find_package(Threads REQUIRED)` + `Threads::Threads`
- Use `target_compile_definitions` instead of `add_definitions`
- Affects lines: 7, 13, 53

### 4k. Fix `.gitignore`

**File:** `.gitignore`

Add `libs/*.a` alongside `libs/*.so`.

### 4l. Fix CI hardcoded repo name

**File:** `.github/workflows/build.yml`

Replace `"abelbour/mnn-llm-server"` with `${{ github.repository }}`.

- Affects line: 123

### 4m. Fix CI API error handling

**File:** `.github/workflows/build.yml`

Add `set -e` and validate `VERSION` is non-empty before proceeding.

- Affects line: 22

### 4n. Fix `grep -c "^"` on empty string

**File:** `scripts/start.sh`

Replace `echo "" | grep -c "^"` with a direct string length check.

- Affects line: 253

---

## Implementation Order

| Phase | Files Modified | Est. Lines Changed |
|-------|----------------|-------------------|
| 1 (P0 Security) | `index.html`, `main.cpp` | ~80 |
| 2 (P1 Critical) | `main.cpp`, `index.html`, `CMakeLists.txt`, `start.sh` | ~60 |
| 3 (P2 API/Perf) | `main.cpp`, `index.html`, `start.sh` | ~100 |
| 4 (P3 Polish) | `main.cpp`, `index.html`, `CMakeLists.txt`, `start.sh`, `build.yml`, `.gitignore` | ~80 |

**Total:** ~320 lines changed across 6 files.

## Commits

1. `fix: P0 XSS and critical bugs` — Phase 1
2. `fix: P1 crashes, validation, build` — Phase 2
3. `feat: API compatibility and performance` — Phase 3
4. `style: polish, accessibility, portability` — Phase 4
