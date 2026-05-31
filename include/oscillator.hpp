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

#pragma once

#include "params.hpp"

#include "zc_async_queue.h"

#include <queue>
#include <thread>

//! \file oscillator.hpp
//! 

extern float DEFAULT_SAMPLE_RATE;  //!< Default audio sample rate

//! \brief Class for the oscillator component of ZZACWT, 
//! responsible for generating the audio signal based on user 
//! settings and applying various distortions to the frequency.
//! 
//! \brief This class outputs data into a queue which represents
//! a constant volume tone at the current pitch. 
//! 
//! The pitch
//! is controlled by settings which are updated by the user interface.
//! There is a base pitch which can then be subject to either
//! a steady frequency drift or a cyclic frequency drift.
//! 
//! The oscillator will generate samples almost on demand,
//! maintaining an output queue of audio data which is consumed 
//! by the modulator. When the queue is nearly empty, the oscillator
//! will generate more audio data based on the current settings and
//! push it onto the queue. 
//! 
//! For each sample, the oscillator will maintain a phase accumulator 
//! which is incremented by the current frequency (base pitch plus any drift) 
//! divided by the sample rate. The output sample value is then determined
//! by the sine of the phase accumulator. The phase accumulator will be 
//! wrapped around to stay within the range of 0 to 2*pi.
//! 
//! Drift is defined in the settings as of two types: steady and cyclic.
//! Steady drift will see the frequency increase or decrease at a constant
//! rate (in Hz per second). 
//! Cyclic drift will see the frequency vary sinusoidally around the base pitch.
//! This drift is defined by two settings: the drift rate (in Hz) which 
//! determines the amplitude of the frequency variation, and the drift period (in seconds) 
//! which determines how quickly the frequency oscillates.
//! 

class oscillator {

public:
	//! Constructor
	//! \param output_queue Pointer to the queue where the oscillator will push generated audio samples
	oscillator(zc_async_queue<float>* output_queue);

	//! Destructor
	~oscillator();

	//! Apply the current settings to the oscillator. 
	void apply_settings();

private:

	//! Generate next sample based on current settings. Update the phase accumulator and apply any frequency drift as needed.
	float next_sample();

	//! Pointer to the output queue.
	zc_async_queue<float>* output_queue_ = nullptr;

	//! Current phase accumulator for the oscillator (in radians).
	float phase_accumulator_ = 0.0F;

	//! Oscillator base pitch (in Hz).
	float base_pitch_ = 700.0F;

	//! Output level for the oscillator. This is a constant value that represents the amplitude of the output signal. The actual audio sample value will be this level multiplied by the sine of the phase accumulator.
	float output_level_ = 1.0F;

	//! Drift parameters - drift type.
	disturber_type current_disturber_ = disturber_type::NONE;
	//! Drift rate for steady drift (in Hz per second).
	float drift_rate_ = 0.0F;
	//! Drift amplitude for cyclic drift (in Hz).
	float drift_amplitude_ = 0.0F;
	//! Drift period for cyclic drift (in seconds).
	float drift_period_ = 0.0F;
	//! Current drift phase accumulator for cyclic drift (in radians).
	float drift_phase_accumulator_ = 0.0F;
	//! Current drift frequency offset (in Hz).
	float current_drift_offset_ = 0.0F;
	//! Sample delta time (in seconds) for calculating drift changes.
	const float sample_delta_time_ = 1.0F / DEFAULT_SAMPLE_RATE;
	//! Update the current drift offset and return the total frequency 
	//! (base pitch plus drift) for the current sample.
	float update_drift_and_get_frequency();

	//! Fading parameters - period  in seconds.
	float fading_period = 0.0F; 
	//! Current fading phase accumulator (in radians).
	float fading_phase_accumulator_ = 0.0F;
	//! fading amplitude (0 to 1).
	float fading_amplitude_ = 0.0F;
	//! Update the current fading level based on the fading settings and return the current fading multiplier (0 to 1) to apply to the output sample.
	float update_fading_and_get_multiplier();

	//! Thread for generating audio samples.
	std::thread generation_thread_;
	//! Flag to signal the generation thread to stop.
	bool stop_generation_ = false;
	//! Method for the generation thread to continuously generate audio samples and push them onto the output queue.
	static void generation_loop(oscillator* osc);

};
