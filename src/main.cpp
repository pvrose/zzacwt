/*
	Copyright 2026, Philip Rose, GM3ZZA

	This file is part of ZZACWT: ZZA CW Trainer.

	ZZACWT is free software: you can redistribute it and/or modify it under the
	terms of the Lesser GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later version.

	ZZACWT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
	PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along with ZZACWT.
	If not, see <https://www.gnu.org/licenses/>.
*/

#include "oscillator.hpp"
#include "codec.hpp"
#include "mod_mixer.hpp"
#include "monitor.hpp"
#include "noise_gen.hpp"
#include "params.hpp"
#include "review.hpp"
#include "shaper.hpp"
#include "text_gen.hpp"
#include "user_if.hpp"

#include "zc_async_queue.h"
#include "zc_audio.h"
#include "zc_drawing.h"
#include "zc_file_holder.h"
#include "zc_fltk.h"
#include "zc_settings.h"
#include "zc_status.h"
#include "zc_ticker.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include <cstdint>
#include <deque>
#include <map>
#include <queue>
#include <string>

// Include Windows headers for console colour support
#ifdef _WIN32
#include <windows.h>
#endif


// Externals included in zc_zzanvad.cpp
extern zc_file_holder* file_holder_;
extern std::string APP_NAME;
extern std::string APP_VERSION;

//! File holder customisation - control data
const std::map < uint8_t, file_control_t > FILE_CONTROL = {
	// ID, { filename, reference, read-only
	{ FILE_SETTINGS, { "ZZACWT.json", false, false, 0 }},
	{ FILE_STATUS, { "status.txt", false, false, 0}},
	{ FILE_ICON_ZZA, { "rose.png", true, true, 0}},
	{ FILE_ICON_PDF, { "pdf.png", true, true, 0}},
	{ FILE_TEXT_FILE, { "text.txt", true, true, 0 }},
	{ FILE_WORD_LIST, { "word_list.txt", true, true, 0 }},
	{ FILE_QSO_DATA, { "qso_data.txt", true, true, 0 }}
};

double DEFAULT_SAMPLE_RATE = 24000.0; //!< Default sample rate for audio generation
double DEFAULT_BASE_PITCH = 700.0; //!< Default base pitch for the oscillator in Hz
double DEFAULT_RISE_FALL = 0.005; //!< Default rise time for the audio envelope in seconds
double DEFAULT_WPM = 12.0; //!< Default speed in words per minute
double MAXIMUM_WPM = 40.0; //!< Maximum speed in words per minute. Do not let decode go beyond this.
double MINIMUM_WPM = 6.0;  //!< Minimum speed in words per minute.
int OUTPUT_CHUNK_SIZE = 4096; //!< Threshold for when the oscillator should generate more audio samples (in samples)
int GENERATION_CHUNK_SIZE = 4096; //!< Number of audio samples to generate in each batch when the oscillator is generating audio samples
int OSCILLATOR_CHUNK_SIZE = 4096; //!< Number of audio samples to generate in each batch when the oscillator is generating audio samples
int NOISE_CHUNK_SIZE = 4096; //!< Number of audio samples to generate in each batch when the noise generator is generating audio samples
int SHAPER_CHUNK_SIZE = 4096; //!< Number of audio samples to process in each batch when the shaper is processing audio samples
int LOWER_CHUNK_SIZE = 512; //!< Minimum queue size before waking producers (~23ms buffer at 22050 Hz)

int DEFAULT_FFT_SIZE = 256;  //!< Default FFT size.
double DEFAULT_OVERLAP = 75.0;        //!< Default FFT sampling overlap
double DEFAULT_MAX_PITCH = 3000.0;    //!< Default maximum frequency on display
double DEFAULT_MAX_TIME = 5.0;        //!< Default maximum timeon display

int BUFFER_DEPTH = 64; //!< Default portaudio buffer

oscillator* oscillator_ = nullptr; //!< Pointer to the oscillator instance
text_gen* text_gen_ = nullptr; //!< Pointer to the text generator instance
shaper* shaper_ = nullptr; //!< Pointer to the shaper instance
noise_gen* noise_gen_ = nullptr; //!< Pointer to the noise generator instance
mod_mixer* mod_mixer_ = nullptr; //!< Pointer to the modulator/mixer instance
zc_audio* speaker_ = nullptr; //!< Pointer to the speaker instance
review* review_ = nullptr; //!< Pointer to the review window instance
monitor* monitor_ = nullptr; //!< Pointer to the monitor window instance
zc_audio* microphone_ = nullptr; //!< Pointer to the microphone instance
zc_async_queue<std::string>* mon_text_q_ = nullptr; //!< Pointer to monitored text interface

bool restart_ = false; //!< Flag to indicate that the app should be restarted - when the user changes a setting that requires a restart

int main(int argc, char** argv)
{
	// Allow multi-threading involvement with FLTK locking mechanidm
	Fl::lock();
#ifdef _WIN32
	// Enable Windows console colour support (ANSI escape sequences)
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		SetConsoleMode(hOut, dwMode);
	}
