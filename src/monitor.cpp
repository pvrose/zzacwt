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
#include "monitor.hpp"

#include "zc_graph_.h"
#include "zc_settings.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <thread>
#include <vector>

#include <fftw3.h>

// External sample rate variable defined in main.cpp. 
extern float DEFAULT_SAMPLE_RATE;

monitor::monitor()
{
	// Load the parameters from the settings and update the internal variables accordingly.
	load_parameters();
	// Allocate the FFT input and output buffers and create the FFT plan.
	create_fft_buffers_and_plan();
	// Start the processing thread to process the audio samples and recover the symbols.
	start_processing_thread();
}

monitor::~monitor()
{
	stop_processing_thread();
	// Free the FFT input and output buffers and destroy the FFT plan.
	reset_fft_buffers_and_plan();
}

// Load the parameters from the settings and update the internal variables accordingly.
void monitor::load_parameters()
{
	zc_settings settings;
	settings.get("FFT Size", fft_size_, fft_size_);
	settings.get("Image Interval", image_interval_, image_interval_);
	settings.get("Samples Per Image", samples_per_image_, samples_per_image_);
}

// Store the parameters from the settings.
void monitor::store_parameters() const
{
	zc_settings settings;
	settings.set("FFT Size", fft_size_);
	settings.set("Image Interval", image_interval_);
	settings.set("Samples Per Image", samples_per_image_);
}

// Add a new audio sample to the monitor. This will be called by the audio output module.
void monitor::add_audio_sample(float sample)
{
	// Add the new audio sample to the queue and update the head index.
	// Protected by mutex to ensure thread safety between audio callback and processing thread.
	std::lock_guard<std::mutex> lock(audio_queue_mutex_);
	audio_queue_.push_back(sample);
}

// Set display buffer for plotting the frequency domain images. Add a callback function to update the display when new images are available.
void monitor::set_display_buffer(zc_graph_::data_set_dens_t* buffer, std::function<void(void*)> callback, void* user_data)
{
	display_buffer_ = buffer;
	display_callback_ = callback;
	display_user_data_ = user_data;
}

// Start the processing thread to process the audio samples and recover the symbols.
void monitor::start_processing_thread()
{
	stop_processing_ = false;
	processing_thread_ = new std::thread(processing_thread_function, this);
}

// Stop the processing thread and wait for it to finish
void monitor::stop_processing_thread()
{
	stop_processing_ = true;
	if (processing_thread_ && processing_thread_->joinable()) {
		processing_thread_->join();
	}
	delete processing_thread_;
	processing_thread_ = nullptr;
}

// If necessary, reset the FFT buffers and plan and recreate them.
void monitor::reset_fft_buffers_and_plan()
{
	// Free the existing FFT input and output buffers and destroy the existing FFT plan.
	if (fft_plan_) {
		fftw_destroy_plan(fft_plan_);
		fft_plan_ = nullptr;
	}
	if (fft_input_buffer_) {
		fftw_free(fft_input_buffer_);
		fft_input_buffer_ = nullptr;
	}
	if (fft_output_buffer_) {
		fftw_free(fft_output_buffer_);
		fft_output_buffer_ = nullptr;
	}
}

// Create the FFT buffers and plan based on the current parameters.
void monitor::create_fft_buffers_and_plan() {
	// Allocate new FFT input and output buffers and create a new FFT plan with the updated parameters.
	fft_input_buffer_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size_);
	fft_output_buffer_ = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * fft_size_);
	fft_plan_ = fftw_plan_dft_1d(fft_size_, fft_input_buffer_, fft_output_buffer_, FFTW_FORWARD, FFTW_MEASURE);
}

// Restart the processing thread to apply the new parameters after resetting the FFT buffers and plan.
void monitor::reset_parameters() {
	stop_processing_thread();
	reset_fft_buffers_and_plan();
	load_parameters();
	create_fft_buffers_and_plan();
	start_processing_thread();
}

// Processing thread function to continuously process the audio samples and recover the symbols until signalled to stop.
void monitor::processing_thread_function(monitor* self)
{
	while (!self->stop_processing_) {
		// Check if there are enough samples to process (thread-safe with mutex).
		bool should_process = false;
		{
			std::lock_guard<std::mutex> lock(self->audio_queue_mutex_);
			should_process = (self->audio_queue_.size() >= self->samples_per_image_);
		}
		if (should_process) {
			self->process_audio_samples();
		}
		std::this_thread::yield();
		// TODO 10 ms is too long. This needs to be a smallish fraction (10%?) of the image_interval_.
//		std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Sleep for a short time to avoid busy waiting.
	}
}

