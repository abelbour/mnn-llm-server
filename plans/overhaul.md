# MNN LLM Server — Overhaul Plan

## Background

MNN's `Llm::response()` writes generated tokens to an `std::ostream` as they are
produced.  The library provides `LlmStreamBuffer` (a custom `std::streambuf`)
that accepts a `(const char*, size_t)` callback, so every chunk the model emits
can be forwarded **immediately** to an SSE response — no artificial buffering
or sleeping needed.

The current code ignores this: it passes a plain `std::stringstream`, waits for
the full response, then chops it into 10-char pieces with `sleep(10ms)`.
All other issues stem from this same outdated approach.

---

## Phase 1 — Investigation (✓ Implemented)

- Located MNN `llm.hpp` API signatures
- Found `LlmStreamBuffer` + callback pattern in MNN's own `mls_server.cpp`
- Confirmed `generate_init()` + `generate(1)` step-by-step control exists
- Conclusion: **true token-by-token streaming is possible and well-supported**

---

## Phase 2 — Server Rewrite (✓ Implemented)

### 2a — Configurable `max_tokens`

- Parse `max_tokens` from `POST /v1/chat/completions` body (default `256`)
- Overload `chat(prompt, maxTokens)` on `MnnServer` to accept the value
- Pass it as the 4th arg to `llm_->response()`

### 2b — Multi-turn Message Parsing

- Replace `parseMessages()` to iterate **all** messages in order
- Format: `System: ...` then alternating `User: ...` / `Assistant: ...`
- End with `Assistant:` (the model should continue from here)

### 2c — True Token-by-Token Streaming

- Add `MnnServer::chatStreaming(prompt, maxTokens, sink)` method
- Inside it:
  1. Create a `LlmStreamBuffer` that writes SSE chunks via a callback
  2. Wrap it in an `std::ostream`
  3. Call `llm_->response(prompt, &ostream, "<eop>", maxTokens)`
  4. When `<eop>` or `stopRequested_`, signal `[DONE]`
- The callback fires in real-time as the model decodes tokens

### 2d — Non-Streaming Path

- Keep `MnnServer::chat(prompt, maxTokens)` for non-streaming
- Use a plain `std::stringstream`, same as today, but with `max_tokens`

### 2e — Remove Artificial Streaming Code

- Delete the 10-char chunk loop and `sleep(10ms)`
- The streaming path now uses native token timing

### 2f — Background Inference Thread

- **Cancelled** — cpp-httplib's `set_chunked_content_provider()` already runs on a
  dedicated thread, so `/health` stays responsive even during inference. The mutex
  serializes concurrent chat requests (expected for single-model server).

**Files changed:** `src/main.cpp`

---

## Phase 3 — Web UI Auto-Detect (✓ Implemented)

### 3a — Local Server Detection

On page load, before showing the settings modal:

```
GET /v1/models  (relative to window.location.origin)
```

- If **200** with valid model list:
  - Set `apiUrl = window.location.origin + '/v1/chat/completions'`
  - Pick first model as default
  - Set `apiKey = 'dummy'`
  - Skip the "configure API key" modal
- If **fails** (not a local server):
  - Show settings modal as before (defaulting to OpenAI)
  - User must provide their own API key

### 3b — Skip API Key Check for Local Mode

- Add `isLocal` flag derived from auto-detection
- Skip the `if (!apiKey) showNotification(...)` guard when `isLocal === true`
- Still send `dummy` key header for compatibility

### 3c — Keep Settings Modal

- Accessible via ⚙ button for power users
- Pre-filled with detected local values or previous OpenAI settings

**Files changed:** `web/index.html`

---

## Phase 4 — Build System (✓ Implemented)

### 4a — Flexible MNN Path

Replace hardcoded `/root/MNN` with env-var fallback chain:

```cmake
if(DEFINED ENV{MNN_ROOT})
  set(MNN_ROOT "$ENV{MNN_ROOT}" CACHE PATH "MNN root directory")
else()
  set(MNN_ROOT "/root/MNN" CACHE PATH "MNN root directory")
endif()
```

### 4b — Find-Package Style Discovery

Add `find_path`/`find_library` for the MNN include dirs and libs:

```cmake
if(NOT EXISTS "${MNN_INCLUDE_DIR}/MNN/MNNForwardType.h")
  find_path(MNN_INCLUDE_DIR MNN/MNNForwardType.h)
endif()
if(NOT EXISTS "${MNN_LLM_INCLUDE_DIR}/llm/llm.hpp")
  find_path(MNN_LLM_INCLUDE_DIR llm/llm.hpp)
endif()
```

**Files changed:** `src/CMakeLists.txt`
- Overload `chat(prompt, maxTokens)` on `MnnServer` to accept the value
- Pass it as the 4th arg to `llm_->response()`

