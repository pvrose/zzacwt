# Capturing Thread Debug Output

## Problem
The `[THREAD]` messages are going to a console window that disappears when the application closes.

## Solution: Redirect stderr to a file

### Option 1: Add Code to main.cpp (Recommended)

Add this at the very beginning of your `main()` function:

```cpp
#include <cstdio>

int main(int argc, char* argv[]) {
	// Redirect stderr to file for thread debugging
	FILE* stderr_file = nullptr;
	freopen_s(&stderr_file, "thread_debug.log", "w", stderr);
	if (stderr_file) {
		setvbuf(stderr, nullptr, _IONBF, 0); // Unbuffered for immediate output
	}

	// ... rest of your existing main code ...
}
```

**Result:** All `[THREAD]` messages will be written to `thread_debug.log` in the same directory as the executable.

### Option 2: Run from Command Line

Open Command Prompt or PowerShell and run:

```cmd
cd C:\Users\pvros\source\repos\zzacwt\out\build\x64-Debug
zzacwt.exe 2> thread_debug.log
```

**Result:** stderr redirected to thread_debug.log

### Option 3: Keep Console Window Open (Quick Test)

Add this at the END of your `main()` function (before final return):

```cpp
	fprintf(stderr, "\n[PRESS ENTER TO EXIT]\n");
	getchar();
	return 0;
}
```

**Result:** Console stays open so you can read the messages.

## What to Look For

After running, check `thread_debug.log` for:

### Normal Output (all good):
```
[THREAD] Oscillator thread started, ID: 12345678
[THREAD] Shaper thread started, ID: 23456789
[THREAD] Monitor thread started, ID: 34567890
[THREAD] Noise_gen thread started, ID: 45678901
[THREAD] Mod_mixer thread started, ID: 56789012
[THREAD] Oscillator thread exiting normally, ID: 12345678
[THREAD] Shaper thread exiting normally, ID: 23456789
[THREAD] Monitor thread exiting normally, ID: 34567890
[THREAD] Noise_gen thread exiting normally, ID: 45678901
[THREAD] Mod_mixer thread exiting normally, ID: 56789012
```

### Exception Output (problem identified):
```
[THREAD] Oscillator thread started, ID: 12345678
[THREAD] Shaper thread started, ID: 23456789
[THREAD] Monitor thread started, ID: 34567890
[THREAD] Noise_gen thread started, ID: 45678901
[THREAD] Mod_mixer thread started, ID: 56789012
[THREAD] Shaper thread exception, ID: 23456789, error: std::out_of_range at index 5
[THREAD] Oscillator thread exiting normally, ID: 12345678
[THREAD] Monitor thread exiting normally, ID: 34567890
[THREAD] Noise_gen thread exiting normally, ID: 45678901
[THREAD] Mod_mixer thread exiting normally, ID: 56789012
```

## Matching to Debug Output

From Visual Studio Debug Output:
```
The thread 26540 has exited with code 1 (0x1).
```

Thread 26540 is the Windows thread ID. Our log shows a hash of std::thread::id.

**If NO exception is logged** but thread 26540 exits with code 1, it means:
- Thread 26540 is NOT one of our 5 worker threads
- It's likely a PortAudio, FLTK, or Windows system thread
- The exception is happening in library code, not our code

**If an exception IS logged**, we can identify exactly which thread failed and why.

## Next Steps

1. Add stderr redirect to main.cpp (Option 1 above)
2. Rebuild
3. Run application
4. Close application
5. Check `thread_debug.log` file
6. Report what you find

The log file will be in:
```
C:\Users\pvros\source\repos\zzacwt\out\build\x64-Debug\thread_debug.log
```

Or wherever your executable is running from.
