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

//! \file monitor.hpp
//! 
#include "zc_async_queue.h"
#include "zc_graph_.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include <fftw3.h>

//! \brief Class to monitor the generated audio samples and recover the transmitted symbols.
//! 
//! The received audio samples will be received using a callback in the audio output module.
//! They will be collected into a queue buffer and processed to get images in the 
//! frequency domain that are regulalrly sampled in time.
//! 
//! These images will be processed to detect the presence of the transmitted tone and the 
//! mark and space symbols will be recovered. These can then be analysed to determine
//! the average dot speed and convert the symbols back into text for display to the user.
//! 
//! Analysis will also provide an indication of the actual dot speeds etc.
//! 
//! For simplicity, the time period at which the images are sampled will be an integer multiple
//! (N) of the audio sample rate (22050 Hz) and will be specified as a parameter in settings.
//! The number of audio samples (M) in each image will be at least this number and will also 
//! be specified as a parameter in settings.
//! To achieve the required frequency resolution, the audio samples will be padded with zeros 
//! as necessary to reach the required number of samples (F) for the FFT.
//! 
//! The audio samples will be collected into a queue. As soon as there are at least M
//! samples in the queue, the first M samples will be processed to get the first image. 
//! The first N samples in the queue will be discarded. This process will be repeated
//! continually to get a stream of images in the frequency domain.
//! 
//! The queue will be implemented as an indexed map of samples. The head index is written
//! in the audio thread and the tail index by the processing thread.
//! 
//! While developing the monitor, the frequency domain images will be plotted against 
//! time to provide a visual representation of the received signal and to help with the 
//! development of the symbol detection algorithm.
//! 
class monitor
{
public:
	//! Constructor.
	monitor();
	//! Destructor.
	~monitor();

	//! Add a new audio sample to the monitor. This will be called by the audio output module.
	void add_audio_sample(float sample);

	//! Set display buffer for plotting the frequency domain images. 
	//! Add a callback function to update the display when new images are available.
	void set_display_buffer(zc_graph_::data_set_dens_t* buffer, std::function<void(void*)> callback, void* user_data);

	//! Get the pitch of the selected bin in Hz. This is the frequency of the transmitted signal that is being monitored.
	float get_selected_bin_pitch() const {
		if (selected_signal_bin_ >= 0) {
			return get_bin_frequency(selected_signal_bin_);
		}
		else {
			return 0.0f;
		}
	}

	//! Reset monitor after changing parameters. 
	//! This will reset the FFT buffers and plan. Stop and restart the processing thread to apply the new parameters.
	void reset_parameters();

private:

	//! Start the processing thread to process the audio samples and recover the symbols.
	void start_processing_thread();

	//! Stop the processing thread and wait for it to finish.
	void stop_processing_thread();

	//! If necessary, reset the FFT buffers and plan.
	void reset_fft_buffers_and_plan();

	//! Create the FFT buffers and plan based on the current parameters.
	void create_fft_buffers_and_plan();

	//! Processing thread function to continuously process the audio samples and recover the symbols until signalled to stop.
	static void processing_thread_function(monitor* self);

	//! Load the parameters from the settings and update the internal variables accordingly.
	void load_parameters();

	//! Store the parameters from the settings.
	void store_parameters() const;

	//! Process one image of audio samples to get the frequency domain representation.
	void process_audio_samples();

	//! Add the frequency domain image to the display buffer and call the display callback to update the display.
	void update_display_buffer();

	//! Identify the candidate frequency bin for the transmitted signal and add
	//! it to the selected_signal_ queue for use by the symbol detection algorithm.
	void identify_signal_bin();

	//! Return the frequency of the \p bin in Hz based on the FFT size and sample rate.
	float get_bin_frequency(int bin) const;

	//! Thread for processing the audio samples and recovering the symbols.
	std::thread* processing_thread_;
	//! Flag to signal the processing thread to stop.
	std::atomic<bool> stop_processing_ = false;

	//! The queue of audio samples that are currently being collected.
	std::deque<double> audio_queue_;
	//! Mutex to protect audio_queue_ from concurrent access.
	std::mutex audio_queue_mutex_;

	//! FFT input buffer.
	fftw_complex* fft_input_buffer_ = nullptr;
	//! FFT output buffer.
	fftw_complex* fft_output_buffer_ = nullptr;
	//! FFT plan.
	fftw_plan fft_plan_ = nullptr;

	//! Display buffer for plotting the frequency domain images.
	//! Z is the magnitude of the frequency component at that time and frequency.
	zc_graph_::data_set_dens_t* display_buffer_ = nullptr;
	//! Callback function to update the display when new images are available.
	std::function<void(void*)> display_callback_;
	//! User data to pass to the display callback function.
	void* display_user_data_ = nullptr;

	//! The queue of frequency images that have been processed and are waiting to be displayed.
	std::deque<std::vector<float>> image_queue_;

	//! Selected frequency bin for extracting signal
	zc_async_queue<float> selected_signal_;

	//! Selected frequency bin number for extracting signal
	int selected_signal_bin_ = -1;

	//! Parameters for the monitor.
	unsigned int fft_size_ = 1024; // Number of samples for the FFT (F).
	unsigned int image_interval_ = 100; // Time interval between images in samples (N).
	unsigned int samples_per_image_ = 256; // Minimum number of samples per image (M).
	unsigned int display_depth_ = 100; // Number of images to display in the frequency domain plot.



};