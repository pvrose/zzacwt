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
#include "shaper.hpp"

#include "params.hpp"
#include "codec.hpp"
#include "text_gen.hpp"

#include "zc_async_queue.h"
#include "zc_audio_data.h"
#include "zc_settings.h"

#include <cmath>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

constexpr float MARK_VALUE = 1.0F; //!< Value of the audio envelope when in mark state
constexpr float SPACE_VALUE = 0.0F; //!< Value of the audio envelope when in space state

extern int UPPER_QUEUE_THRESHOLD;
extern int LOWER_QUEUE_THRESHOLD;
extern int GENERATION_CHUNK_SIZE;
extern int SHAPER_CHUNK_SIZE;
extern float DEFAULT_RISE_FALL;

extern text_gen* text_gen_; //!< Pointer to the text generator instance

// Constructor
shaper::shaper(zc_async_queue<zc_audio_data>* audio_data_queue)
	: audio_data_queue_(audio_data_queue),
	rng_(std::random_device{}())
{
	// Initialise settings
	apply_settings();
}

// Destructor
shaper::~shaper()
{
	// Signal the generation thread to stop and wait for it to finish
	stop_generation_ = true;
	if (generation_thread_.joinable()) {
		generation_thread_.join();
	}
}

// Apply settings and update internal state
void shaper::apply_settings() {
	// Read settings and update internal state as they may have changed.
	zc_settings settings;
	settings.get("Dot Speed", character_speed_, 12.0F);
	disturber_type disturber;
	settings.get("Disturber Type", disturber, disturber_type::NONE);
	if (disturber == disturber_type::TIMING) {
		settings.get("Timing Disturbance", timing_disturbance_level_, 0);
	}
	else {
		timing_disturbance_level_ = 0; // No timing disturbance if disturber type is not timing
	}
	if (disturber == disturber_type::SOFTNESS) {
		// Softness is defined in milliseconds in the settings, convert to seconds.
		float softness;
		settings.get("Softness", softness, 10.0F);
		rise_fall_time_ = softness / 1000.0F;
	}
	else {
		rise_fall_time_ = DEFAULT_RISE_FALL; // Default softness if disturber type is not softness
	}
	settings.get("Speed Type", speed_mode_, speed_type::NORMAL);
	if (speed_mode_ == speed_type::FARNSWORTH) {
		settings.get("Farnsworth", farnsworth_speed_, 12.0F);
	}
	if (speed_mode_ == speed_type::WORDSWORTH) {
		settings.get("Wordsworth", wordsworth_speed_, 12.0F);
	}
	update_symbol_durations();
	// Start the generation thread if not already running
	if (!generation_thread_.joinable()) {
		generation_thread_ = std::thread(generation_loop, this);
	}
}

// Update symbol durations based on current speed settings
void shaper::update_symbol_durations() {
	dot_time_ = 1.2F / character_speed_; // Standard formula for dot time based on WPM
	symbol_durations_[symbol_t::DOT_MARK] = dot_time_;
	symbol_durations_[symbol_t::DASH_MARK] = 3 * dot_time_;
	symbol_durations_[symbol_t::INTERNAL_SPACE] = dot_time_;
	switch (speed_mode_) {
	case speed_type::NORMAL:
		symbol_durations_[symbol_t::CHARACTER_SPACE] = 3 * dot_time_;
		symbol_durations_[symbol_t::WORD_SPACE] = 7 * dot_time_;
		break;
	case speed_type::FARNSWORTH:
	{
		float farnsworth_time = (60.0F / farnsworth_speed_) - (31.0F * 1.2F / character_speed_); // Total additional time needed for Farnsworth
		symbol_durations_[symbol_t::CHARACTER_SPACE] = farnsworth_time * 3.0F / 19.0F; // Character space is 3/19 of total additional time
		symbol_durations_[symbol_t::WORD_SPACE] = farnsworth_time * 7.0F / 19.0F; // Word space is 7/19 of total additional time
		break;
	}
	case speed_type::WORDSWORTH:
	{
		float wordsworth_time = (60.0F / wordsworth_speed_) - (43.0F * 1.2F / character_speed_); // Total additional time needed for Wordsworth
		symbol_durations_[symbol_t::CHARACTER_SPACE] = 3 * dot_time_; // Character space remains unchanged in Wordsworth
		symbol_durations_[symbol_t::WORD_SPACE] = wordsworth_time; // Word space is increased by the total additional time
		break;
	}
	default:
		symbol_durations_[symbol_t::CHARACTER_SPACE] = 3 * dot_time_;
		symbol_durations_[symbol_t::WORD_SPACE] = 7 * dot_time_;
		break;
	}
}

