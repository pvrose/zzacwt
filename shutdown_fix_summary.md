# Shutdown-Time Heap Corruption Fix

## Issue Summary
The application was experiencing an **RTL_BALANCED_NODE RBTree corruption** (exception 0xC0000409) during shutdown in `windows.storage.dll` ETW (Event Tracing for Windows) unregistration.

## Root Cause
The crash during shutdown was caused by **improper thread shutdown ordering** that led to:

1. **Worker threads still running during shutdown** - when `ExitProcess()` started unloading DLLs
2. **Threads blocked in `wait_and_pop()`** when queues were destroyed - causing undefined behavior with condition variables and mutexes
3. **Heap corruption** from earlier thread-safety violations propagating to ETW's internal RBTree data structures
4. **No explicit queue shutdown mechanism** - threads would remain blocked waiting for data that would never come

### The Sequence of Failure
1. Application calls `exit()` → `ExitProcess()` → `LdrShutdownProcess()`
2. Main deletes objects, but threads are still running or blocked
3. Queues destroyed while threads blocked in `wait_and_pop()` 
4. Condition variables/mutexes destroyed while in use → undefined behavior
5. Heap corruption accumulates during shutdown
6. Windows unloads `windows.storage.dll` 
7. DLL cleanup calls `EtwUnregisterTraceGuids()`
8. ETW tries to remove entry from RBTree with corrupted nodes
9. **CRASH**: RBTree validation fails

## The Fix

### 1. Added Queue Shutdown Mechanism (`zc_async_queue.h`)
- Added `shutdown_` flag to track when queue is being destroyed
- Added `shutdown()` method that sets flag and wakes all waiting threads
- Modified `wait_and_pop()` to return `bool` - returns `false` when shutting down
- Added destructor that calls `shutdown()` before destruction

### 2. Updated Consumers to Handle Shutdown (`mod_mixer.cpp`)
- Check return value of `wait_and_pop()` in all three queue accesses
- Break out of processing loop when queue returns `false` (shutting down)
- Allows `mod_mixer` thread to exit cleanly during shutdown

### 3. Fixed Shutdown Order in `main.cpp`
Implemented proper cleanup sequence:

```
Step 1: Stop audio output (consumer of final queue)
Step 2: Shutdown ALL queues (wake up blocked threads)
Step 3: Delete mod_mixer (consumer thread can now exit cleanly)
Step 4: Delete producers (oscillator, shaper, noise_gen)
Step 5: Delete monitor (audio processor)
Step 6: Delete GUI components
Step 7: Delete queues (now safe - no threads using them)
```

### Key Improvements
- **Explicit shutdown signaling** - threads know when to stop waiting
- **No dangling threads** - all threads join before their queues are destroyed
- **Reverse dependency order** - delete consumers before producers
- **Clean condition variable cleanup** - no threads blocked during destruction

## Files Modified

1. **../zzacommon/include/zc_async_queue.h**
   - Added shutdown mechanism to prevent undefined behavior during destruction

2. **src/mod_mixer.cpp**
   - Handle queue shutdown signals gracefully

3. **src/main.cpp**
   - Reordered cleanup sequence
   - Added explicit queue shutdown calls
   - Added queue deletion

## Prevention of Future Issues

This fix prevents:
- ✅ Threads blocked during queue destruction
- ✅ Undefined behavior from destroying mutexes/condition variables in use
- ✅ Heap corruption from accessing deleted objects
- ✅ Race conditions during shutdown
- ✅ ETW RBTree corruption from accumulated heap damage

## Testing Recommendations

1. **Verify clean shutdown** - no crashes on application exit
2. **Check for thread leaks** - all threads should exit cleanly
3. **Test with Address Sanitizer** - verify no use-after-free
4. **Test with Thread Sanitizer** - verify no data races during shutdown
5. **Stress test** - rapid start/stop cycles

## Related Documentation
See `heap_corruption_analysis.md` for details on the original thread-safety issues that contributed to this problem.
