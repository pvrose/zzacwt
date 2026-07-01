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
#include "review.hpp"
#include "text_gen.hpp"

#include "zc_active_queue.h"
#include "zc_settings.h"

#include <cmath>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Enable queue monitoring in debug builds
#ifdef _DEBUG
#define ENABLE_QUEUE_MONITORING
#endif

#ifdef ENABLE_QUEUE_MONITORING
#include <chrono>
#include <iostream>
#endif

constexpr double MARK_VALUE = 1.0F; //!< Value of the audio envelope when in mark state
constexpr double SPACE_VALUE = 0.0F; //!< Value of the audio envelope when in space state

extern int SHAPER_CHUNK_SIZE;
extern double DEFAULT_RISE_FALL;

extern text_gen* text_gen_; //!< Pointer to the text generator instance
extern review* review_; //!< Pointer to the review instance

// Constructor
shaper::shaper(zc_active_queue<double>* audio_data_queue, zc_async_queue<std::string>* text_queue)
	: audio_data_queue_(audio_data_queue),
	  text_queue_(text_queue),
	  rng_(std::random_device{}())
{
	// Initialise settings
	apply_settings();
	audio_data_queue_->set_low_callback(cb_audio_data_queue_low, this, true);
}

// Destructor
shaper::~shaper()
{
}

// Apply settings and update internal state
void shaper::apply_settings() {
	// Read settings and update internal state as they may have changed.
	zc_settings settings;
	settings.get("Dot Speed", dot_speed_, 12.0);
	content_mode mode;
	settings.get("Content Mode", mode, content_mode::LETTERS);
	test_mode_b_ = mode == content_mode::TEST_MODE_B;
	settings.get("Overall WPM", overall_speed_, 12.0);
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
		double softness;
		settings.get("Softness", softness, 10.0);
		rise_fall_time_ = softness / 1000.0;
	}
	else {
		rise_fall_time_ = DEFAULT_RISE_FALL; // Default softness if disturber type is not softness
	}
	settings.get("Speed Type", speed_mode_, speed_type::NORMAL);
	settings.get("Sample Rate", sample_rate_, sample_rate_);
	update_symbol_durations();
}

