# Thread-Safe Queue Implementation Notes

## Problem Identified
Race condition in `zc_speaker.cpp` at line 177:
```cpp
*next_buffer_ = speech_->front();
```

The `speech_` queue is accessed from multiple threads:
- **PortAudio callback thread** (reading)
- **Another thread** (writing/pushing audio data)

This causes memory corruption with `_Map` pointer showing garbage value `0x111011101110112`.

## Solutions

### Option 1: Simple Mutex-Protected Wrapper (Recommended for Audio)

```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
	std::queue<T> queue_;
	mutable std::mutex mutex_;
	std::condition_variable cond_;

public:
	void push(T value) {
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.push(std::move(value));
		cond_.notify_one();
	}

	bool try_pop(T& value) {
		std::lock_guard<std::mutex> lock(mutex_);
		if (queue_.empty()) return false;
		value = std::move(queue_.front());
		queue_.pop();
		return true;
	}

	T& front() {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.front();
	}

	void pop() {
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.pop();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.empty();
	}

	size_t size() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.size();
	}
};
```

**Usage:**
```cpp
// Replace std::queue<zc_audio_data>* speech_
// with ThreadSafeQueue<zc_audio_data>* speech_

// In callback:
zc_audio_data buffer;
if (speech_->try_pop(buffer)) {
	*next_buffer_ = buffer;
	// process...
}

// When adding data:
speech_->push(audio_data);
```

### Option 2: Boost.Lockfree (High Performance)
```cpp
#include <boost/lockfree/queue.hpp>

// Fixed capacity, lock-free
boost::lockfree::queue<zc_audio_data*> queue(128);

// Push
queue.push(data_ptr);

// Pop
zc_audio_data* data;
if (queue.pop(data)) {
	// use data
}
```

### Option 3: Intel TBB
```cpp
#include <tbb/concurrent_queue.h>

tbb::concurrent_queue<zc_audio_data> queue;

// Push
queue.push(data);

// Pop
zc_audio_data data;
if (queue.try_pop(data)) {
	// use data
}
```

### Option 4: moodycamel::ConcurrentQueue
Header-only, very fast. See: https://github.com/cameron314/concurrentqueue

## Implementation Considerations

1. **Keep critical sections short** - Don't hold locks while processing audio
2. **Protect ALL access points** - Every read and write to the shared queue
3. **Consider next_buffer_** - Check if it needs protection too
4. **Audio callback timing** - Mutex overhead is usually negligible for audio callbacks

## Quick Fix for zc_speaker

Add mutex as member variable:
```cpp
std::mutex speech_mutex_;
```

Protect all `speech_` operations:
```cpp
// Reading in callback:
{
	std::lock_guard<std::mutex> lock(speech_mutex_);
	if (!speech_->empty()) {
		*next_buffer_ = speech_->front();
		speech_->pop();
	}
}

// Writing from other thread:
{
	std::lock_guard<std::mutex> lock(speech_mutex_);
	speech_->push(audio_data);
}
```

## Note
This issue affects code reused in multiple projects. Consider creating a shared thread-safe queue utility class.
