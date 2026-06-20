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

#include "codec.hpp"
#include "params.hpp"

#include "zc_graph_.h"
#include "zc_settings.h"
#include "zc_utils.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <fftw3.h>

// Enable queue monitoring in debug builds
#ifdef _DEBUG
#define ENABLE_QUEUE_MONITORING
#endif

#ifdef ENABLE_QUEUE_MONITORING
#include <chrono>
#include <iostream>
#endif

// External sample rate variable defined in main.cpp. 
extern double DEFAULT_SAMPLE_RATE;
extern int DEFAULT_FFT_SIZE;
extern double DEFAULT_OVERLAP;
extern double DEFAULT_MAX_PITCH;
extern double DEFAULT_MAX_TIME;

monitor::monitor(zc_async_queue<double>* audio_sent, zc_async_queue<double>* audio_received)
	: audio_sent_queue_(audio_sent), 
	audio_received_queue_(audio_received)
{
}

monitor::~monitor()
{
	stop_monitor();
}

// Load the parameters from the settings and update the internal variables accordingly.
void monitor::load_parameters()
{
	zc_settings settings;
	settings.get("FFT Size", fft_size_, fft_size_);
	double overlap = DEFAULT_OVERLAP;
	settings.get("FFT Overlap %", overlap, overlap);
	double max_freq = DEFAULT_MAX_PITCH;
	settings.get("Spectrogram Frequency Span", max_freq, max_freq);
	double max_time = DEFAULT_MAX_TIME;
	settings.get("Spectrogram Time Span", max_time, max_time);
	double freq_bin = DEFAULT_SAMPLE_RATE / static_cast<double>(fft_size_);
	double interval = static_cast<double>(fft_size_) * (1.0 - overlap * 0.01);
	image_interval_ = static_cast<int>(interval);
	double time_per_sample = interval / DEFAULT_SAMPLE_RATE;
	// Calculate the number of images
	display_depth_ = static_cast<int>(max_time / time_per_sample);
	audio_source_t source = audio_source_t::NO_AUDIO;
	settings.get("Decode Source", source, source);
	// Use set dot speed as a basis for setting decoding thresholds
	double dot_speed;
	settings.get("Dot Speed", dot_speed, 20.0);
	if (dot_times_.empty()) dot_times_.add(1.2 / dot_speed);
	if (dash_times_.empty()) dash_times_.add(3.0 * dot_times_.value());
	update_derived_times();
}

// Store the parameters from the settings.
void monitor::store_parameters() const
{
}

// Set display buffer for plotting the frequency domain images. Add a callback function to update the display when new images are available.
void monitor::set_display_buffer(
	zc_graph_::data_set_dens_t* buffer, 
	std::vector<zc_graph_::data_point_t>* waveform_buffer,
	std::function<void(void*)> callback, void* user_data)
{
	display_buffer_ = buffer;
	waveform_display_buffer_ = waveform_buffer;
	display_callback_ = callback;
	display_user_data_ = user_data;
}

// Start the processing thread to process the audio samples and recover the symbols.
void monitor::start_processing_thread()
{
	if (processing_thread_) return;
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
	shaping_window_.resize(fft_size_);
	double inv_fft_size = 1.0 / static_cast<double>(fft_size_);
	// Create Hann window to shape the FFT input
	for (size_t i = 0; i < fft_size_; i++) {
		shaping_window_[i] = 0.5 * (1.0 - std::cos(2.0 * zc::PI * static_cast<double>(i) * inv_fft_size));
	}

}

// stop processing and tidy up FFT. 
void monitor::stop_monitor() {
	stop_processing_thread();
	reset_fft_buffers_and_plan();
};

