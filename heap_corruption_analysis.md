# Heap Corruption Analysis - zzacwt.exe

## Date
Analysis performed during debugging session

## Exception Details
- **Exception Type**: EmbeddedBreakpoint
- **Exception Message**: A breakpoint instruction (__debugbreak() statement or a similar call) was executed in zzacwt.exe
- **Root Cause**: Windows heap corruption detection triggered during `RtlAllocateHeap` call

## Call Stack Summary
```
[1-11]  ntdll.dll heap corruption detection and reporting
[12]    msvcrt.dll!_malloc_dbg()
[13-31] TextInputFramework.dll (Windows Text Input processing)
[32]    zzacwt.exe!Fl_WinAPI_System_Driver::wait (Fl_win32.cxx:441)
		- Currently executing: DispatchMessageW(&fl_msg)
		- Message being processed: WM_TIMER (0x113 / 275)
[33]    zzacwt.exe!Fl::wait
[34]    zzacwt.exe!Fl::run
[35]    zzacwt.exe!main (main.cpp:168)
```

## Root Cause

### Heap Corruption via Thread-Safety Violation

The heap corruption is caused by **concurrent access to FLTK widgets from multiple threads**:

1. **Main Thread (12452)**: Processing Windows messages in the FLTK event loop
   - Executing `DispatchMessageW()` which triggered Windows' TextInputFramework.dll
   - TextInputFramework attempted heap allocation and detected corruption

2. **Audio Thread(s)**: PortAudio callback thread (thread 11436) and multiple worker threads
   - Thread 9656: `oscillator::generation_loop`
   - Thread 21952: `shaper::generation_loop`
   - Thread 14676: `zc_async_queue<double>::size`
   - Thread 18592: `mod_mixer::modulation_loop`
   - Thread 8064: `ProcessingThreadProc` (PortAudio)
   - Thread 5488: `monitor::processing_thread_function`
   - Thread 12952: `monitor::processing_thread_function`

### The Threading Problem

In `main.cpp`, audio callbacks are registered that directly access GUI widgets:

```cpp
// Lines 93-104 in main.cpp
static void audio_metadata_callback(const std::string& metadata)
{
	if (review_ && !metadata.empty()) {
		review_->add_sent_text(metadata, text_source_t::SENT_TEXT);  // ← UNSAFE: GUI operation from audio thread
	}
}

static void audio_sample_callback(double sample)
{
	if (review_) {
		review_->add_audio_sample(sample);  // ← May be safe if monitor_ queues internally
	}
}
```

### Direct FLTK Widget Manipulation from Non-Main Thread

**Critical Issue in `review::add_sent_text()` (review.hpp)**:

```cpp
void review::add_sent_text(const std::string& text, text_source_t source) {
	if (source == text_source_t::SENT_TEXT) {
		Fl_Text_Buffer* buffer = td_sent_->buffer();
		buffer->append(text.c_str());  // ← UNSAFE: Direct buffer modification
		Fl_Text_Buffer* highlight_buffer = td_sent_->style_buffer();
		char style_char = show_as_sending_ ? STYLE_NORMAL : STYLE_HIDDEN;
		char* style_str = new char[text.size() + 1];  // ← Heap allocation from wrong thread
		*std::fill_n(style_str, text.size(), style_char) = '\0';
		highlight_buffer->append(style_str);  // ← UNSAFE: Direct buffer modification
		delete[] style_str;  // ← Heap deallocation
		int last_line = td_sent_->line_start(buffer->length() - 1);
		td_sent_->scroll(last_line, 0);  // ← UNSAFE: Widget operation
		td_sent_->redraw();  // ← UNSAFE: Widget operation
		Fl::awake();  // ← Called AFTER already corrupting data
	}
}
```

**Problems**:
- `Fl_Text_Buffer::append()` is NOT thread-safe
- `td_sent_->scroll()` and `td_sent_->redraw()` are FLTK widget operations that MUST run on the main thread
- `Fl::awake()` is called AFTER the damage is done, not before
- Multiple heap allocations (`new`/`delete`) from non-main thread racing with main thread's heap operations

## FLTK Thread Safety Rules

From FLTK documentation:
> **FLTK is NOT thread-safe.** All GUI operations, including widget creation, modification, and destruction, must occur on the main thread.

The correct pattern is:
1. Audio thread: Queue data only (thread-safe queue/mutex)
2. Audio thread: Call `Fl::awake()` to signal main thread
3. Main thread: Process queued data and update widgets in event loop

## Current State Analysis

### Local Variables at Break Point
- `have_message` = 1 (message was available)
- `fl_msg.message` = 0x113 (WM_TIMER)
- `fl_msg.hwnd` = NULL (thread message, not window message)
- `Fl_X::first` = valid window list (review window)

### Active Threads
19 threads total, including:
- 1 main GUI thread (12452) - where corruption was detected
- 8+ worker threads for audio processing
- Multiple Windows system threads

### Observer Notes
User indicates: "The audio samples are already protected within review_ and monitor_"
- This suggests `add_audio_sample()` may already use proper queuing
- However, `add_sent_text()` clearly does NOT use proper queuing

## Recommendations

### Immediate Fix Required
The `audio_metadata_callback()` function must NOT directly call `review_->add_sent_text()`. Options:

1. **Option A**: Implement a thread-safe queue for metadata
   ```cpp
   // Thread-safe queue
   std::mutex metadata_mutex;
   std::queue<std::string> metadata_queue;

   // Audio thread: Just queue
   void audio_metadata_callback(const std::string& metadata) {
	   std::lock_guard<std::mutex> lock(metadata_mutex);
	   metadata_queue.push(metadata);
	   Fl::awake();
   }

   // Main thread: Process queue in FLTK event loop
   void process_metadata_queue() {
	   std::queue<std::string> local_queue;
	   {
		   std::lock_guard<std::mutex> lock(metadata_mutex);
		   local_queue.swap(metadata_queue);
	   }
	   while (!local_queue.empty()) {
		   if (review_) {
			   review_->add_sent_text(local_queue.front(), text_source_t::SENT_TEXT);
		   }
		   local_queue.pop();
	   }
   }
   ```

2. **Option B**: Use Fl::awake(callback, data) properly
   ```cpp
   void audio_metadata_callback(const std::string& metadata) {
	   std::string* data = new std::string(metadata);
	   Fl::awake([](void* p) {
		   std::string* str = static_cast<std::string*>(p);
		   if (review_) {
			   review_->add_sent_text(*str, text_source_t::SENT_TEXT);
		   }
		   delete str;
	   }, data);
   }
   ```

### Verification Steps
1. Add thread ID logging to `add_sent_text()` to confirm it's being called from non-main thread
2. Enable heap debugging flags (pageheap) to catch corruption earlier
3. Review all callback paths from PortAudio to ensure no other direct GUI access

## Related Code Files
- `src/main.cpp` - Contains unsafe callbacks (lines 93-104)
- `include/review.hpp` / `src/review.cpp` - Contains `add_sent_text()` with direct FLTK operations
- `C:\Users\pvros\source\repos\fltk\src\Fl_win32.cxx` - FLTK event loop (line 441)
- `zc_speaker.h` - PortAudio interface with callback registration

## Conclusion
This is a classic **race condition between main GUI thread and audio processing threads**, resulting in heap corruption when both threads simultaneously access FLTK's internal data structures. The corruption is detected during a subsequent heap allocation in Windows' TextInputFramework rather than at the point of corruption, making it harder to diagnose.

**Priority**: HIGH - This is a critical bug that causes application crashes
**Difficulty**: MEDIUM - Well-understood problem with standard solutions
