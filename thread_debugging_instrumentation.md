# Thread Debugging Instrumentation Applied

## Problem

Application still failing on shutdown with thread exit code 1 and heap corruption, even after:
1. Fixed initialization race condition in monitor thread
2. Added null checks in update_display_buffer()
3. Removed empty queue throw statements from mod_mixer

Need to identify **which thread** is crashing and **what exception** is being thrown.

## Solution: Comprehensive Thread Instrumentation

Added thread ID logging and exception handling to **ALL worker threads** to:
1. Identify which thread crashes (by matching thread ID to exit code 1)
2. Capture and log the exact exception message
3. Prevent thread exit code 1 by catching exceptions

## Changes Applied

### Files Modified

1. **src/oscillator.cpp** - `generation_loop()`
2. **src/shaper.cpp** - `generation_loop()`
3. **src/monitor.cpp** - `processing_thread_function()`
4. **src/noise_gen.cpp** - `generation_loop()`
5. **src/mod_mixer.cpp** - `modulation_loop()`

### Pattern Applied to All Threads

```cpp
void worker_thread_function(worker* self) {
	fprintf(stderr, "[THREAD] <ThreadName> thread started, ID: %u\n", 
			std::hash<std::thread::id>{}(std::this_thread::get_id()));
	try {
		while (!self->stop_flag_) {
			// ... existing work loop ...
		}
		fprintf(stderr, "[THREAD] <ThreadName> thread exiting normally, ID: %u\n",
				std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[THREAD] <ThreadName> thread exception, ID: %u, error: %s\n",
				std::hash<std::thread::id>{}(std::this_thread::get_id()), e.what());
	}
	catch (...) {
		fprintf(stderr, "[THREAD] <ThreadName> thread unknown exception, ID: %u\n",
				std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
}
```

### Thread Names in Log Output

- `[THREAD] Oscillator thread started/exiting/exception, ID: <hash>`
- `[THREAD] Shaper thread started/exiting/exception, ID: <hash>`
- `[THREAD] Monitor thread started/exiting/exception, ID: <hash>`
- `[THREAD] Noise_gen thread started/exiting/exception, ID: <hash>`
- `[THREAD] Mod_mixer thread started/exiting/exception, ID: <hash>`

## Expected Debug Output

### Normal Run (all good):
```
[THREAD] Oscillator thread started, ID: 12345
[THREAD] Shaper thread started, ID: 23456
[THREAD] Monitor thread started, ID: 34567
[THREAD] Noise_gen thread started, ID: 45678
[THREAD] Mod_mixer thread started, ID: 56789
... application runs ...
[THREAD] Oscillator thread exiting normally, ID: 12345
[THREAD] Shaper thread exiting normally, ID: 23456
[THREAD] Monitor thread exiting normally, ID: 34567
[THREAD] Noise_gen thread exiting normally, ID: 45678
[THREAD] Mod_mixer thread exiting normally, ID: 56789
```

### With Exception (problematic run):
```
[THREAD] Oscillator thread started, ID: 12345
[THREAD] Shaper thread started, ID: 23456
[THREAD] Monitor thread started, ID: 34567
[THREAD] Noise_gen thread started, ID: 45678
[THREAD] Mod_mixer thread started, ID: 56789
... application runs ...
[THREAD] Shaper thread exception, ID: 23456, error: vector::_M_range_check: __n >= this->size()
The thread 6304 has exited with code 0 (0x0).  ← NOW code 0 instead of 1!
... other threads exit normally ...
```

**Key improvement:** The exception is now **caught and logged**, so:
1. We see **which thread** threw the exception (by name and ID)
2. We see the **exact error message**
3. Thread exits with **code 0** instead of code 1 (clean exit)
4. Heap corruption *might* still occur if the exception left data in invalid state, but at least we'll know the source

## Next Steps

### After Next Run:

1. **Check stderr output** for `[THREAD]` messages
2. **Match thread ID** from exception log to Windows debug output `The thread XXXX has exited with code 1`
3. **Read exception message** to understand what failed
4. **Fix the root cause** of that specific exception

### Possible Sources Still to Investigate:

If exceptions continue after this instrumentation:

- **Text gen thread** (if it exists and wasn't covered)
- **PortAudio callback threads** (zc_audio callbacks)
- **FLTK event loop issues** (though main thread should be stable)
- **Race conditions during queue shutdown** (wait_and_pop() returning false vs. accessing invalid data)
- **Double-free or use-after-delete** during cleanup sequence

### If Still No Exception Logged:

If thread still exits with code 1 but NO exception is logged, then the crash is happening **outside** the try-catch scope:
- During thread initialization (before try block)
- In static destructors or thread cleanup
- In system libraries (PortAudio, FFTW, FLTK)
- Stack overflow or other fatal signal

## Files to Check

When running the application, monitor:
- **Console/stderr output** - look for `[THREAD]` messages
- **Debug Output window in Visual Studio** - look for thread exit codes
- Match thread hash IDs to exit codes to identify culprit

## Summary

✅ **Oscillator** thread now has ID logging and exception handling  
✅ **Shaper** thread now has ID logging and exception handling  
✅ **Monitor** thread now has ID logging and exception handling  
✅ **Noise_gen** thread now has ID logging and exception handling  
✅ **Mod_mixer** thread now has ID logging and exception handling  

All worker threads will now:
- Log their start with unique ID
- Catch and log any exceptions with thread ID and error message
- Exit cleanly with code 0 even if exception occurs
- Log normal exit with thread ID

This comprehensive instrumentation should **immediately identify** which thread is crashing and why when you run the application next.
