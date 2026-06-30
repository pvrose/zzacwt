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
#include "zc_active_queue.h"

#include <queue>
#include <stdexcept>
#include <thread>

// Enable queue monitoring in debug builds
//#ifdef _DEBUG
//#define ENABLE_QUEUE_MONITORING
//#endif

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
mod_mixer::mod_mixer(zc_active_queue<double>* oscillator_queue,
	zc_active_queue<double>* shaper_queue,
	zc_active_queue<double>* noise_queue,
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
	while (!that->stop_modulation_) {
		if (that->clear_requested_) {
			// Clear all queues and reset internal state
			if (that->output_queue_) that->output_queue_->clear();
			that->clear_requested_ = false;
		}

		// Check if the output queue needs more samples and if we have input samples available
		if (that->output_queue_ &&
			that->output_queue_->size() < LOWER_CHUNK_SIZE) {

			// Process all samples in this shaper data block
			// Note that input queues will wait until they have enough samples to fill the output queue
			while (that->output_queue_->size() < OUTPUT_CHUNK_SIZE) {
				// Get envelope value from shaper
				double envelope = that->shaper_queue_->pop();
				// Get oscillator sample (when available)
				double oscillator_sample = that->oscillator_queue_->pop();
				// Get noise sample (if available)
				double noise_sample = that->noise_queue_->pop();
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
}

//! \brief Clear all queues and reset the internal state of the modulator/mixer
void mod_mixer::clear() {
	clear_requested_ = true;
}

