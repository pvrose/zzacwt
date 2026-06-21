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
#include "noise_gen.hpp"

#include "params.hpp"

#include "zc_async_queue.h"
#include "zc_settings.h"

#include <chrono>
#include <cmath>
#include <queue>
#include <random>
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

extern double DEFAULT_SAMPLE_RATE; //!< Default audio sample rate
extern int NOISE_CHUNK_SIZE; //!< Number of samples to generate in each chunk when generating noise
extern int LOWER_CHUNK_SIZE;

//! Constructor
noise_gen::noise_gen(zc_async_queue<double>* output_queue)
	: audio_data_queue_(output_queue),
	rng_(std::random_device{}())
{
	// Initialise settings
	apply_settings();
	// Start the generation thread
	generation_thread_ = std::thread(generation_loop, this);
}

//! Destructor
noise_gen::~noise_gen() {
	// Signal the generation thread to stop and wait for it to finish
	stop_generation_ = true;
	wake(); // Wake up the thread so it can exit
	if (generation_thread_.joinable()) {
		generation_thread_.join();
	}
}

//! Wake up the generation thread to produce more samples
void noise_gen::wake() {
	std::lock_guard<std::mutex> lock(wake_mutex_);
	wake_condition_.notify_one();
}

//! Apply settings and update internal state
void noise_gen::apply_settings() {
	// Read settings and update internal state as they may have changed.
	zc_settings settings;
	settings.get("Disturber Type", current_disturber_, disturber_type::NONE);
	switch (current_disturber_) {
		case disturber_type::NOISE_WHITE:
			// Get volume setting for white noise
			settings.get("Noise Volume", noise_volume_, 0.0);
			noise_severity_ = 0.0F;
			// No additional settings needed
			break;
		case disturber_type::NOISE_IMPACT:
			settings.get("Noise Volume", noise_volume_, 0.0);
			settings.get("Noise Severity", noise_severity_, 0.0);
			break;
		case disturber_type::NOISE_TONES:
			// Impact and Plink/plonk specific settings
			settings.get("Noise Volume", noise_volume_, 0.0);
			settings.get("Noise Severity", noise_severity_, 0.0);
			break;
		default:
			// No noise generation, so no settings needed
			noise_volume_ = 0.0;
			noise_severity_ = 0.0;
			return;
	}
	// Get sample rate
	settings.get("Sample Rate", sample_rate_, DEFAULT_SAMPLE_RATE);
	// Update the noise event distribution based on the severity setting:
	// The severity setting controls the frequency of noise events.
	// We can model this using an exponential distribution, where the 
	// mean time between events is inversely proportional to the severity.
	// Add a smidgeon to avoid division by zero when severity is zero.
	noise_event_dist_ = std::exponential_distribution<double>(noise_severity_ / 10.0 + 1e-6);
	// Update the white noise distribution based on the volume setting
	// Convert dB to linear amplitude: amplitude = 10^(dB/20)
	double noise_amplitude = std::pow(10.0, noise_volume_ / 20.0);
	white_noise_dist_ = std::normal_distribution<double>(0.0F, noise_amplitude);
	// Reset the next noise event time
	next_noise_event_time_ = static_cast<int>(noise_event_dist_(rng_) * sample_rate_);

}

