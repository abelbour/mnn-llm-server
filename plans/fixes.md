# Post-Review Fix Plan

Issues found during code review, ordered by severity.

## 1. `LlmStreamBuffer` namespace qualification (Medium)

**File:** `src/main.cpp:125`
**Problem:** Used without namespace prefix. MNN's own examples do this, but since
`MNN::Transformer::Llm` is used with prefix everywhere else, compilation may fail
if `LlmStreamBuffer` is in a namespace.
**Fix:** Qualify as `MNN::Transformer::LlmStreamBuffer`. If that doesn't match MNN's
actual namespace, the compile error will tell us the correct one.

## 2. `stopRequested_` should be `std::atomic<bool>` (Low)

**File:** `src/main.cpp:27,112,126`
**Problem:** Written from the HTTP handler thread (via `sink.on_cancel`) and read
from the chunked content provider thread. A plain `bool` is technically a data race.
**Fix:** Change to `std::atomic<bool>`. Add `#include <atomic>`.

## 3. Misplaced includes (Low)

**File:** `src/main.cpp:262-263`
**Problem:** `#include <unistd.h>` and `#include <libgen.h>` are placed after
function definitions instead of at the top with the other includes.
**Fix:** Move them to the include section at lines 1-16.

## 4. Save Settings requires API key in local mode (Low)

**File:** `web/index.html:410-413`
**Problem:** `saveSettings()` rejects empty API key even when `isLocal` is true.
**Fix:** Skip API key validation when `isLocal` is true.

## 5. HTML input default shows OpenAI URL (Cosmetic)

**File:** `web/index.html:170`
**Problem:** The `value` attribute defaults to `https://api.openai.com/v1/...` even
though JS auto-detection will override it. Confusing if JS fails to load.
**Fix:** Change placeholder to empty string.

## 6. Plan file update

Update `plans/overhaul.md` to reflect implementation status.