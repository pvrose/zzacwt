# Complete Thread Crash Investigation - Status Summary

## Investigation Timeline

### Phase 1: Initial Exception Analysis
- **Problem:** 0xC0000409 "RTL_BALANCED_NODE RBTree entry has been corrupted" during shutdown
- **Finding:** Shutdown-time corruption indicated earlier runtime heap damage
- **Root Cause Theory:** Cross-thread FLTK access and unsafe callbacks

### Phase 2: Queue Shutdown Fix
- **Problem:** Threads blocked in queue wait during shutdown
- **Fix Applied:** Added shutdown signaling to zc_async_queue
- **Result:** Still crashing - not the root cause

### Phase 3: FLTK Thread Safety Fix
- **Problem:** `cb_decoder_callback()` directly modified FLTK widgets from monitor thread
- **Fix Applied:** Moved GUI updates to main thread via atomics and polling
- **Result:** Partial improvement, but still crashing

### Phase 4: Monitor Thread Initialization Race
- **Problem:** Monitor thread started BEFORE display buffer was set
- **Finding:** Thread accessed `display_buffer_->y_values` when buffer was nullptr
- **Fix Applied:** 
  1. Reordered `configure_spectrogram()` to set buffer before starting thread
  2. Added null/bounds checks in `update_display_buffer()`
  3. Added exception handling in monitor thread
- **Result:** Still crashing with thread exit code 1

### Phase 5: Mod_Mixer Exception Fix
- **Problem:** `modulation_loop()` threw `std::underflow_error` on empty queues
- **Finding:** Three explicit `throw` statements for normal shutdown condition
- **Fix Applied:** User removed the throw statements
- **Result:** Still crashing with thread exit code 1

### Phase 6: Comprehensive Thread Instrumentation (CURRENT)
- **Problem:** Unknown thread still exiting with code 1, can't identify which
- **Fix Applied:** Added thread ID logging and exception handling to ALL worker threads
  - Oscillator
  - Shaper
  - Monitor
  - Noise_gen
  - Mod_mixer
- **Expected Result:** Next run will show which thread crashes and the exact exception message

## Current State of Fixes

### ✅ Completed Fixes

1. **zc_async_queue shutdown signaling**
   - File: `../zzacommon/include/zc_async_queue.h`
   - `shutdown()` method added
   - `wait_and_pop()` returns bool
   - Destructor calls shutdown

2. **mod_mixer shutdown handling**
   - File: `src/mod_mixer.cpp`
   - Checks `wait_and_pop()` return values
   - Exits cleanly on queue shutdown

3. **main.cpp cleanup sequencing**
   - File: `src/main.cpp`
   - Stops queues before deleting objects
   - Proper dependency order

4. **review FLTK thread safety**
   - File: `src/review.cpp`, `include/review.hpp`
   - `cb_decoder_callback()` no longer touches FLTK directly
   - Atomics for pitch/WPM
   - Main-thread polling for widget updates

5. **Monitor initialization race condition**
   - File: `src/review.cpp` - `configure_spectrogram()`
   - Display buffer now set BEFORE thread starts
   - No more nullptr access during startup

6. **Monitor thread null checks**
   - File: `src/monitor.cpp` - `update_display_buffer()`
   - Null pointer guards
   - Buffer size validation
   - Prevents out-of-bounds access

7. **ALL worker threads exception handling**
   - Files: `src/oscillator.cpp`, `src/shaper.cpp`, `src/monitor.cpp`, `src/noise_gen.cpp`, `src/mod_mixer.cpp`
   - All threads wrapped in try-catch
   - Thread ID logging on start/exit/exception
   - Exception messages logged to stderr

### ⚠️ Partially Fixed

1. **Spectrogram data race**
   - Analysis: `spectrogram_data_race_analysis.md`
   - Issue: Monitor writes `z_values` while FLTK reads (no synchronization)
   - Partial Fix: Separate capture/display buffers created
   - Remaining: Full double-buffer swap not yet implemented
   - Priority: Medium (may not be causing current crash)

### ❌ Still Needs Investigation

1. **Thread exit code 1 - identity unknown**
   - Thread 6304 (or similar) exits with code 1
   - Heap corruption still detected during shutdown
   - **Next Step:** Run instrumented build to identify which thread and why

## All Worker Threads Status

