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

#include "zc_async_deque.h"
#include "zc_async_queue.h"
#include "zc_graph_.h"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Widget.H>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Fl_Button;
class Fl_Check_Button;
class Fl_Choice;
class Fl_Group;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Text_Editor;
class zc_wheel_value_slider;
class zc_graph_density;
class monitor;

//! \file decode_control.hpp

//! \brief Class decode_control - displays decoder controls.
//! 
//! Controls include:
//! FFT configuration.
//! Spectrogram and waveform displays.
//! 
class decode_control : public Fl_Double_Window {
public:
	//! \brief Constructor for decode_control class.
	decode_control(int W, int H, const char* L = nullptr);
	//! \brief Destructor for decode_control class.
	~decode_control();
	static void cb_decode_source(Fl_Widget* w, void* data);
	static void cb_ch_fft_size(Fl_Widget* w, void* data);
	static void cb_slider_overlap(Fl_Widget* w, void* data);
	static void cb_slider_max_pitch(Fl_Widget* w, void* data);
	static void cb_slider_max_time(Fl_Widget* w, void* data);
	static void cb_slider_squelch(Fl_Widget* w, void* data);

	//! Callback from zc_ticker to update the sent window
	static void cb_ticker(void* data);

	//! Callback to update spectrogram with latest audio sample
	static void cb_update_spectrogram(void* data);

	//! Callback to redraw the review window with latest spectrogram data
	//! Needs to run in main thread to avoid FLTK crashing when trying to draw from another thread.
	static void cb_redraw(void* data);

	//! \brief Load settings.
	void load_settings();

	//! \brief Start decoder and spectrogram display.
	void start_decoder() {
		update_decoder_controls();
		configure_spectrogram();
	}

private:

	//! \brief Create the widgets for the review window.
	void create_widgets();

	//! \brief Save settings.
	void save_settings() const;

	//! \brief Configure the spectogram graph widget
	//! This must include validation and correction of the settings, and 
	//! updating the spectrogram with the new settings.
	void configure_spectrogram();

	//! \brief Update decoder and spectrogram controls from settings
	void update_decoder_controls();

	//! \brief Callback from decoder in monitor.
	static void cb_decoder_callback(void* data, const std::string& decoded_text);

	// Widgets.

	Fl_Group* g_sgram_;              //!< Group for spectogram and controls
	zc_graph_density* spectrogram_;  //!< Spectrogram (sideways waterfall) graph.
	zc_graph_cartesian* waveform_;   //!< Waveform graph.
	Fl_Choice* ch_decode_source_;    //!< Choice to select source of audio for decoding.
	Fl_Choice* ch_fft_size_;         //!< Size of FFT input (number of samples in each chunk)
	zc_wheel_value_slider* sl_overlap_;    //!< Overlap between FFT chunks (in eigthth-chunks)
	zc_wheel_value_slider* sl_max_freq_;   //!< Maximum audio frequency in display (1 kHz to SAMPLE_RATE/2)
	zc_wheel_value_slider* sl_max_time_;   //!< Maximum time span in dispalay.
	zc_wheel_value_slider* sl_squelch_;    //!< Squelch (default level at which to start decoding
	Fl_Output* op_freq_bin_;         //!< Frequency bin size (in Hz)
	Fl_Output* op_time_slice_;       //!< Length of a time slice (in ms) - non overlapped part of a chunk. 
	Fl_Output* op_decoded_pitch_;    //!< Decoded pitch 
	Fl_Output* op_decoded_wpm_;      //!< Decoded WPM

	//! Sample rate
	double sample_rate_;
	audio_source_t decode_source_;  //!< Source of audio for decoding.

	//! Spectrogram data.
	zc_graph_::data_set_dens_t* spectrogram_data_capture_ = nullptr;  // Written by monitor thread
	zc_graph_::data_set_dens_t* spectrogram_data_display_ = nullptr;  // Read by FLTK on main thread
	std::mutex spectrogram_mutex_;                          // Protects buffer swap
	std::atomic<bool> spectrogram_data_ready_{ false };       // Flag for buffer swap
	//! Waveform data. Control is the same as for spectrogram data.
	std::vector<zc_graph_::data_point_t>* waveform_data_capture_ = nullptr;  // Written by monitor thread
	std::vector<zc_graph_::data_point_t>* waveform_data_display_ = nullptr;  // Read by FLTK on main thread

	//! Main thread ID for thread safety checks
	std::thread::id main_thread_id_;

	//! Latest decoded pitch and WPM (set from monitor thread, read from main thread)
	std::atomic<double> latest_decoded_pitch_{ 0.0 };
	std::atomic<double> latest_decoded_wpm_{ 0.0 };

	//! Check if we're on the main thread, throw exception if not
	void check_main_thread(const char* method_name) const;

};