#endif
	file_holder_ = new zc_file_holder(argv[0], FILE_CONTROL);
	zc_settings settings;
	int base_size;
	settings.get("Base Size", base_size, DEFAULT_DEFAULT_SIZE);
	zc::customise_fltk(base_size);
	status_ = new zc_status(zc_status::HAS_CONSOLE, {});
	ticker_ = new zc_ticker();

	// Create the monitor queue
	mon_text_q_ = new zc_async_queue<std::string>;
	// Create the combined audio queue
	zc_async_queue<double>* audio_out_queue = new zc_async_queue<double>();
	zc_async_queue<double>* audio_monitor_queue = new zc_async_queue<double>();
	zc_async_queue<double>* audio_receive_queue = new zc_async_queue<double>();
	double sample_rate;
	settings.get("Sample Rate", sample_rate, DEFAULT_SAMPLE_RATE);
	// Create the speaker before user if.
	speaker_ = new zc_audio(zc_audio_direction::AUDIO_OUT, 1, sample_rate, audio_out_queue, audio_monitor_queue);
	// Create the microphone
	microphone_ = new zc_audio(zc_audio_direction::AUDIO_IN, 1, sample_rate, audio_receive_queue, nullptr);

	// Create the main user interface window
	user_if* window = new user_if(600, 800);
	std::string label = APP_NAME + " v" + APP_VERSION + " - CW Trainer";
	window->copy_label(label.c_str());
	// Create the oscillator output queue
	zc_async_queue<double>* carrier_queue = new zc_async_queue<double>();
	// Create the oscilaltor
	oscillator_ = new oscillator(carrier_queue);
	// Create the audio envelope queue
	zc_async_queue<double>* envelope_queue = new zc_async_queue<double>();
	// Create the shaper
	shaper_ = new shaper(envelope_queue, mon_text_q_);
	// Create the text generator
	text_gen_ = new text_gen();
	// Create the inserted noise queue
	zc_async_queue<double>* noise_queue = new zc_async_queue<double>();
	// Create the noise generator
	noise_gen_ = new noise_gen(noise_queue);
	// Create the modulator/mixer, passing producer objects for wake-up
	mod_mixer_ = new mod_mixer(carrier_queue, envelope_queue, noise_queue, audio_out_queue,
		oscillator_, shaper_, noise_gen_);

	//// Initialize and enable the speaker (uses default audio device)
	//if (!speaker_->use_port(0)) {
	//	status_->misc_status(ST_ERROR, "Failed to select audio port");
	//	return 1;
	//}
	//status_->misc_status(ST_OK, "Audio output initialized successfully");

	monitor_ = new monitor(audio_monitor_queue, audio_receive_queue);

	// Show the window
	window->show(argc, argv);

	review_ = new review(600, 800);
	label = APP_NAME + " v" + APP_VERSION + " - Review";
	review_->copy_label(label.c_str());
	review_->add_sent_text_queue(mon_text_q_);
	review_->show();

	// Run the FLTK event loop
	bool result = Fl::run();

	// Clean up resources - this will not be reached until the application is closed, but it's good practice to include it here for completeness and in case of future changes that might allow for a cleaner shutdown process.
	// IMPORTANT: Shutdown order matters to avoid accessing deleted objects and to ensure threads are properly stopped

	// Step 1: Stop audio output first to stop consuming from queues
	if (speaker_) {
		delete speaker_;
		speaker_ = nullptr;
	}

	// Step 2: Shutdown all queues to wake up any blocked threads before deleting consumers/producers
	// This prevents threads from being blocked in wait_and_pop() when their objects are destroyed
	if (carrier_queue) carrier_queue->shutdown();
	if (envelope_queue) envelope_queue->shutdown();
	if (noise_queue) noise_queue->shutdown();
	if (audio_out_queue) audio_out_queue->shutdown();
	if (audio_monitor_queue) audio_monitor_queue->shutdown();
	if (mon_text_q_) mon_text_q_->shutdown();

	// Step 3: Stop the consumer (mod_mixer) which will stop calling wait_and_pop on producer queues
	if (mod_mixer_) {
		delete mod_mixer_;
		mod_mixer_ = nullptr;
	}

	// Step 4: Stop producers (they now know no one is consuming)
	if (noise_gen_) {
		delete noise_gen_;
		noise_gen_ = nullptr;
	}
	if (shaper_) {
		delete shaper_;
		shaper_ = nullptr;
	}
	if (oscillator_) {
		delete oscillator_;
		oscillator_ = nullptr;
	}

	// Step 5: Stop monitor (processes audio samples)
	if (monitor_) {
		delete monitor_;
		monitor_ = nullptr;
	}

	// Step 6: Clean up GUI components
	if (text_gen_) {
		delete text_gen_;
		text_gen_ = nullptr;
	}
	if (review_) {
		delete review_;
		review_ = nullptr;
	}
	if (window) {
		delete window;
		window = nullptr;
	}

	// Step 7: Delete queues (now that no threads are using them)
	delete carrier_queue;
	delete envelope_queue;
	delete noise_queue;
	delete audio_out_queue;
	delete audio_monitor_queue;
	delete mon_text_q_;
	mon_text_q_ = nullptr;

	if (restart_) {
		restart_ = false;
		// If the restart flag is set, restart the application by re-executing the main function.
		// This is a simple way to apply settings that require a restart without needing to implement a more complex state management system.
		result = main(argc, argv);
	}
	return result;
}

//! Restart the application - sets the restart flag and closes all windows to trigger a restart in main()
void restart_application()
{
	restart_ = true;
	// Hide all the open windows - this will allow Fl to close the app.
	Fl_Window* wx = Fl::first_window();
	for (; wx; wx = Fl::first_window()) {
		// Keep the banner showing if we need to see a severe or fatal error.
		wx->hide();
	}
}