// (Re-)configure, initialise FFT and start proeccsing.
const double HIGH_LEVEL = 2.0 / 3.0;
const double LOW_LEVEL = 1.0 / 3.0;
void monitor::start_monitor(double max_value) {
	load_parameters();
	image_queue_.clear();
	running_mean_high_.clear();
	// The initial value of the running mean high is set to 25% 
	// as the full max_value will be spread over several bins. 
	// The running mean low is set to 0.0.
	running_mean_high_.add(max_value * 0.25);
	running_mean_low_.clear();
	running_mean_low_.add(0.0);
	update_detected_signal_levels();
	selected_signal_bin_ = -1;
	create_fft_buffers_and_plan();
	start_processing_thread();
}

// Processing thread function to continuously process the audio samples and recover the symbols until signalled to stop.
void monitor::processing_thread_function(monitor* self)
{
#ifdef ENABLE_QUEUE_MONITORING
	fprintf(stderr, "[THREAD] Monitor thread started, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	try {
#endif
		while (!self->stop_processing_) {
			// Check if there are enough samples to process (thread-safe with mutex).
			bool should_process = false;
			{
				zc_async_queue<double>* active_queue = self->active_audio_queue_;
				should_process = (!active_queue || active_queue->size() >= self->fft_size_);
			}
			if (should_process) {
				self->process_audio_samples();
			}
			std::this_thread::yield();
		}
#ifdef ENABLE_QUEUE_MONITORING
		fprintf(stderr, "[THREAD] Monitor thread exiting normally, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
	catch (const std::exception& e) {
		// Log the exception so we know what went wrong
		fprintf(stderr, "[THREAD] Monitor thread exception, ID: %zu, error: %s\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), e.what());
		// Thread will exit cleanly with code 0 instead of 1
	}
	catch (...) {
		fprintf(stderr, "[THREAD] Monitor thread unknown exception, ID: %zu\n", std::hash<std::thread::id>{}(std::this_thread::get_id()));
	}
#endif
}

// Process one chunk of audio samples
void monitor::process_audio_samples() {
	// Copy the first M samples of the audio buffer into the FFT input.
	// Protected by mutex to ensure thread safety with audio callback thread.
	{
		if (active_audio_queue_) {
			while (audio_queue_copy_.size() < fft_size_) {
				// Copy the audio queue 
				double sample;
				zc_async_queue<double>* active_queue = active_audio_queue_;
				if (active_queue && active_queue->try_pop(sample)) {
					audio_queue_copy_.push_back(sample);
				}
				else {
					break;
				}
			}
			for (size_t i = 0; i < fft_size_; i++) {
				if (i < audio_queue_copy_.size()) {
					fft_input_buffer_[i][0] = audio_queue_copy_[i] * shaping_window_[i]; // Real part
					fft_input_buffer_[i][1] = 0.0; // Imaginary part
				}
				else {
					fft_input_buffer_[i][0] = 0.0; // Real part
					fft_input_buffer_[i][1] = 0.0; // Imaginary part
				}
			}
			// Copy the first N samples to the waveform interim buffer for display.
			for (size_t i = 0; i < image_interval_ && i < audio_queue_copy_.size(); i++) {
				waveform_interim_buffer_.push(audio_queue_copy_[i]);
			}
			// Remove the first N samples from the audio queue
			for (size_t i = 0; i < image_interval_ && !audio_queue_copy_.empty(); i++) {
	
				audio_queue_copy_.pop_front();
			}
		}
		// TODO The below code is in the audio processing path - consider moving it as it is not 
		// needed for the FFT processing. 
		// Drain the queues that are not being monitored to prevent them from 
		// filling up and blocking the audio callback thread.
		if (active_audio_queue_ != audio_sent_queue_) {
			double sample;
			while (audio_sent_queue_->try_pop(sample)) {
				// Discard the sample
			}
		}
		if (active_audio_queue_ != audio_received_queue_) {
			double sample;
			while (audio_received_queue_->try_pop(sample)) {
				// Discard the sample
			}
		}
	}
	// Execute the FFT to get the frequency domain representation of the audio samples.
	fftw_execute(fft_plan_);
	// Update the display buffer with the new frequency domain image and call the display callback to update the display.
	if (display_buffer_ && display_callback_ && active_audio_queue_) {
		update_display_buffer();
	}

}

// Add the frequency domain image to the display buffer and call the display callback to update the display.
void monitor::update_display_buffer() {
	// Guard against race conditions during initialization/shutdown
	if (!display_buffer_ || !display_callback_) {
		return;  // Buffer not ready yet or already cleaned up
	}

	// Only process the frequency bins for the frequency range we are interested in
	size_t num_bins = display_buffer_->y_values.size();
	if (num_bins == 0 || display_depth_ == 0) {
		return;  // Not initialized properly
	}

	// Verify z_values is sized correctly to prevent out-of-bounds access
	if (display_buffer_->z_values.size() != num_bins * display_depth_) {
		return;  // Size mismatch - avoid crash
	}

	std::vector<double> image(num_bins);
	for (size_t i = 0; i < num_bins; i++) {
		double real = fft_output_buffer_[i][0];
		double imag = fft_output_buffer_[i][1];
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
	// Update the waveform display buffer with the current waveform data.
	// Shift the waveform display buffer N samples left and add the current waveform data at the right.
	int iw = 0;
	int iv = iw + image_interval_;
	int copy_last = (int)waveform_display_buffer_->size() - image_interval_;
	while(iw < copy_last) {
			
		(*waveform_display_buffer_)[iw++].second = (*waveform_display_buffer_)[iv++].second;
	}
	double d = 0.0;
	while(iw < waveform_display_buffer_->size()) {
		d = waveform_interim_buffer_.empty() ? 0.0 : waveform_interim_buffer_.front();
		(*waveform_display_buffer_)[iw++].second = d;
		if (!waveform_interim_buffer_.empty()) {
			waveform_interim_buffer_.pop();
		}
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
		// If that magnitude is considered a logic high, increment the count for that bin.
		if (get_signal(max_value)) {
			largest_bin_count[largest_bin]++;
		}
	}
	// Now get the number of the bin which is the largest bin in the most images.
	// That is the bin number for which the count in largest_bin_count is the largest.
	// Default to the previously selected bin.
	int bin_number = selected_signal_bin_;
	for (int i = 0; i < largest_bin_count.size(); i++) {
		if (bin_number < 0 || largest_bin_count[i] > largest_bin_count[bin_number]) {
			bin_number = i;
		}
	}
	// Update the selected_signal_ queue with the magnitude of the selected frequency bin over time.
	bool signal = get_signal(image_queue_.back()[bin_number]);
	image_count_++;
	symbol_t symbol = decode_signal(signal);
	if (symbol != symbol_t::UNFINISHED) {
//		printf("Decoded symbol: %s, Duration: %d, Dit size: %d\n", symbol_strings_.at(symbol).c_str(), image_count_, dit_size_);
		current_symbol_ = symbol;
		accumulate_symbol();
		update_speed();
		image_count_ = 0;
		// Only update the selected signal bin if we have a valid symbol. 
		selected_signal_bin_ = bin_number;
	}

}

// Return the frequency of the \p bin in Hz based on the FFT size and sample rate.
double monitor::get_bin_frequency(int bin) const {
	return static_cast<double>(bin) * DEFAULT_SAMPLE_RATE / fft_size_;
}

// Set the decode callback and data
void monitor::set_decode_callback(std::function<void(void*, const std::string&)> callback, void* user_data) {
	decode_callback_ = callback;
	decode_user_data_ = user_data;
}

// Convert the signal into a Boolean value
bool monitor::get_signal(double signal) {
	bool result = previous_signal_;
	if (signal > high_trigger_level_) {
		result = true;
		running_mean_high_.add(signal);
	}
	else if (signal < low_trigger_level_) {
		result = false;
		running_mean_low_.add(signal);
	}
	update_detected_signal_levels();

	//if (result != previous_signal_) 
	//	printf("Signal: %f, Result: %d, Duration: %d\n", signal, previous_signal_, image_count_);
	return result;
}

//! Update detected signal levels
void monitor::update_detected_signal_levels() {
	// Update the trigger levels based on the running means
	high_trigger_level_ = running_mean_high_.value() * HIGH_LEVEL + 
		running_mean_low_.value() * (1.0 - HIGH_LEVEL);
	low_trigger_level_ = running_mean_high_.value() * LOW_LEVEL + 
		running_mean_low_.value() * (1.0 - LOW_LEVEL);
}


// Decode signal. This code is cribbed off my arduino sketch doing the same job.
// If the level has transited check the duration since the last one and
// decode the symbol accordingly.
symbol_t monitor::decode_signal(bool signal) {
	symbol_t result = symbol_t::UNFINISHED;
	if (signal && !previous_signal_) {
		if (image_count_ < min_dit_size_) {
			result = symbol_t::NOISE;
		}
		else if (image_count_ < max_int_size_) {
			result = symbol_t::INTERNAL_SPACE;
		}
		else if (image_count_ < max_char_size_) {
			result = symbol_t::CHARACTER_SPACE;
		}
		else {
			result = symbol_t::WORD_SPACE;
		}
	} 
	else if (signal && previous_signal_) {
		// \todo handle stuck high
	}
	else if (!signal && !previous_signal_) {
		if (image_count_ > max_char_size_) {
			result = symbol_t::WORD_SPACE;
		}
	}
	else {
		if (image_count_ < min_dit_size_) {
			result = symbol_t::NOISE;
		}
		else if (image_count_ < max_dit_size_) {
			result = symbol_t::DOT_MARK;
		} 
		else {
			result = symbol_t::DASH_MARK;
		}
	}
	previous_signal_ = signal;
	return result;
}

// Take the current symbol and append it to the current list.
// If we have a character or word space translate the Morse code
// symbols into one or more characters and call back.
void monitor::accumulate_symbol() {
	recovered_symbols_.push_back(current_symbol_);
	if (current_symbol_ == symbol_t::WORD_SPACE ||
		current_symbol_ == symbol_t::CHARACTER_SPACE) {
		std::string word;
		codec::decode(recovered_symbols_, word);
		decode_callback_(decode_user_data_, word);
		recovered_symbols_.clear();
	}
}

// Update the speed 
void monitor::update_speed() {
	// Constants
	double MAX_DASH_DOT = 4.8;    // Maximum dash:dot ratio.
	double MIN_DASH_DOT = 2.6;    // Minimum dash:dot ration.
	double duration = static_cast<double>(image_count_ * image_interval_) / DEFAULT_SAMPLE_RATE;
	switch (current_symbol_) {
	case symbol_t::DOT_MARK:
	case symbol_t::INTERNAL_SPACE:
		dot_times_.add(duration);
		break;
	case symbol_t::DASH_MARK: {
		dash_times_.add(duration);
		double weight = dash_times_.value() / dot_times_.value();
		if (weight < MIN_DASH_DOT) {
			dot_times_.clear();
			dot_times_.add(dash_times_.value() / MIN_DASH_DOT);
		}
		else if (weight > MAX_DASH_DOT) {
			dot_times_.clear();
			dot_times_.add(dash_times_.value() / MAX_DASH_DOT);
		}
	}
		break;
	default:
		// Do nothin
		break;
	}
	update_derived_times();
}

// Convert monitored dot time into the cvarious threshold times
void monitor::update_derived_times() {
	unsigned int dit_samples = static_cast<unsigned int>(dot_times_.value() * DEFAULT_SAMPLE_RATE);
	dit_size_ = dit_samples / image_interval_;
	min_dit_size_ = 0; 
	max_dit_size_ = dit_size_ * 2;
	max_int_size_ = dit_size_ * 2;
	max_char_size_ = dit_size_ * 6;
}

// Get the decoded WPM 
double monitor::get_wpm() const {
	if (dot_times_.empty()) return 0.0;
	return 1.2 / dot_times_.value();
}