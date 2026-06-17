# Critical Thread-Safety Bug Fix - FLTK Widget Access from Monitor Thread

## Root Cause Analysis

The application crash with `0xC0000409` ("RTL_BALANCED_NODE RBTree entry has been corrupted") during shutdown was caused by **heap corruption occurring at runtime**, not during shutdown itself. The shutdown-time crash was merely a **symptom** of earlier memory corruption.

### The Critical Bug

In `review::cb_decoder_callback()`, which is called from **monitor's processing thread** (NOT the main thread), the code was directly calling `Fl_Output::value()` to update FLTK widgets:

```cpp
// UNSAFE - Called from monitor thread!
void review::cb_decoder_callback(void* data, const std::string& text) {
	review* r = static_cast<review*>(data);
	r->decoded_text_queue_.push(text);  // ✅ Safe
	double freq = monitor_->get_selected_bin_pitch();
	char temp[10];
	snprintf(temp, sizeof(temp), "%.0f", freq);
	r->op_decoded_pitch_->value(temp);  // ❌ UNSAFE - FLTK widget access from worker thread!
	double wpm = monitor_->get_wpm();
	std::snprintf(temp, sizeof(temp), "%.1f", wpm);
	r->op_decoded_wpm_->value(temp);  // ❌ UNSAFE - FLTK widget access from worker thread!
}
```

**FLTK is NOT thread-safe.** All GUI widget operations must occur on the main thread. Calling `Fl_Output::value()` from the monitor's processing thread corrupts FLTK's internal data structures, leading to heap corruption that manifests later during shutdown when Windows attempts to clean up ETW registration data structures.

### Why This Caused the Crash

1. **Monitor thread** calls `cb_decoder_callback()` from its processing loop
2. Callback directly modifies `Fl_Output` widgets (`op_decoded_pitch_`, `op_decoded_wpm_`)
3. FLTK's internal text buffer management corrupts heap memory
4. Corruption goes undetected until shutdown
5. During process exit, Windows ETW unregistration tries to remove RBTree nodes
6. Corrupted heap causes `RtlRbRemoveNode()` to fail with 0xC0000409
7. Application crashes with "RTL_BALANCED_NODE RBTree entry has been corrupted"

This aligns perfectly with the earlier `heap_corruption_analysis.md` document, which identified unsafe FLTK access from non-main threads as the root cause.

## The Fix

### Changes Made

#### 1. Added atomic storage in `review` class (`include/review.hpp`)
```cpp
//! Latest decoded pitch and WPM (set from monitor thread, read from main thread)
std::atomic<double> latest_decoded_pitch_{0.0};
std::atomic<double> latest_decoded_wpm_{0.0};
```

#### 2. Fixed `cb_decoder_callback()` to be thread-safe (`src/review.cpp`)
```cpp
// SAFE - Only queues data and updates atomics
void review::cb_decoder_callback(void* data, const std::string& text) {
	review* r = static_cast<review*>(data);
	r->decoded_text_queue_.push(text);
	// Store freq/WPM in atomics - will be read and displayed on main thread
	double freq = monitor_->get_selected_bin_pitch();
	r->latest_decoded_pitch_.store(freq, std::memory_order_relaxed);
	double wpm = monitor_->get_wpm();
	r->latest_decoded_wpm_.store(wpm, std::memory_order_relaxed);
}
```

#### 3. Updated `poll_decoded_text()` to update widgets on main thread (`src/review.cpp`)
```cpp
void review::poll_decoded_text() {
	std::string text;
	while (decoded_text_queue_.try_pop(text)) {
		add_sent_text(text, text_source_t::DECODED_TEXT);
	}
	// Safely update FLTK widgets with latest values from monitor thread
	double freq = latest_decoded_pitch_.load(std::memory_order_relaxed);
	double wpm = latest_decoded_wpm_.load(std::memory_order_relaxed);
	if (freq > 0.0 || wpm > 0.0) {
		char temp[10];
		snprintf(temp, sizeof(temp), "%.0f", freq);
		op_decoded_pitch_->value(temp);
		snprintf(temp, sizeof(temp), "%.1f", wpm);
		op_decoded_wpm_->value(temp);
	}
}
```

`poll_decoded_text()` is called from `cb_ticker()`, which runs on the **main thread** via FLTK's timer mechanism, so all FLTK widget operations are now safe.

## Key Principles

1. **Never call FLTK functions from worker threads** - all GUI operations must be on the main thread
2. **Use queues or atomics** to transfer data from worker threads to main thread
3. **Use `Fl::awake()` or timer callbacks** to process queued data on the main thread
4. **Shutdown-time crashes often indicate runtime corruption** - look for the original bug earlier in execution

## Files Modified

- `include/review.hpp` - Added atomic storage for decoded pitch/WPM
- `src/review.cpp` - Fixed `cb_decoder_callback()` and `poll_decoded_text()`

## Testing

After this fix:
- Monitor thread no longer touches FLTK widgets
- All widget updates occur on main thread via ticker callback
- No heap corruption should occur
- Application should close cleanly without crashes

## Related Work

- Complements the shutdown-order fixes in `shutdown_fix_summary.md`
- Addresses the root cause identified in `heap_corruption_analysis.md`
- Follows FLTK thread-safety best practices
