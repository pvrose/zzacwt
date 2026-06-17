// Redirect stderr to a file for capturing thread debug messages
// Add this at the very top of main() before any other code

#pragma once
#include <cstdio>

inline void redirect_stderr_to_file() {
	FILE* stderr_file = nullptr;
	freopen_s(&stderr_file, "thread_debug.log", "w", stderr);
	if (stderr_file) {
		// Make stderr unbuffered so messages appear immediately
		setvbuf(stderr, nullptr, _IONBF, 0);
	}
}
