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

#include "zc_audio_data.h"

#include <queue>
#include <thread>

extern int UPPER_QUEUE_THRESHOLD; //!< Threshold for when to generate more audio samples
extern int LOWER_QUEUE_THRESHOLD; //!< Threshold for when to stop generating audio samples

//! \brief Constructor for the modulator/mixer class
//! \param oscillator_queue Pointer to the queue containing oscillator samples
//! \param shaper_queue Pointer to the queue containing shaped envelope samples with metadata
//! \param noise_queue Pointer to the queue containing noise samples
//! \param output_queue Pointer to the queue where mixed audio samples will be pushed
mod_mixer::mod_mixer(std::queue<float>* oscillator_queue,
	std::queue<zc_audio_data>* shaper_queue,
	std::queue<float>* noise_queue,
	std::queue<zc_audio_data>* output_queue)
	: oscillator_queue_(oscillator_queue),
	shaper_queue_(shaper_queue),
	noise_queue_(noise_queue),
	output_queue_(output_queue)
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
void mod_mixer::modulation_loop(mod_mixer* mixer) {
	while (!mixer->stop_modulation_) {
		// Check if the output queue needs more samples and if we have input samples available
		if (mixer->output_queue_ &&
			mixer->output_queue_->size() < LOWER_QUEUE_THRESHOLD &&
			mixer->oscillator_queue_ && !mixer->oscillator_queue_->empty() &&
			mixer->shaper_queue_ && !mixer->shaper_queue_->empty()) {

			// Get the next envelope sample with metadata from the shaper
			zc_audio_data shaper_data = mixer->shaper_queue_->front();
			mixer->shaper_queue_->pop();
			// Create output audio data with metadata
			zc_audio_data output_data;
			output_data.metadata = shaper_data.metadata;

			// Process all samples in this shaper data block
			while (!shaper_data.data.empty() && mixer->output_queue_->size() < UPPER_QUEUE_THRESHOLD) {
				// Get envelope value from shaper
				float envelope = shaper_data.data.front();
				shaper_data.data.pop();

				// Get oscillator sample (when available)
				float oscillator_sample = 0.0F;
				if (mixer->oscillator_queue_) {
					if (mixer->oscillator_queue_->empty()) {
						// Yield until oscillator samples are available to avoid busy waiting
						while (mixer->oscillator_queue_->size() < LOWER_QUEUE_THRESHOLD) {
							std::this_thread::yield();
						}
					}
					oscillator_sample = mixer->oscillator_queue_->front();
					mixer->oscillator_queue_->pop();
				}

				// Get noise sample (if available)
				float noise_sample = 0.0F;
				if (mixer->noise_queue_) {
					if (mixer->noise_queue_->empty()) {
						// Yield until noise samples are available to avoid busy waiting
						while (mixer->noise_queue_->size() < LOWER_QUEUE_THRESHOLD) {
							std::this_thread::yield();
						}
					}
					noise_sample = mixer->noise_queue_->front();
					mixer->noise_queue_->pop();
				}

				// Modulate: multiply oscillator by envelope, then add noise
				float mixed_sample = (oscillator_sample * envelope) + noise_sample;

				output_data.data.push(mixed_sample);
			}

			// Push to output queue
			mixer->output_queue_->push(output_data);
		}
		else {
			// Yield to other threads when queues are full or inputs are empty
			std::this_thread::yield();
		}
	}
}

