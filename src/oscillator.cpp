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

#include "zc_settings.h"

// Local constant for pi (GCC-compatible)
namespace {
	constexpr float PI = 3.14159265358979323846f;
}
extern int UPPER_QUEUE_THRESHOLD; //!< Threshold for when the oscillator should generate more audio samples (in samples)
extern int LOWER_QUEUE_THRESHOLD; //!< Threshold for when the oscillator should stop generating audio samples (in samples)
extern int OSCILLATOR_CHUNK_SIZE; //!< Number of samples to generate in each chunk when generating oscillator samples

//! \brief Constructor for the oscillator class
//! \param output_queue Pointer to the queue where generated audio samples will be pushed
//! 
oscillator::oscillator(zc_async_queue<float>* output_queue) {
	apply_settings();
	output_queue_ = output_queue;
	// Start the generation thread
	generation_thread_ = std::thread(generation_loop, this);
}

//! \brief Destructor for the oscillator class
oscillator::~oscillator() {
	// Signal the generation thread to stop and wait for it to finish
	stop_generation_ = true;
	if (generation_thread_.joinable()) {
		generation_thread_.join();
	}
}

//! \brief Apply the current settings to the oscillator
void oscillator::apply_settings() {
	zc_settings settings;
	settings.get("Pitch (Hz)", base_pitch_, 700.0F);
	float volume_dB;
	settings.get("Volume (dB)", volume_dB, 0.0F);
	output_level_ = std::pow(10.0F, volume_dB / 20.0F);
	settings.get("Disturber Type", current_disturber_, disturber_type::NONE);
	settings.get("Drift Rate", drift_rate_, 0.0F);
	settings.get("Drift Amplitude", drift_amplitude_, 0.0F);
	settings.get("Drift Period", drift_period_, 1.0F);
	// Reset drift state when settings are applied
	current_drift_offset_ = 0.0F;
	drift_phase_accumulator_ = 0.0F;
}

//! \brief Generation loop for the oscillator thread
void oscillator::generation_loop(oscillator* osc) {
	// Set first phase accumulator value to 0 and set first output value to 0.
	osc->phase_accumulator_ = 0.0F;
	osc->output_queue_->push(0.0F);
	while (!osc->stop_generation_) {
		// If the output queue is nearly empty, generate more samples
		if (osc->output_queue_ && osc->output_queue_->size() < LOWER_QUEUE_THRESHOLD) {
			// Generate a block of samples up to the upper threshold.
			while (osc->output_queue_->size() < OSCILLATOR_CHUNK_SIZE) {
				float sample = osc->next_sample();
				if (osc->output_queue_) {
					osc->output_queue_->push(sample);
				}
			}
		}
		else {
			// Sleep briefly to avoid busy-waiting
			std::this_thread::yield();
		}
	}
}

//! \brief Generate the next audio sample based on the current settings
//! and update the phase accumulator
float oscillator::next_sample() {
	// Update the current frequency based on drift
	float frequency = update_drift_and_get_frequency();
	// Increment the phase accumulator by the current frequency 
	// divided by the sample rate
	phase_accumulator_ += 2.0F * PI * frequency / DEFAULT_SAMPLE_RATE;
	// Wrap the phase accumulator to stay within 0 to 2*pi
	if (phase_accumulator_ >= 2.0F * PI) {
		phase_accumulator_ -= 2.0F * PI;
	}
	// Return the sine of the phase accumulator as the output sample value
	return std::sin(phase_accumulator_) * output_level_;
}

//! \brief Update the current drift offset based on the selected 
//! disturber type and return the total frequency for the current sample
float oscillator::update_drift_and_get_frequency() {
	switch (current_disturber_) {
	case disturber_type::DRIFT_STEADY:
		current_drift_offset_ += drift_rate_ * sample_delta_time_;
		break;
	case disturber_type::DRIFT_CYCLIC:
		drift_phase_accumulator_ += 2.0F * PI * sample_delta_time_ / drift_period_;
		if (drift_phase_accumulator_ >= 2.0F * PI) {
			drift_phase_accumulator_ -= 2.0F * PI;
		}
		current_drift_offset_ = drift_amplitude_ * std::sin(drift_phase_accumulator_);
		break;
	default:
		current_drift_offset_ = 0.0F;
		break;
	}
	return base_pitch_ + current_drift_offset_;
}