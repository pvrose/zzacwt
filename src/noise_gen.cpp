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

extern float DEFAULT_SAMPLE_RATE; //!< Default audio sample rate
extern int UPPER_QUEUE_THRESHOLD; //!< Threshold for when the oscillator should generate more audio samples (in samples)
extern int LOWER_QUEUE_THRESHOLD; //!< Threshold for when the oscillator should stop generating audio samples (in samples)
extern int NOISE_CHUNK_SIZE; //!< Number of samples to generate in each chunk when generating noise

//! Constructor
noise_gen::noise_gen(zc_async_queue<float>* output_queue)
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
	if (generation_thread_.joinable()) {
		generation_thread_.join();
	}
}

//! Apply settings and update internal state
void noise_gen::apply_settings() {
	// Read settings and update internal state as they may have changed.
	zc_settings settings;
	settings.get("Disturber Type", current_disturber_, disturber_type::NONE);
	switch (current_disturber_) {
		case disturber_type::NOISE_WHITE:
			// Get volume setting for white noise
			settings.get("Noise Volume", noise_volume_, 0.0F);
			noise_severity_ = 0.0F;
			// No additional settings needed
			break;
		case disturber_type::NOISE_IMPACT:
		case disturber_type::NOISE_TONES:
			// Impact and Plink/plonk specific settings
			settings.get("Noise Volume", noise_volume_, 0.0F);
			settings.get("Noise Severity", noise_severity_, 0.0F);
			break;
		default:
			// No noise generation, so no settings needed
			noise_volume_ = 0.0F;
			noise_severity_ = 0.0F;
			return;
	}
	// Update the noise event distribution based on the severity setting:
	// The severity setting controls the frequency of noise events.
	// We can model this using an exponential distribution, where the 
	// mean time between events is inversely proportional to the severity.
	// Add a smidgeon to avoid division by zero when severity is zero.
	noise_event_dist_ = std::exponential_distribution<float>(1.0F / (noise_severity_ + 1e-6F));
	// Update the white noise distribution based on the volume setting
	// Convert dB to linear amplitude: amplitude = 10^(dB/20)
	float noise_amplitude = std::pow(10.0F, noise_volume_ / 20.0F);
	white_noise_dist_ = std::normal_distribution<float>(0.0F, noise_amplitude);
	// Reset the next noise event time
	next_noise_event_time_ = static_cast<int>(noise_event_dist_(rng_) * DEFAULT_SAMPLE_RATE);

}

//! Thread function that continuously generates noise samples 
//! according to the current settings and pushes them onto 
//! the audio data queue.
void noise_gen::generation_loop(noise_gen* instance) {
	while (!instance->stop_generation_) {
		if (instance->clear_requested_) {
			// Clear the output queue and reset internal state
			if (instance->audio_data_queue_) {
				instance->audio_data_queue_->clear();
			}
			instance->next_noise_event_time_ = static_cast<int>(instance->noise_event_dist_(instance->rng_) * DEFAULT_SAMPLE_RATE);
			instance->clear_requested_ = false;
		}
		// Check if we need to generate more noise samples
		if (instance->audio_data_queue_ && instance->audio_data_queue_->size() < LOWER_QUEUE_THRESHOLD) {
			std::vector<float> noise_samples;
			int samples_to_generate = NOISE_CHUNK_SIZE - instance->audio_data_queue_->size();
			// Generate noise samples based on the current disturber type
			switch (instance->current_disturber_) {
			case disturber_type::NOISE_WHITE:
				// Generate a block of white noise samples
				for (int i = 0; i < samples_to_generate; ++i) {
					noise_samples.push_back(instance->white_noise_dist_(instance->rng_));
				}
				break;
			case disturber_type::NOISE_IMPACT:
			case disturber_type::NOISE_TONES:
				if (instance->next_noise_event_time_ > samples_to_generate) {
					instance->next_noise_event_time_ -= samples_to_generate;
					// Generate no noise samples, just push zeros
					for (int i = 0; i < samples_to_generate; ++i) {
						noise_samples.push_back(0.0F);
					}
				}
				else {
					// Generate no noise samples until the next noise event time, then generate the noise event
					for (int i = 0; i < instance->next_noise_event_time_; ++i) {
						noise_samples.push_back(0.0F);
					}
					if (instance->current_disturber_ == disturber_type::NOISE_IMPACT) {
						// Time to generate a new impact noise event
						instance->generate_impact_noise(noise_samples);
					}
					else {
						// Time to generate a new plink/plonk noise event
						instance->generate_plink_plonk(noise_samples);
					}
					// Schedule the next noise event
					instance->next_noise_event_time_ = static_cast<int>(instance->noise_event_dist_(instance->rng_) * DEFAULT_SAMPLE_RATE);
				}
				break;
			default:
				// No noise generation, just push zeros
				for (int i = 0; i < samples_to_generate; ++i) {
					noise_samples.push_back(0.0F);
				}
				break;
			}
			// Push generated noise samples onto the output queue
			for (float sample : noise_samples) {
				if (instance->audio_data_queue_) {
					instance->audio_data_queue_->push(sample);
				}
			}
		}
		else {
			// Sleep briefly to avoid busy-waiting
			std::this_thread::yield();
		}
	}
}

//! Generate a single burst of impact noise. This will consist of a short burst of white noise.
void noise_gen::generate_impact_noise(std::vector<float>& noise) {
	// Generate a burst of white noise samples for the impact event
	int num_samples = static_cast<int>(0.1F * DEFAULT_SAMPLE_RATE); // 100ms burst
	for (int i = 0; i < num_samples; ++i) {
		noise.push_back(white_noise_dist_(rng_));
	}
}

//! Generate a single plink or plonk event. This will consist of a short burst of tone at a random frequency.
void noise_gen::generate_plink_plonk(std::vector<float>& noise) {
	// Generate a burst of tone samples for the plink/plonk event
	int num_samples = static_cast<int>(0.1F * DEFAULT_SAMPLE_RATE); // 100ms burst
	float frequency = 300.0F + static_cast<float>(rng_() % 2700); // Random frequency between 300 Hz and 3000 Hz

	// Use a phase accumulator that wraps to stay within [0, 2π)
	constexpr float TWO_PI = 2.0F * 3.14159265358979323846F;
	float phase = 0.0F;
	float phase_increment = TWO_PI * frequency / DEFAULT_SAMPLE_RATE;

	// Convert dB to linear amplitude for plink/plonk tones
	float noise_amplitude = std::pow(10.0F, noise_volume_ / 20.0F);
	for (int i = 0; i < num_samples; ++i) {
		float sample = std::sin(phase);
		noise.push_back(sample * noise_amplitude);

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
}