//! Generation loop for the generation thread
void shaper::generation_loop(shaper* that) {
	while (!that->stop_generation_) {
		if (!text_gen_) continue;
		if (that->clear_requested_) {
			// Clear the audio data queue and reset internal state
			that->audio_data_queue_->clear();
			that->is_mark_ = false; // Reset to space state
			that->clear_requested_ = false; // Reset the clear request flag
		}

		if (that->audio_data_queue_->size() < LOWER_QUEUE_THRESHOLD) {
			// Get the next word from the text generator
			std::string word = text_gen_->get_next_word();
			// Create audio data for the word
			if (word.empty()) {
				zc_audio_data audio_data;
				// Generate silence/zero envelope to keep pipeline flowing
				// Push a small block of zeros (or appropriate envelope values)
				for (int i = 0; i < SHAPER_CHUNK_SIZE; ++i) {  // Match a reasonable block size
					audio_data.data.push(0.0F);
				}
				audio_data.metadata = "";
				that->audio_data_queue_->push(audio_data);
			}
			else {
				// Normal processing
				std::vector<std::vector<symbol_t>> symbols;
				int index = 0;
				codec::encode(word, symbols);
				int sample_countdown = SHAPER_CHUNK_SIZE; // Countdown to control chunk sizes for pushing to the queue
				for (const auto& char_symbols : symbols) {
					bool meta_data_set = false; // Flag to track if metadata has been set for the current word
					for (const auto& symbol : char_symbols) {
						zc_audio_data audio_data;
						that->generate_envelope(symbol, audio_data.data);

						if (!meta_data_set) {
							if (index < word.size() - 1) {
								// Check if the character was a prosign (starts with < and ends with >) and set metadata accordingly
								if (word[index] == '<' && word.back() == '>') {
									audio_data.metadata = word; // Set metadata to the whole word for prosigns
								}
								else {
									audio_data.metadata = word[index]; // Set metadata to the word for the first symbol of the word
								}
							}
							else {
								audio_data.metadata = std::string(1, word[index]) + " ";
							}

							meta_data_set = true;
						}
						else {
							audio_data.metadata = ""; // Set metadata if needed
						}
						// Split the generated audio data into chunks and push to the queue
						zc_audio_data* audio_data_ptr = new zc_audio_data();
						audio_data_ptr->metadata = audio_data.metadata; // Copy metadata to the chunk
						while (!audio_data.data.empty()) {
							audio_data_ptr->data.push(audio_data.data.front());
							audio_data.data.pop();
							--sample_countdown;
							if (sample_countdown <= 0) {
								that->audio_data_queue_->push(*audio_data_ptr); // Push the chunk to the queue
								delete audio_data_ptr; // Clean up the chunk after pushing
								audio_data_ptr = new zc_audio_data(); // Create a new chunk for the next set of samples
								audio_data_ptr->metadata = ""; // Clear metadata for subsequent chunks
								sample_countdown = SHAPER_CHUNK_SIZE; // Reset countdown for the next chunk
							}
						}
						delete audio_data_ptr; // Clean up the last chunk if it wasn't pushed
					}
					++index;

				}
			}
		}
		// Yield to allow other threads to run and check for stop signal
		std::this_thread::yield();
	}
}