// Process one chunk of audio samples
void monitor::process_audio_samples() {
	// Copy the first M samples of the audio buffer into the FFT input.
	// Protected by mutex to ensure thread safety with audio callback thread.
	{
		std::lock_guard<std::mutex> lock(audio_queue_mutex_);
		for (size_t i = 0; i < samples_per_image_; i++) {
			if (i < audio_queue_.size()) {
				fft_input_buffer_[i][0] = audio_queue_[i]; // Real part
				fft_input_buffer_[i][1] = 0.0; // Imaginary part
			}
			else {
				fft_input_buffer_[i][0] = 0.0; // Real part
				fft_input_buffer_[i][1] = 0.0; // Imaginary part
			}
		}
		// Remove the first N samples from the audio queue
		for (size_t i = 0; i < image_interval_; i++) {
			audio_queue_.pop_front();
		}
	}
	// Pad the remaining samples with zeros if there are less than F samples in the queue.
	for (size_t i = samples_per_image_; i < fft_size_; i++) {
		fft_input_buffer_[i][0] = 0.0; // Real part
		fft_input_buffer_[i][1] = 0.0; // Imaginary part
	}
	// Execute the FFT to get the frequency domain representation of the audio samples.
	fftw_execute(fft_plan_);
	// Update the display buffer with the new frequency domain image and call the display callback to update the display.
	if (display_buffer_ && display_callback_) {
		update_display_buffer();
	}

}

// Add the frequency domain image to the display buffer and call the display callback to update the display.
void monitor::update_display_buffer() {
	// Only process the frequency bins for the frequency range we are interested in
	size_t num_bins = display_buffer_->y_values.size(); 
	std::vector<float> image(num_bins);
	for (size_t i = 0; i < num_bins; i++) {
		float real = fft_output_buffer_[i][0];
		float imag = fft_output_buffer_[i][1];
		image[i] = std::sqrt(real * real + imag * imag); // Magnitude of the frequency component
	}
	// Add the new image to the image queue.
	image_queue_.push_back(image);
	// Scan the image queue to identify the candidate frequency bin for the transmitted signal and update the selected_signal_ queue with the magnitude of that bin over time.
	identify_signal_bin();
	// If the image queue exceeds the display depth, remove the oldest image.
	if (image_queue_.size() > display_depth_) {
		image_queue_.pop_front();
	}
	// Shift the display buffer 1 column left and add the current image at the right.
	for (int r = 0; r < num_bins; r++) {
		for (int c = 0; c < display_depth_ - 1; c++) {
			display_buffer_->z_values[r * display_depth_ + c] = display_buffer_->z_values[r * display_depth_ + c + 1];
		}
		display_buffer_->z_values[r * display_depth_ + display_depth_ - 1] = image[r];
	}
	// Call the display callback to update the display with the new image.
	display_callback_(display_user_data_);
}

// Identify the candidate frequency bin for the transmitted signal and add it to the selected_signal_ queue for use by the symbol detection algorithm.
void monitor::identify_signal_bin() {
	if (image_queue_.empty()) {
		return;
	}
	// Scan the entire image queue to find the frequency bin with the maximum magnitude 
	// for the maximum number of images.

	// First get the largest bin in each image.
	std::vector <int> largest_bin_count(image_queue_.front().size(), 0);
	// For each image find the bin with the largest magnitude.
	for (int i = 0; i < image_queue_.size(); i++) {
		const auto& current_image = image_queue_[i];
		int largest_bin = 0;
		auto max_value = current_image[0];
		const size_t bin_count = current_image.size();
		for (size_t j = 1; j < bin_count; j++) {
			const auto current_value = current_image[j];
			if (current_value > max_value) {
				max_value = current_value;
				largest_bin = j;
			}
		}
		largest_bin_count[largest_bin]++;
	}
	// Now get the number of the bin which is the largest bin in the most images.
	// That is the bin number for which the count in largest_bin_count is the largest.
	int bin_number = 0;
	for (int i = 0; i < largest_bin_count.size(); i++) {
		if (largest_bin_count[i] > largest_bin_count[bin_number]) {
			bin_number = i;
		}
	}
	// Update the selected_signal_ queue with the magnitude of the selected frequency bin over time.
	selected_signal_.push(image_queue_.back()[bin_number]);
	selected_signal_bin_ = bin_number;
}

// Return the frequency of the \p bin in Hz based on the FFT size and sample rate.
float monitor::get_bin_frequency(int bin) const {
	return static_cast<float>(bin) * DEFAULT_SAMPLE_RATE / fft_size_;
}