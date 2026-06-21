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

// Enable queue monitoring in debug builds
#ifdef _DEBUG
#define ENABLE_QUEUE_MONITORING
#endif

#ifdef ENABLE_QUEUE_MONITORING
#include <chrono>
#include <iostream>
#endif

// Local constant for pi (GCC-compatible)
namespace {
	constexpr double PI = 3.14159265358979323846;
}
extern int LOWER_CHUNK_SIZE; //!< Threshold for when the oscillator should stop generating audio samples (in samples)
extern int OSCILLATOR_CHUNK_SIZE; //!< Number of samples to generate in each chunk when generating oscillator samples

//! \brief Constructor for the oscillator class
//! \param output_queue Pointer to the queue where generated audio samples will be pushed
//! 
oscillator::oscillator(zc_async_queue<double>* output_queue) {
	apply_settings();
	output_queue_ = output_queue;
	// Start the generation thread
	generation_thread_ = std::thread(generation_loop, this);
}

//! \brief Destructor for the oscillator class
oscillator::~oscillator() {
	// Signal the generation thread to stop and wait for it to finish
	stop_generation_ = true;
	wake(); // Wake up the thread so it can exit
	if (generation_thread_.joinable()) {
		generation_thread_.join();
	}
}

//! \brief Wake up the generation thread to produce more samples
void oscillator::wake() {
	std::lock_guard<std::mutex> lock(wake_mutex_);
	wake_condition_.notify_one();
}

//! \brief Apply the current settings to the oscillator
void oscillator::apply_settings() {
	zc_settings settings;
	settings.get("Pitch (Hz)", base_pitch_, 700.0);
	double volume_dB;
	settings.get("Volume (dB)", volume_dB, 0.0);
	output_level_ = std::pow(10.0F, volume_dB / 20.0);
	settings.get("Disturber Type", current_disturber_, disturber_type::NONE);
	settings.get("Drift Rate", drift_rate_, 0.0);
	settings.get("Drift Amplitude", drift_amplitude_, 0.0);
	settings.get("Drift Period", drift_period_, 1.0);
	settings.get("Fading Period", fading_period, 0.0);
	settings.get("Fading Depth", fading_amplitude_, 0.0);
	settings.get("Sample Rate", sample_rate_, DEFAULT_SAMPLE_RATE);
	sample_delta_time_ = 1.0 / sample_rate_;
	// Reset drift state when settings are applied
	current_drift_offset_ = 0.0;
	drift_phase_accumulator_ = 0.0;
	fading_phase_accumulator_ = 0.0;
}

//! \brief Generation loop for the oscillator thread
void oscillator::generation_loop(oscillator* osc) {
#ifdef ENABLE_QUEUE_MONITORING
	fprintf(stderr, "[THREAD] Oscillator thread started, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	try {
#endif
		// Set first phase accumulator value to 0 and set first output value to 0.
		osc->phase_accumulator_ = 0.0;
		osc->output_queue_->push(0.0);
		while (!osc->stop_generation_) {
		// If the output queue is nearly empty, generate more samples
		if (osc->output_queue_ && osc->output_queue_->size() < LOWER_CHUNK_SIZE) {
			// Generate a block of samples up to the upper threshold.
			while (osc->output_queue_->size() < OSCILLATOR_CHUNK_SIZE) {
				double sample = osc->next_sample();
				if (osc->output_queue_) {
					osc->output_queue_->push(sample);
				}
			}
		}
		else {
			// Wait for wake-up signal instead of busy-waiting
			std::unique_lock<std::mutex> lock(osc->wake_mutex_);
			osc->wake_condition_.wait(lock);
		}
		}
#ifdef ENABLE_QUEUE_MONITORING
		fprintf(stderr, "[THREAD] Oscillator thread exiting normally, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[THREAD] Oscillator thread exception, ID: %zu, error: %s\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), e.what());
	}
	catch (...) {
		fprintf(stderr, "[THREAD] Oscillator thread unknown exception, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
#endif
}

//! \brief Generate the next audio sample based on the current settings
//! and update the phase accumulator
double oscillator::next_sample() {
	// Update the current frequency based on drift
	double frequency = update_drift_and_get_frequency();
	// Increment the phase accumulator by the current frequency 
	// divided by the sample rate
	phase_accumulator_ += 2.0 * PI * frequency * sample_delta_time_;
	// Wrap the phase accumulator to stay within 0 to 2*pi
	if (phase_accumulator_ >= 2.0 * PI) {
		phase_accumulator_ -= 2.0 * PI;
	}
	// Apply fading multiplier to the output sample
	double fading_multiplier = update_fading_and_get_multiplier();
	// Return the sine of the phase accumulator as the output sample value
	return std::sin(phase_accumulator_) * output_level_ * fading_multiplier;
}

//! \brief Update the current drift offset based on the selected 
//! disturber type and return the total frequency for the current sample
double oscillator::update_drift_and_get_frequency() {
	switch (current_disturber_) {
	case disturber_type::DRIFT_STEADY:
		// drift rate is in % current frequency (pitch + offset) per second.
		current_drift_offset_ += (drift_rate_ * 0.01 * (base_pitch_ + current_drift_offset_) * sample_delta_time_);
		break;
	case disturber_type::DRIFT_CYCLIC:
		drift_phase_accumulator_ += 2.0 * PI * sample_delta_time_ / drift_period_;
		if (drift_phase_accumulator_ >= 2.0 * PI) {
			drift_phase_accumulator_ -= 2.0 * PI;
		}
		current_drift_offset_ = drift_amplitude_ * std::sin(drift_phase_accumulator_);
		break;
	default:
		current_drift_offset_ = 0.0;
		break;
	}
	return base_pitch_ + current_drift_offset_;
}

//! \brief Update the current fading level based on the fading settings and return the current fading multiplier (0 to 1) to apply to the output sample.
double oscillator::update_fading_and_get_multiplier() {
	if (current_disturber_ != disturber_type::FADING) {
		return 1.0; // No fading, so multiplier is 1
	}
	fading_phase_accumulator_ += 2.0 * PI * sample_delta_time_ / fading_period;
	if (fading_phase_accumulator_ >= 2.0 * PI) {
		fading_phase_accumulator_ -= 2.0 * PI;
	}
	return 1.0 - (fading_amplitude_ * (0.5 * (1 - std::cos(fading_phase_accumulator_)))); // Fading multiplier based on a cosine wave
}