//! Generate the audio envelope for the specified symbol
void shaper::generate_envelope(symbol_t symbol, std::queue<float>& audio_samples) {
	// Get the target state (mark or space) and duration for the symbol
	bool target_mark = (symbol == symbol_t::DOT_MARK || symbol == symbol_t::DASH_MARK);
	// Calculate the total duration for the symbol including timing disturbance
	float duration = symbol_durations_[symbol] + generate_delta_t();
	if (duration < 0.0F) {
		// If the disturbance results in a negative duration, we will treat it as an overshoot disturbance and add a brief overshoot before transitioning to the target state.
		add_overshoot(audio_samples, -duration, target_mark); 
	}
	else {
		// Add a raised cosine transition to the target state
		add_raised_cosine(audio_samples, rise_fall_time_, target_mark);
	}
	// Add samples for the remaining duration of the symbol
	float value = target_mark ? MARK_VALUE : SPACE_VALUE;
	int num_samples = static_cast<int>((duration - rise_fall_time_) * DEFAULT_SAMPLE_RATE);
	for (int i = 0; i < num_samples; ++i) {
		audio_samples.push(value);
	}
}

//! Add a raised cosine transition onto the specified audio sample queue
void shaper::add_raised_cosine(std::queue<float>& audio_samples, float duration, bool target_mark) {
	int num_samples = static_cast<int>(duration * DEFAULT_SAMPLE_RATE);
	float start_value = is_mark_ ? MARK_VALUE : SPACE_VALUE;
	float end_value = target_mark ? MARK_VALUE : SPACE_VALUE;
	for (int i = 0; i < num_samples; ++i) {
		float t = static_cast<float>(i) / num_samples; // Normalized time (0 to 1)
		float envelope_value = start_value + (end_value - start_value) * 0.5F * (1 - cosf(3.14159265F * t)); // Raised cosine formula
		audio_samples.push(envelope_value);
	}
	is_mark_ = target_mark; // Update current state to target state after transition
}

//! Add an overshoot disturbance to the specified audio sample queue
//! For now keep it simple, for each millisecond of overshoot duration,
//! add a brief overshoot of 10% above the target mark value or 10% below the target space value,
//! tapering off over the duration of the overshoot.
void shaper::add_overshoot(std::queue<float>& audio_samples, float duration, bool target_mark) {
	int num_samples = static_cast<int>(duration * DEFAULT_SAMPLE_RATE);
	float start_value = is_mark_ ? MARK_VALUE : SPACE_VALUE;
	float end_value = target_mark ? MARK_VALUE : SPACE_VALUE;
	// No overshoot if the target state is the same as the current state
	if (start_value == end_value) {
		for (int i = 0; i < num_samples; ++i) {
			audio_samples.push(end_value);
		}
		return;
	}
	float overshoot_value = target_mark ? MARK_VALUE + (duration * 0.1F) : SPACE_VALUE - (duration * 0.1F);
	for (int i = 0; i < num_samples; ++i) {
		float t = static_cast<float>(i) / num_samples; // Normalized time (0 to 1)
		float envelope_value = start_value + (overshoot_value - start_value) * (1 - t) + (end_value - overshoot_value) * t;
		audio_samples.push(envelope_value);
	}
	is_mark_ = target_mark; // Update current state to target state after transition
}

//! Generate timing disturbance value
float shaper::generate_delta_t() {
	if (timing_disturbance_level_ == 0) {
		return 0.0F; // No disturbance
	}
	// Define the disturbance range based on the level
	float min_factor = 1.0F - 0.05F * timing_disturbance_level_; // Minimum factor (e.g. 0.98 for level 1)
	float max_factor = 1.0F + 0.10F * timing_disturbance_level_; // Maximum factor (e.g. 1.05 for level 1)
	// TODO: Implement a non-uniform distribution with a mean of 1.0F. 
	// For simplicity, we will use a uniform distribution here, 
	// but you can replace this with a more complex distribution as needed.
	std::uniform_real_distribution<float> dist(min_factor, max_factor);
	return (dist(rng_) - 1.0F) * dot_time_; // Return the disturbance as a delta time based on the dot time
}

//! Clear the generation of audio samples
void shaper::clear() {
	clear_requested_ = true; // Signal the generation loop to clear the queue and reset state
}
