# Critical Data Race Bug - Spectrogram Buffer Access

## **ROOT CAUSE: Data Race on Spectrogram Buffer**

The heap corruption is caused by a **data race** where the monitor processing thread **writes** to the spectrogram buffer while the FLTK rendering thread **reads** from it, with **NO synchronization**.

### **The Bug**

In `review::configure_spectrogram()`:

```cpp
// Creates ONE buffer
spectrogram_data_display_ = new zc_graph_::data_set_dens_t;
// ... fills it with data ...

// Gives SAME buffer to FLTK (reads during drawing)
spectrogram_->add_data_set(2, spectrogram_data_display_, map);

// Gives SAME buffer to monitor thread (writes continuously)
monitor_->set_display_buffer(spectrogram_data_display_, cb_update_spectrogram, this);
//                           ^^^^^^^^^^^^^^^^^^^^^^^^ ❌ SAME POINTER!
```

Then in `monitor::update_display_buffer()` (runs on **monitor thread**):

```cpp
// Directly writes to z_values while FLTK may be reading it!
for (int r = 0; r < num_bins; r++) {
	for (int c = 0; c < display_depth_ - 1; c++) {
		display_buffer_->z_values[r * display_depth_ + c] = 
			display_buffer_->z_values[r * display_depth_ + c + 1];  // ❌ DATA RACE!
	}
	display_buffer_->z_values[r * display_depth_ + display_depth_ - 1] = image[r];
}
```

**Result:** Undefined behavior, heap corruption, random crashes.

### **Why This Is Catastrophic**

1. **Monitor thread** continuously updates FFT data and writes to `z_values`
2. **Main/FLTK thread** reads `z_values` during widget drawing (`spectrogram_->draw()`)
3. **NO mutex** protects these accesses
4. C++ standard: data race on non-atomic = **undefined behavior**
5. Manifests as heap corruption detected by Windows debug CRT

### **The Unused Member**

The class has `spectrogram_data_capture_` declared but **never used**! This suggests double-buffering was **intended** but **never implemented**.

## **The Fix: Proper Double-Buffering**

### **Architecture**

```
Monitor Thread:          Main Thread:

write to                read from
capture buffer -------> display buffer
				^
				|
		   swap on ticker (main thread only)
```

### **Implementation Changes**

#### 1. **Add synchronization members** (in `review.hpp`):
```cpp
//! Spectrogram data.
zc_graph_::data_set_dens_t* spectrogram_data_capture_;  // Written by monitor thread
zc_graph_::data_set_dens_t* spectrogram_data_display_;  // Read by FLTK on main thread
st_mutex spectrogram_mutex_;                            // Protects buffer swap
std::atomic<bool> spectrogram_data_ready_{false};       // Flag for buffer swap
```

#### 2. **Create TWO buffers** (in `review::configure_spectrogram()`):
```cpp
// Create BOTH buffers with identical structure
spectrogram_data_display_ = new zc_graph_::data_set_dens_t;
spectrogram_data_capture_ = new zc_graph_::data_set_dens_t;

// Initialize both with same X/Y/Z structure
for (auto* buffer : {spectrogram_data_display_, spectrogram_data_capture_}) {
	buffer->x_values.resize(num_time_samples);
	buffer->y_values.resize(num_freq_bins);
	buffer->z_values.resize(num_time_samples * num_freq_bins);
	// ... fill x_values and y_values ...
}

// FLTK gets display buffer (reads only)
spectrogram_->add_data_set(2, spectrogram_data_display_, map);

// Monitor gets capture buffer (writes only)
monitor_->set_display_buffer(spectrogram_data_capture_, cb_update_spectrogram, this);
```

#### 3. **Signal when data is ready** (in `review::cb_update_spectrogram()`):
```cpp
// Called from monitor thread
void review::cb_update_spectrogram(void* data) {
	review* r = static_cast<review*>(data);
	// Signal that new data is ready for swap
	r->spectrogram_data_ready_.store(true, std::memory_order_release);
	r->spectrogram_->redraw();  // Safe - just sets damage flag
}
```

#### 4. **Swap buffers on main thread** (in `review::cb_ticker()`):
```cpp
void review::cb_ticker(void* data) {
	review* r = static_cast<review*>(data);
	r->poll_text_queue();
	r->poll_decoded_text();

	// Swap spectrogram buffers if monitor has written new data
	if (r->spectrogram_data_ready_.load(std::memory_order_acquire)) {
		std::lock_guard<std::mutex> lock(r->spectrogram_mutex_);
		// Swap pointers - now monitor writes to old display, FLTK reads from old capture
		std::swap(r->spectrogram_data_capture_, r->spectrogram_data_display_);
		r->spectrogram_data_ready_.store(false, std::memory_order_relaxed);
		// TODO: Must tell FLTK widget about the new pointer!
		// Depending on zc_graph_density API, may need to update its internal pointer
	}

	r->g_sgram_->redraw();
	r->td_decoded_->redraw();
}
```

### **Critical Note**

After swapping pointers, we must **update the FLTK widget's internal reference** to the display buffer. The API of `zc_graph_density` needs to be checked - if it caches the pointer from `add_data_set()`, there needs to be a method to update it, OR the widget needs to use indirection (store a pointer-to-pointer).

**Alternative if widget update is complex:** Instead of swapping pointers, **copy** the z_values array:

```cpp
if (r->spectrogram_data_ready_.load(std::memory_order_acquire)) {
	std::lock_guard<std::mutex> lock(r->spectrogram_mutex_);
	// Copy z_values from capture to display
	r->spectrogram_data_display_->z_values = r->spectrogram_data_capture_->z_values;
	r->spectrogram_data_ready_.store(false, std::memory_order_relaxed);
}
```

This is slightly less efficient (copy overhead) but simpler and guaranteed safe.

##  **Summary**

**Bug #1 (FIXED):** `cb_decoder_callback()` called `Fl_Output::value()` from monitor thread → heap corruption  
**Bug #2 (IDENTIFIED):** Monitor thread and FLTK both access same spectrogram buffer with no synchronization → data race → heap corruption

Both bugs need to be fixed for stable operation.

## **Files That Need Changes**

1. `include/review.hpp` - Add mutex, atomic flag, swap method declaration (DONE)
2. `src/review.cpp`:
   - `configure_spectrogram()` - Create both buffers, give capture to monitor (NEEDS MANUAL FIX)
   - `cb_update_spectrogram()` - Set ready flag (DONE)
   - `cb_ticker()` - Swap or copy buffers (DONE, but needs widget update)

## **Next Steps**

1. Verify `zc_graph_density` widget API for updating data pointer after creation
2. Apply configure_spectrogram changes (automated replace failed due to whitespace)
3. Test with both fixes applied
4. May need third fix if zc_graph_density doesn't support pointer updates
