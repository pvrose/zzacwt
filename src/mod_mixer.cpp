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

#include "mod_mixer.hpp"

#include "noise_gen.hpp"
#include "oscillator.hpp"
#include "shaper.hpp"
#include "zc_async_queue.h"

#include <queue>
#include <stdexcept>
#include <thread>

// Enable queue monitoring in debug builds
#ifdef _DEBUG
#define ENABLE_QUEUE_MONITORING
#endif

#ifdef ENABLE_QUEUE_MONITORING
#include <chrono>
#include <iostream>
#endif

extern int OUTPUT_CHUNK_SIZE; //!< Threshold for when to generate more audio samples
extern int LOWER_CHUNK_SIZE; //!< Threshold for when to stop generating audio samples

//! \brief Constructor for the modulator/mixer class
//! \param oscillator_queue Pointer to the queue containing oscillator samples
//! \param shaper_queue Pointer to the queue containing shaped envelope samples
//! \param noise_queue Pointer to the queue containing noise samples
//! \param output_queue Pointer to the queue where mixed audio samples will be pushed
//! \param oscillator_ptr Pointer to the oscillator object for waking it up
//! \param shaper_ptr Pointer to the shaper object for waking it up
//! \param noise_gen_ptr Pointer to the noise_gen object for waking it up
mod_mixer::mod_mixer(zc_async_queue<double>* oscillator_queue,
	zc_async_queue<double>* shaper_queue,
	zc_async_queue<double>* noise_queue,
	zc_async_queue<double>* output_queue,
	oscillator* oscillator_ptr,
	shaper* shaper_ptr,
	noise_gen* noise_gen_ptr)
	: oscillator_queue_(oscillator_queue),
	shaper_queue_(shaper_queue),
	noise_queue_(noise_queue),
	output_queue_(output_queue),
	oscillator_(oscillator_ptr),
	shaper_(shaper_ptr),
	noise_gen_(noise_gen_ptr)
{
	// Start the modulation/mixing thread
	modulation_thread_ = std::thread(modulation_loop, this);
}

//! \brief Destructor for the modulator/mixer class
mod_mixer::~mod_mixer() {
	// Signal the modulation thread to stop and wait for it to finish
	stop_modulation_ = true;
	if (modulation_thread_.joinable()) {
		modulation_thread_.join();
	}
}

