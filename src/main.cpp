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
#include "noise_gen.hpp"
#include "review.hpp"
#include "shaper.hpp"
#include "text_gen.hpp"
#include "user_if.hpp"

#include "zc_async_queue.h"
#include "zc_audio_data.h"
#include "zc_file_holder.h"
#include "zc_fltk.h"
#include "zc_speaker.h"
#include "zc_status.h"
#include "zc_ticker.h"

#include <FL/Fl.H>

#include <cstdint>
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
	{ FILE_SETTINGS, { "ZZAVNAD.json", false, false, 0 }},
	{ FILE_STATUS, { "status.txt", false, false, 0}},
	{ FILE_ICON_ZZA, { "rose.png", true, true, 0}},
	{ FILE_TEXT_FILE, { "text.txt", true, true, 0 }},
	{ FILE_WORD_LIST, { "word_list.txt", true, true, 0 }}
};

float DEFAULT_SAMPLE_RATE = 22050.0F; //!< Default sample rate for audio generation
float DEFAULT_BASE_PITCH = 700.0F; //!< Default base pitch for the oscillator in Hz
float DEFAULT_RISE_FALL = 0.005F; //!< Default rise time for the audio envelope in seconds
float DEFAULT_WPM = 12.0F; //!< Default speed in words per minute
int UPPER_QUEUE_THRESHOLD = 64; //!< Threshold for when the oscillator should generate more audio samples (in samples)
int LOWER_QUEUE_THRESHOLD = 16; //!< Threshold for when the oscillator should stop generating audio samples (in samples)
int GENERATION_CHUNK_SIZE = 128; //!< Number of audio samples to generate in each batch when the oscillator is generating audio samples
int OSCILLATOR_CHUNK_SIZE = 64; //!< Number of audio samples to generate in each batch when the oscillator is generating audio samples
int NOISE_CHUNK_SIZE = 64; //!< Number of audio samples to generate in each batch when the noise generator is generating audio samples
int SHAPER_CHUNK_SIZE = 128; //!< Number of audio samples to process in each batch when the shaper is processing audio samples
oscillator* oscillator_ = nullptr; //!< Pointer to the oscillator instance
text_gen* text_gen_ = nullptr; //!< Pointer to the text generator instance
shaper* shaper_ = nullptr; //!< Pointer to the shaper instance
noise_gen* noise_gen_ = nullptr; //!< Pointer to the noise generator instance
mod_mixer* mod_mixer_ = nullptr; //!< Pointer to the modulator/mixer instance
zc_speaker* speaker_ = nullptr; //!< Pointer to the speaker instance
review* review_ = nullptr; //!< Pointer to the review window instance

// In-fill logic. Take the metadata as it's sent by speaker and send it to review.
static void audio_metadata_callback(const std::string& metadata)
{
	if (review_ && !metadata.empty()) {
		review_->add_sent_text(metadata, text_source_t::SENT_TEXT);
	}
}

int main(int argc, char** argv)
{
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
	zc::customise_fltk();
	status_ = new zc_status(zc_status::HAS_CONSOLE, {});
	ticker_ = new zc_ticker();

	// Create the main user interface window
	user_if* window = new user_if(600, 800);
	std::string label = APP_NAME + " v" + APP_VERSION + " - CW Trainer";
	window->copy_label(label.c_str());
	// Create the oscillator output queue
	zc_async_queue<float>* carrier_queue = new zc_async_queue<float>();
	// Create the oscilaltor
	oscillator_ = new oscillator(carrier_queue);
	// Create the audio envelope queue
	zc_async_queue<zc_audio_data>* envelope_queue = new zc_async_queue<zc_audio_data>();
	// Create the shaper
	shaper_ = new shaper(envelope_queue);
	// Create the text generator
	text_gen_ = new text_gen();
	// Create the inserted noise queue
	zc_async_queue<float>* noise_queue = new zc_async_queue<float>();
	// Create the noise generator
	noise_gen_ = new noise_gen(noise_queue);
	// Create the combined audio queue
	zc_async_queue<zc_audio_data>* audio_out_queue = new zc_async_queue<zc_audio_data>();
	// Create the modulator/mixer
	mod_mixer_ = new mod_mixer(carrier_queue, envelope_queue, noise_queue, audio_out_queue);
	// Create the speaker
	speaker_ = new zc_speaker(audio_out_queue);
	speaker_->set_text_callback(audio_metadata_callback);

	// Initialize and enable the speaker (uses default audio device)
	if (!speaker_->use_port(0)) {
		status_->misc_status(ST_ERROR, "Failed to select audio port");
		return 1;
	}
	status_->misc_status(ST_OK, "Audio output initialized successfully");


	// Show the window
	window->show(argc, argv);

	review_ = new review(600, 800);
	label = APP_NAME + " v" + APP_VERSION + " - Review";
	review_->copy_label(label.c_str());
	review_->show();

	// Run the FLTK event loop
	return Fl::run();
}