// Update symbol durations based on current speed settings
void shaper::update_symbol_durations() {
	dot_time_ = 1.2 / dot_speed_; // Standard formula for dot time based on WPM
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
		double farnsworth_time = (60.0 / overall_speed_) - (31.0 * 1.2 / dot_speed_); // Total additional time needed for Farnsworth
		symbol_durations_[symbol_t::CHARACTER_SPACE] = farnsworth_time * 3.0 / 19.0; // Character space is 3/19 of total additional time
		symbol_durations_[symbol_t::WORD_SPACE] = farnsworth_time * 7.0 / 19.0; // Word space is 7/19 of total additional time
		break;
	}
	case speed_type::WORDSWORTH:
	{
		double wordsworth_time = (60.0 / overall_speed_) - (43.0 * 1.2 / dot_speed_); // Total additional time needed for Wordsworth
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

// Callback function for when the audio data queue is low
void shaper::cb_audio_data_queue_low(void* user_data) {
	shaper* that = static_cast<shaper*>(user_data);
	if (that && that->audio_data_queue_) {
		if (text_gen_ == nullptr) {
			// If text_gen_ is not initialized, we cannot generate new words.
			for (int i = 0; i < SHAPER_CHUNK_SIZE; ++i) {
				if (that->test_mode_b_) that->audio_data_queue_->push(1.0);
				else that->audio_data_queue_->push(0.0);
			}
			return;
		}
		int sample_count = 0;
		while (sample_count < SHAPER_CHUNK_SIZE) {
			std::string word = text_gen_->get_next_word();
			if (word.empty()) {
				// Generate silence/zero envelope to keep pipeline flowing
				while (sample_count < SHAPER_CHUNK_SIZE) {
					if (that->test_mode_b_) that->audio_data_queue_->push(1.0);
					else that->audio_data_queue_->push(0.0);
					++sample_count;
				}
				that->text_queue_->push(""); // Push an empty string to indicate no word
			}
			else {
				std::vector<std::vector<symbol_t>> symbols;
				int index = 0;
				codec::encode(word, symbols);
				for (const auto& char_symbols : symbols) {
					bool meta_data_set = false; // Flag to track if metadata has been set for the current word
					for (const auto& symbol : char_symbols) {
						std::string metadata = ""; // Initialize metadata for the current symbol
						sample_count += that->generate_envelope(symbol);
						if (!meta_data_set) {
							if (index < word.size() - 1) {
								// Check if the character was a prosign (starts with < and ends with >) and set metadata accordingly
								if (word[index] == '<' && word.back() == '>') {
									metadata = word; // Set metadata to the whole word for prosigns
								}
								else {
									metadata = word[index]; // Set metadata to the word for the first symbol of the word
								}
							}
							else {
								metadata = std::string(1, word[index]) + " ";
							}
							meta_data_set = true;
						}
						else {
							metadata = ""; // Set metadata if needed
						}
						that->text_queue_->push(metadata); // Push metadata to the text queue
					}
					++index;
				}
			}
		}
	}
}

//! Generate the audio envelope for the specified symbol
int shaper::generate_envelope(symbol_t symbol) {
	int result = 0;
	// Get the target state (mark or space) and duration for the symbol
	bool target_mark = (symbol == symbol_t::DOT_MARK || symbol == symbol_t::DASH_MARK);
	// Calculate the total duration for the symbol including timing disturbance
	double duration = symbol_durations_[symbol] + generate_delta_t();
	if (duration < 0.0F) {
		// If the disturbance results in a negative duration, we will treat it as an overshoot disturbance and add a brief overshoot before transitioning to the target state.
		result += add_overshoot(-duration, target_mark); 
	}
	else {
		// Add a raised cosine transition to the target state
		result += add_raised_cosine(rise_fall_time_, target_mark);
	}
	// Add samples for the remaining duration of the symbol
	double value = target_mark ? MARK_VALUE : SPACE_VALUE;
	int num_samples = static_cast<int>((duration - rise_fall_time_) * sample_rate_);
	for (int i = 0; i < num_samples; ++i) {
		audio_data_queue_->push(value);
	}
	result += num_samples;
	return result;
}

//! Add a raised cosine transition onto the specified audio sample queue
int shaper::add_raised_cosine(double duration, bool target_mark) {
	int num_samples = static_cast<int>(duration * sample_rate_);
	double start_value = is_mark_ ? MARK_VALUE : SPACE_VALUE;
	double end_value = target_mark ? MARK_VALUE : SPACE_VALUE;
	for (int i = 0; i < num_samples; ++i) {
		double t = static_cast<double>(i) / num_samples; // Normalized time (0 to 1)
		double envelope_value = start_value + (end_value - start_value) * 0.5 * (1 - cosf(3.14159265 * t)); // Raised cosine formula
		audio_data_queue_->push(envelope_value);
	}
	is_mark_ = target_mark; // Update current state to target state after transition
	return num_samples;
}

//! Add an overshoot disturbance to the specified audio sample queue
//! For now keep it simple, for each millisecond of overshoot duration,
//! add a brief overshoot of 10% above the target mark value or 10% below the target space value,
//! tapering off over the duration of the overshoot.
int shaper::add_overshoot(double duration, bool target_mark) {
	int num_samples = static_cast<int>(duration * sample_rate_);
	double start_value = is_mark_ ? MARK_VALUE : SPACE_VALUE;
	double end_value = target_mark ? MARK_VALUE : SPACE_VALUE;
	// No overshoot if the target state is the same as the current state
	if (start_value == end_value) {
		for (int i = 0; i < num_samples; ++i) {
			audio_data_queue_->push(end_value);
		}
		return num_samples;
	}
	double overshoot_value = target_mark ? MARK_VALUE + (duration * 0.1F) : SPACE_VALUE - (duration * 0.1F);
	for (int i = 0; i < num_samples; ++i) {
		double t = static_cast<double>(i) / num_samples; // Normalized time (0 to 1)
		double envelope_value = start_value + (overshoot_value - start_value) * (1 - t) + (end_value - overshoot_value) * t;
		audio_data_queue_->push(envelope_value);
	}
	is_mark_ = target_mark; // Update current state to target state after transition
	return num_samples;
}

//! Generate timing disturbance value
double shaper::generate_delta_t() {
	if (timing_disturbance_level_ == 0) {
		return 0.0F; // No disturbance
	}
	// Define the disturbance range based on the level
	double min_factor = 1.0F - 0.05F * timing_disturbance_level_; // Minimum factor (e.g. 0.98 for level 1)
	double max_factor = 1.0F + 0.10F * timing_disturbance_level_; // Maximum factor (e.g. 1.05 for level 1)
	// For simplicity, we will use a uniform distribution here.
	std::uniform_real_distribution<double> dist(min_factor, max_factor);
	return (dist(rng_) - 1.0F) * dot_time_; // Return the disturbance as a delta time based on the dot time
}