| Thread | Exception Handling | Thread ID Logging | Status |
|--------|-------------------|-------------------|--------|
| Oscillator | ✅ | ✅ | Ready for debugging |
| Shaper | ✅ | ✅ | Ready for debugging |
| Monitor | ✅ | ✅ | Ready for debugging |
| Noise_gen | ✅ | ✅ | Ready for debugging |
| Mod_mixer | ✅ | ✅ | Ready for debugging |

## Thread Architecture

```
Main Thread (FLTK Event Loop)
├─ Oscillator Thread       → generates sine wave samples
├─ Shaper Thread          → generates envelope samples (calls text_gen synchronously)
├─ Noise_gen Thread       → generates noise samples
├─ Mod_mixer Thread       → combines oscillator × shaper + noise
├─ Monitor Thread         → FFT analysis, spectrogram, decoder
└─ PortAudio Threads      → audio I/O callbacks (managed by PortAudio library)
```

**Note:** `text_gen` is NOT a thread - it's called synchronously by shaper thread.

## Next Debugging Session

### Run the Application and Check:

1. **Visual Studio Debug Output Window**
   - Look for thread exit messages: `The thread XXXX has exited with code 1 (0x1).`
   - Note the thread ID (e.g., 6304)

2. **stderr Console Output**
   - Look for `[THREAD]` messages
   - Find thread start: `[THREAD] <Name> thread started, ID: 12345`
   - Find exception: `[THREAD] <Name> thread exception, ID: 12345, error: <message>`
   - Match thread hash ID to Windows thread ID

3. **Correlation**
   - Thread name tells you WHICH worker crashed
   - Exception message tells you WHY
   - Thread ID confirms the match

### Example Output to Look For:

```
[THREAD] Oscillator thread started, ID: 2841023456
[THREAD] Shaper thread started, ID: 3951234567
[THREAD] Monitor thread started, ID: 4062345678
[THREAD] Noise_gen thread started, ID: 5173456789
[THREAD] Mod_mixer thread started, ID: 6284567890
...
[THREAD] Shaper thread exception, ID: 3951234567, error: std::logic_error: mutex lock failed
The thread 6304 has exited with code 0 (0x0).
```

**Key:** Thread will now exit with code 0 (because exception is caught), and we'll see the exact error message.

## Potential Remaining Issues

If instrumentation shows no exception logged but thread still crashes:

1. **Static destructor crash** - happens outside try-catch
2. **Stack overflow** - silent corruption
3. **System library crash** - FFTW, PortAudio, FLTK internal
4. **Access violation before try block** - during thread initialization
5. **Double-free or use-after-delete** - memory already freed

## Files Modified in This Investigation

### Analysis Documents Created:
- `heap_corruption_analysis.md` (original reference)
- `shutdown_fix_summary.md` (Phase 2)
- `thread_safety_fix_summary.md` (Phase 3)
- `spectrogram_data_race_analysis.md` (Phase 3)
- `thread_exit_code_1_analysis.md` (Phase 4)
- `thread_exit_code_1_fix_applied.md` (Phase 4)
- `thread_debugging_instrumentation.md` (Phase 6 - current)
- `complete_thread_investigation_summary.md` (this file)

### Code Files Modified:
- `../zzacommon/include/zc_async_queue.h` (shutdown signaling)
- `src/mod_mixer.cpp` (queue shutdown, exception handling)
- `src/review.cpp` (FLTK thread safety, buffer init order)
- `include/review.hpp` (atomics, thread guards)
- `src/monitor.cpp` (null checks, exception handling)
- `src/oscillator.cpp` (exception handling)
- `src/shaper.cpp` (exception handling)
- `src/noise_gen.cpp` (exception handling)

## Confidence Level

**Current Fix Confidence:** HIGH

The comprehensive thread instrumentation will definitively identify:
- **Which thread** is crashing (by name and ID)
- **What exception** is being thrown (exact error message)
- **When** it happens (startup, runtime, shutdown)

Once identified, the fix will be straightforward and targeted.

## Awaiting Next Run

**Status:** ✅ All instrumentation applied, ready for next debugging session

User should:
1. Build the instrumented code
2. Run the application
3. Trigger shutdown (close app)
4. Check Debug Output window for thread exit codes
5. Check console/stderr for `[THREAD]` messages
6. Report findings