//! \brief Modulation/mixing loop for the modulation thread
//! This function runs in a separate thread and combines samples from the oscillator,
//! shaper, and noise generator to produce the final audio output.
void mod_mixer::modulation_loop(mod_mixer* that) {
#ifdef ENABLE_QUEUE_MONITORING
	fprintf(stderr, "[THREAD] Mod_mixer thread started, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	try {
	// Queue monitoring variables
	size_t osc_min = SIZE_MAX, osc_max = 0;
	size_t shaper_min = SIZE_MAX, shaper_max = 0;
	size_t noise_min = SIZE_MAX, noise_max = 0;
	size_t output_min = SIZE_MAX, output_max = 0;
	int underrun_count_osc = 0, underrun_count_shaper = 0, underrun_count_noise = 0, underrun_count_output = 0;
	auto last_report = std::chrono::steady_clock::now();
#endif

	while (!that->stop_modulation_) {
		if (that->clear_requested_) {
			// Clear all queues and reset internal state
			if (that->output_queue_) that->output_queue_->clear();
			that->clear_requested_ = false;
		}

#ifdef ENABLE_QUEUE_MONITORING
		// Track queue sizes for diagnostics
		size_t osc_size = that->oscillator_queue_ ? that->oscillator_queue_->size() : 0;
		size_t shaper_size = that->shaper_queue_ ? that->shaper_queue_->size() : 0;
		size_t noise_size = that->noise_queue_ ? that->noise_queue_->size() : 0;
		size_t output_size = that->output_queue_ ? that->output_queue_->size() : 0;

		osc_min = std::min(osc_min, osc_size);
		osc_max = std::max(osc_max, osc_size);
		shaper_min = std::min(shaper_min, shaper_size);
		shaper_max = std::max(shaper_max, shaper_size);
		noise_min = std::min(noise_min, noise_size);
		noise_max = std::max(noise_max, noise_size);
		output_min = std::min(output_min, output_size);
		output_max = std::max(output_max, output_size);

		// Detect underruns (queue empty when we need samples)
		if (osc_size == 0) underrun_count_osc++;
		if (shaper_size == 0) underrun_count_shaper++;
		if (noise_size == 0) underrun_count_noise++;
		if (output_size == 0) underrun_count_output++;

		// Report statistics every 5 seconds
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - last_report).count() >= 5) {
			std::cout << "\n=== Queue Statistics (5 sec) ===" << std::endl;
			std::cout << "Oscillator:  min=" << osc_min << " max=" << osc_max << " underruns=" << underrun_count_osc << std::endl;
			std::cout << "Shaper:      min=" << shaper_min << " max=" << shaper_max << " underruns=" << underrun_count_shaper << std::endl;
			std::cout << "Noise:       min=" << noise_min << " max=" << noise_max << " underruns=" << underrun_count_noise << std::endl;
			std::cout << "Output:      min=" << output_min << " max=" << output_max << " underruns=" << underrun_count_output << std::endl;
			std::cout << "LOWER_CHUNK_SIZE=" << LOWER_CHUNK_SIZE << std::endl;

			// Reset for next period
			osc_min = shaper_min = noise_min = output_min = SIZE_MAX;
			osc_max = shaper_max = noise_max = output_max = 0;
			underrun_count_osc = underrun_count_shaper = underrun_count_noise = underrun_count_output = 0;
			last_report = now;
		}
#endif

		// Wake up producer threads if their queues are low
		if (that->oscillator_queue_ && that->oscillator_queue_->size() < LOWER_CHUNK_SIZE) {
			if (that->oscillator_) that->oscillator_->wake();
		}
		if (that->shaper_queue_ && that->shaper_queue_->size() < LOWER_CHUNK_SIZE) {
			if (that->shaper_) that->shaper_->wake();
		}
		if (that->noise_queue_ && that->noise_queue_->size() < LOWER_CHUNK_SIZE) {
			if (that->noise_gen_) that->noise_gen_->wake();
		}

		// Check if the output queue needs more samples and if we have input samples available
		if (that->output_queue_ &&
			that->output_queue_->size() < LOWER_CHUNK_SIZE &&
			that->oscillator_queue_ && !(that->oscillator_queue_->size() < LOWER_CHUNK_SIZE) &&
			that->shaper_queue_ && !(that->shaper_queue_->size() < LOWER_CHUNK_SIZE) &&
			that->noise_queue_ && !(that->noise_queue_->size() < LOWER_CHUNK_SIZE)) {

			// Process all samples in this shaper data block
			while (!that->shaper_queue_->empty() && 
				!that->oscillator_queue_->empty() &&
				!that->noise_queue_->empty() &&
				that->output_queue_->size() < OUTPUT_CHUNK_SIZE) {
				// Get envelope value from shaper
				double envelope = 0.0F;
				if (that->shaper_queue_) {
					if (that->shaper_queue_->size() < LOWER_CHUNK_SIZE) {
						// Wake up the shaper to generate more samples
						if (that->shaper_) {
							that->shaper_->wake();
						}
					}
					if (!that->shaper_queue_->wait_and_pop(envelope)) {
						// Queue is shutting down, exit loop
						break;
					}
				}

				// Get oscillator sample (when available)
				double oscillator_sample = 0.0F;
				if (that->oscillator_queue_) {
					if (that->oscillator_queue_->size() < LOWER_CHUNK_SIZE) {
						// Wake up the oscillator to generate more samples
						if (that->oscillator_) {
							that->oscillator_->wake();
						}
					}
					if (!that->oscillator_queue_->wait_and_pop(oscillator_sample)) {
						// Queue is shutting down, exit loop
						break;
					}
				}

				// Get noise sample (if available)
				double noise_sample = 0.0;
				if (that->noise_queue_) {
					if (that->noise_queue_->size() < LOWER_CHUNK_SIZE) {
						// Wake up the noise generator to generate more samples
						if (that->noise_gen_) {
							that->noise_gen_->wake();
						}
					}
					if (!that->noise_queue_->wait_and_pop(noise_sample)) {
						// Queue is shutting down, exit loop
						break;
					}
				}

				// Modulate: multiply oscillator by envelope, then "or" noise
				double modulation = oscillator_sample * envelope;
				double mixed_sample;
				if (modulation > 0.0) {
					if (noise_sample > 0.0) {
						mixed_sample = std::max(modulation, noise_sample);
					}
					else {
						mixed_sample = modulation + noise_sample; // Add noise to negative modulation
					}
				} else {
					if (noise_sample < 0.0) {
						mixed_sample = std::min(modulation, noise_sample);
					}
					else {
						mixed_sample = modulation + noise_sample; // Add noise to positive modulation
					}
				}
				that->output_queue_->push(mixed_sample);
			}

		}
		else {
			// Yield to other threads when queues are full or inputs are empty
			std::this_thread::yield();
		}
	}
#ifdef ENABLE_QUEUE_MONITORING
	fprintf(stderr, "[THREAD] Mod_mixer thread exiting normally, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[THREAD] Mod_mixer thread exception, ID: %zu, error: %s\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), e.what());
	}
	catch (...) {
		fprintf(stderr, "[THREAD] Mod_mixer thread unknown exception, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
#endif
}

//! \brief Clear all queues and reset the internal state of the modulator/mixer
void mod_mixer::clear() {
	clear_requested_ = true;
}