### 2b — Multi-turn Message Parsing

- Replace `parseMessages()` to iterate **all** messages in order
- Format: `System: ...` then alternating `User: ...` / `Assistant: ...`
- End with `Assistant:` (the model should continue from here)
- Accept `ChatMessages` (array of `{role, content}`) directly

### 2c — True Token-by-Token Streaming

- Add `MnnServer::chatStreaming(prompt, maxTokens, sink)` method
- Inside it:
  1. Create a `LlmStreamBuffer` that writes SSE chunks via a callback
  2. Wrap it in an `std::ostream`
  3. Call `llm_->response(prompt, &ostream, "<eop>", maxTokens)`
  4. Wait for callback to signal `<eop>` or `stoped()`
  5. Signal `[DONE]`
- The callback fires in real-time as the model decodes tokens

### 2d — Non-Streaming Path

- Keep `MnnServer::chat(prompt, maxTokens)` for non-streaming
- Use a plain `std::stringstream`, same as today, but with `<eop>` and `max_tokens`
- Return the complete string after `response()` returns

### 2e — Remove Artificial Streaming Code

- Delete the 10-char chunk loop and `sleep(10ms)` (lines 335-350)
- The streaming path now uses native token timing

### 2f — Background Inference Thread

- Add a thread-safe request queue (`std::queue` + `mutex` + `condition_variable`)
- A single background thread pulls requests and runs inference
- HTTP handler submits prompt, waits on a per-request `promise`/`future` for tokens
- `/health` and `/v1/models` remain lock-free and always responsive
- Clean `AbortController` support: set a flag, `generate()` checks `stoped()` each iteration

**Files changed:** `src/main.cpp`

---

## Phase 3 — Web UI Auto-Detect (`web/index.html`)

### 3a — Local Server Detection

On page load, before showing the settings modal:

```
GET /v1/models  (relative to window.location.origin)
```

- If **200** with valid model list:
  - Set `apiUrl = window.location.origin + '/v1/chat/completions'`
  - Pick first model as default
  - Set `apiKey = 'dummy'`
  - Skip the "configure API key" modal
- If **fails** (not a local server):
  - Show settings modal as before (defaulting to OpenAI)
  - User must provide their own API key

### 3b — Skip API Key Check for Local Mode

- Add `isLocal` flag derived from auto-detection
- Skip the `if (!apiKey) showNotification(...)` guard when `isLocal === true`
- Still send `dummy` key header for compatibility

### 3c — Keep Settings Modal

- Accessible via ⚙ button for power users
- Pre-filled with detected local values or previous OpenAI settings

**Files changed:** `web/index.html`

---

## Phase 4 — Build System (`src/CMakeLists.txt`)

### 4a — Flexible MNN Path

Replace hardcoded `/root/MNN` with env-var fallback chain:

```cmake
set(MNN_ROOT "$ENV{MNN_ROOT}" CACHE PATH "MNN root directory")
if(NOT MNN_ROOT)
  set(MNN_ROOT "/root/MNN" CACHE PATH "MNN root directory" FORCE)
endif()
```

### 4b — Find-Package Style Discovery

Add `find_path`/`find_library` for the MNN include dirs and libs:

```cmake
find_path(MNN_INCLUDE_DIR MNN/MNNForwardType.h HINTS ${MNN_ROOT}/include)
find_library(MNN_LIBRARY MNN HINTS ${MNN_ROOT}/build)
find_library(LLM_LIBRARY llm HINTS ${MNN_ROOT}/build)
```

**Files changed:** `src/CMakeLists.txt`

---

## Migration Strategy

1. **Phase 2** changes are the core: rewrite streaming, add multi-turn, background thread
2. **Phase 3** is independent — can be done in parallel or after Phase 2
3. **Phase 4** is a small isolated change

Each phase is a single atomic commit.

---

## Post-Review Fixes (✓ Implemented)

| # | Issue | Fix |
|---|-------|-----|
| 1 | Misplaced `#include <unistd.h>` and `<libgen.h>` | Moved to top with other includes |
| 2 | `stopRequested_` is a plain `bool` — data race | Changed to `std::atomic<bool>` |
| 3 | HTML input default shows OpenAI URL | Changed to empty + local placeholder |
| 4 | `saveSettings()` requires API key in local mode | Skips validation when `isLocal` |

---

## Verification

- `curl -N -X POST ... stream=true` — tokens appear one-by-one with no 10ms delay
- `curl -X POST ... stream=false` — returns same content in one shot
- Multi-turn: send 3 user messages, get a coherent assistant reply that references all
- Two concurrent curl streaming requests — second blocks at queue level but first streams
- `/health` responds instantly even while a long inference is running
- Web UI auto-detects local server, no API key prompt
- Web UI still works with OpenAI when served from elsewhere