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
#include "codec.hpp"

//! \file monitor.hpp
//! 
#include "zc_async_deque.h"
#include "zc_async_queue.h"
#include "zc_graph_.h"
#include "zc_running_average.h"

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fftw3.h>


//! Used for running averages.
const size_t SPEED_HISTORY_LENGTH = 10;
const size_t LEVEL_HISTORY_LENGTH = 20;


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
//! The image queue will be continually scanned to look for the strongest signal in
//! each image. The strongest signal will be analysed to 
//! recover the Morse code symbols. When enough
//! recovered are presen to form a character the symbols will be presented to
//! the codec block and decoded into that character and fed to the review block to
//! display to the user.
//! 

class monitor
{
public:
	//! Constructor.
	monitor(zc_async_queue<double>* audio_sent, zc_async_queue<double>* audio_received);
	//! Destructor.
	~monitor();

	//! Set display buffer for plotting t6he frequency domain images. 
	//! Add a callback function to update the display when new images are available.
	void set_display_buffer(
		zc_graph_::data_set_dens_t* buffer, 
		std::vector<zc_graph_::data_point_t>* waveform_buffer,
		std::function<void(void*)> callback, void* user_data);

	//! Get the pitch of the selected bin in Hz. This is the frequency of the transmitted signal that is being monitored.
	double get_selected_bin_pitch() const {
		if (selected_signal_bin_ >= 0) {
			return get_bin_frequency(selected_signal_bin_);
		}
		else {
			return 0.0f;
		}
	}

	//! Get the decoded WPM
	double get_wpm() const;

	//! \brief Stop processing. Tidy up FFT etc.
	void stop_monitor();

	//! \brief Start processing. Configure FFT and start proeccsing thread.
	void start_monitor(double max_value);

	//! Set the decoded string callback
	void set_decode_callback(std::function<void(void*, const std::string&)> callback, void* user_data);

	//! Set the monitoring source. If set to NO_AUDIO, monitoring will be disabled. 
	//! Queues may still need to be drained.
	void set_monitoring_source(audio_source_t source) {
		switch (source) {
		case audio_source_t::NO_AUDIO:
			active_audio_queue_ = nullptr;
			break;
		case audio_source_t::SENT_AUDIO:
			active_audio_queue_ = audio_sent_queue_;
			break;
		case audio_source_t::MIC_AUDIO:
			active_audio_queue_ = audio_received_queue_;
			break;
		}
	}


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
	//! \return True if enabled.
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
	double get_bin_frequency(int bin) const;

	//! Decode the current monitored signal.
	symbol_t decode_signal(bool signal);

	//! Convert signal level to boolean. Involve hysteresis.
	bool get_signal(double f);

	//! Accumulate symbols
	void accumulate_symbol();

	//! Update monitored speed
	void update_speed();

	//! Update derived times
	void update_derived_times();

	//! Update detected signal levels
	void update_detected_signal_levels();

	//! Thread for processing the audio samples and recovering the symbols.
	std::thread* processing_thread_ = nullptr;
	//! Flag to signal the processing thread to stop.
	std::atomic<bool> stop_processing_ = false;

	//! Audio queue - sent by the app.
	zc_async_queue<double>* audio_sent_queue_ = nullptr;
	//! Audio queue - received by the monitor.
	zc_async_queue<double>* audio_received_queue_ = nullptr;
	//! Current active audio queue to be processed. This is a pointer to either the sent or received queue.
	std::atomic<zc_async_queue<double>*> active_audio_queue_ = nullptr;

	//! Local copy of the audio queue to be processed. This is a deque of audio samples.
	std::deque<double> audio_queue_copy_;

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
	std::function<void(void*)> display_callback_ = nullptr;
	//! User data to pass to the display callback function.
	void* display_user_data_ = nullptr;

	//! Waveform display buffer for plotting the audio samples.
	std::vector<zc_graph_::data_point_t>* waveform_display_buffer_ = nullptr;
	//! Interim waveform buffer to keep the audio samples in sync with the frequency domain images.
	std::queue<double> waveform_interim_buffer_;

	//! Decoded string callback
	std::function<void(void*, const std::string&)> decode_callback_ = nullptr;
	//! Decode callback suer data
	void* decode_user_data_ = nullptr;

	//! The queue of frequency images that have been processed and are waiting to be displayed.
	std::deque<std::vector<double>> image_queue_;

	//! The current signal value
	bool current_signal_ = false;
	//! The previous signal value
	bool previous_signal_ = false;
	//! The current symbol
	symbol_t current_symbol_ = symbol_t::UNFINISHED;
	//! The current array of symbols
	std::vector<symbol_t> recovered_symbols_;
	//! Image count since last symbol change
	unsigned int image_count_ = 0;
	//! Current monitored dit time in images.
	unsigned int dit_size_ = 0;
	//! Minimum dit time in images
	unsigned int min_dit_size_ = 0;
	//! Maximum dit time in tmages
	unsigned int max_dit_size_ = 0;
	//! Maximum internal space in images
	unsigned int max_int_size_ = 0;
	//! Maximum character space in images
	unsigned int max_char_size_ = 0;
	//! Running average of dot times
	zc_running_average<double, SPEED_HISTORY_LENGTH> dot_times_;
	//! Running average of dash times
	zc_running_average<double, SPEED_HISTORY_LENGTH> dash_times_;
	//! Signal training level - running mean: logic high.
	zc_running_average<double, LEVEL_HISTORY_LENGTH> running_mean_high_;
	//! Signal training level - running mean: logic low.
	zc_running_average<double, LEVEL_HISTORY_LENGTH> running_mean_low_;
	//! High signal trigger level - set to 2/3 between running mean high and low.
	double high_trigger_level_ = 0.7;
	//! Low signal trigger level - set to 1/3 between running mean high and low.
	double low_trigger_level_ = 0.3;

	//! Selected frequency bin number for extracting signal
	int selected_signal_bin_ = -1;

	//! Parameters for the monitor.
	unsigned int fft_size_ = 0; // Number of samples for the FFT (F).
	unsigned int image_interval_ = 0; // Interval between images in samples (N).
	unsigned int display_depth_ = 0; // Number of images to display in the frequency domain plot.

	//! FFT shaping window
	std::vector<double> shaping_window_;

};