//! Thread function that continuously generates noise samples 
//! according to the current settings and pushes them onto 
//! the audio data queue.
void noise_gen::generation_loop(noise_gen* instance) {
#ifdef ENABLE_QUEUE_MONITORING
	fprintf(stderr, "[THREAD] Noise_gen thread started, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	try {
#endif
		while (!instance->stop_generation_) {
		if (instance->clear_requested_) {
			// Clear the output queue and reset internal state
			if (instance->audio_data_queue_) {
				instance->audio_data_queue_->clear();
			}
			instance->next_noise_event_time_ = static_cast<int>(instance->noise_event_dist_(instance->rng_) * instance->sample_rate_);
			instance->clear_requested_ = false;
		}
		// Check if we need to generate more noise samples
		if (instance->audio_data_queue_ && instance->audio_data_queue_->size() < LOWER_CHUNK_SIZE) {
			int samples_to_generate = NOISE_CHUNK_SIZE - instance->audio_data_queue_->size();
			// Generate noise samples based on the current disturber type
			switch (instance->current_disturber_) {
			case disturber_type::NOISE_WHITE:
				// Generate a block of white noise samples
				for (int i = 0; i < samples_to_generate; ++i) {
					instance->audio_data_queue_->push(instance->white_noise_dist_(instance->rng_));
				}
				break;
			case disturber_type::NOISE_IMPACT:
			case disturber_type::NOISE_TONES:
				if (instance->next_noise_event_time_ > samples_to_generate) {
					instance->next_noise_event_time_ -= samples_to_generate;
					// Generate no noise samples, just push zeros
					for (int i = 0; i < samples_to_generate; ++i) {
						instance->audio_data_queue_->push(0.0);
					}
				}
				else {
					// Generate no noise samples until the next noise event time, then generate the noise event
					for (int i = 0; i < instance->next_noise_event_time_; ++i) {
						instance->audio_data_queue_->push(0.0);
					}
					if (instance->current_disturber_ == disturber_type::NOISE_IMPACT) {
						// Time to generate a new impact noise event
						instance->generate_impact_noise();
					}
					else {
						// Time to generate a new plink/plonk noise event
						instance->generate_plink_plonk();
					}
					// Schedule the next noise event
					instance->next_noise_event_time_ = static_cast<int>(instance->noise_event_dist_(instance->rng_) * instance->sample_rate_);
				}
				break;
			default:
				// No noise generation, just push zeros
				for (int i = 0; i < samples_to_generate; ++i) {
					instance->audio_data_queue_->push(0.0);
				}
				break;
			}
		}
		else {
			// Wait for wake-up signal instead of busy-waiting
			std::unique_lock<std::mutex> lock(instance->wake_mutex_);
			instance->wake_condition_.wait(lock);
		}
		}
#ifdef ENABLE_QUEUE_MONITORING
		fprintf(stderr, "[THREAD] Noise_gen thread exiting normally, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[THREAD] Noise_gen thread exception, ID: %zu, error: %s\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), e.what());
	}
	catch (...) {
		fprintf(stderr, "[THREAD] Noise_gen thread unknown exception, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
#endif
}

//! Generate a single burst of impact noise. This will consist of a short burst of white noise.
void noise_gen::generate_impact_noise() {
	// Generate a burst of white noise samples for the impact event
	int num_samples = static_cast<int>(0.1 * sample_rate_); // 100ms burst
	// Generate white noise starting at amplitude 1.0 and decaying 
	// linerarly over the duration of the burst
	for (int i = 0; i < num_samples; ++i) {
		double decay_factor = 1.0F - static_cast<double>(i) / num_samples; // Linear decay over the burst duration
		double sample = white_noise_dist_(rng_) * decay_factor; // Apply decay to the white noise sample
		audio_data_queue_->push(sample);
	}
}

//! Generate a single plink or plonk event. This will consist of a short burst of tone at a random frequency.
void noise_gen::generate_plink_plonk() {
	// Generate a burst of tone samples for the plink/plonk event
	int num_samples = static_cast<int>(0.1 * sample_rate_); // 100ms burst
	double frequency = 300.0 + static_cast<double>(rng_() % 2700); // Random frequency between 300 Hz and 3000 Hz

	// Use a phase accumulator that wraps to stay within [0, 2π)
	constexpr double TWO_PI = 2.0 * 3.14159265358979323846;
	double phase = 0.0;
	double phase_increment = TWO_PI * frequency / sample_rate_;

	// Convert dB to linear amplitude for plink/plonk tones
	double noise_amplitude = std::pow(10.0, noise_volume_ / 20.0);
	for (int i = 0; i < num_samples; ++i) {
		double sample = std::sin(phase);
		audio_data_queue_->push(sample * noise_amplitude);

		// Increment phase and wrap to keep it within [0, 2π)
		phase += phase_increment;
		if (phase >= TWO_PI) {
			phase -= TWO_PI;
		}
	}
}

//! Clear the generation of audio samples. This will clear the output queue and reset the internal state of the noise generator to be ready for a new sequence of symbols.
void noise_gen::clear() {
	clear_requested_ = true; // Signal the generation loop to clear the queue and reset state
	wake(); // Wake up the thread to process the clear request
}
