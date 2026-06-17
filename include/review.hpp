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

class Fl_Button;
class Fl_Check_Button;
class Fl_Choice;
class Fl_Group;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Text_Editor;
class Fl_Value_Slider;
class zc_graph_density;
class monitor;


//! \file review.hpp

//! \brief Class review - displays text represenattions for review.
//! 
//! This class will have three text fields: 
//! 1. The first will show the text that was sent by the trainer.
//! 2. The second will show the text that was typed in by the user.
//! 3. The third will show the text that was decoded from the audio by the trainer.
//! 
//! The text being sent can be displayed while being sent, 
//! or displayed after the sending is complete.
//! The text typed in by the user should be typed manually as
//! it is being sent. 
//! The text decoded from the audio will be displayed
//! as it is being captured. This text can either be
//! decoded from the audio being sent, or from 
//! the audio being captured from the microphone.
//! 
//! Controls include:
//! - A check box to show the sent text while it is being sent.
//! - A button to show the sent text after it has been sent.
//! - A button to compare the sent text with the text typed in by the user and show the results.
//! - A pulldown menu to select the source of the audio for decoding (no audio, sent audio or microphone audio).
//! - A button to compare the sent text with the text decoded from the audio and show the results.
//! 
//! The results of the comparisons will be shown by highlighting the characters in the 
//! typed text or the decoded text that do not match the sent text. 
//! The number of errors and the error rate will also be displayed (TBC).
//! Information about decoded audio such as pitch, WPM and timing variances
//! will also be displayed (TBC).
class review : public Fl_Double_Window {
public:
	//! \brief Constructor for review class.
	review(int W, int H, const char* L = nullptr);
	//! \brief Destructor for review class.
	~review();
	//! \brief Add sent text queue
	void add_sent_text_queue(zc_async_queue<std::string>* queue);
	//! \brief Clear a display of text.
	void clear_display(text_source_t source);
	//! \brief Clear all displays of text.
	void clear_all_displays();
	// Callbacks for controls.
	static void cb_as_sending(Fl_Widget* w, void* data);
	static void cb_show(Fl_Widget* w, void* data);
	static void cb_compare_typed(Fl_Widget* w, void* data);
	static void cb_decode_source(Fl_Widget* w, void* data);
	static void cb_compare_decoded(Fl_Widget* w, void* data);
	static void cb_ch_fft_size(Fl_Widget* w, void* data);
	static void cb_slider_overlap(Fl_Widget* w, void* data);
	static void cb_slider_max_pitch(Fl_Widget* w, void* data);
	static void cb_slider_max_time(Fl_Widget* w, void* data);

	//! Callback for unfinished styles - null function to prevent FLTK from crashing when it encounters a style that is not defined in the style table.
	static void cb_unfinished_style(int pos, void* data) {
		// Do nothing - this is just a placeholder to prevent FLTK from crashing when it encounters a style that is not defined in the style table.
	}

	//! Callback for Fl_Text_Buffer::add_modify_callback
	static void cb_modify(
		int pos, int nInserted, 
		int nDeleted, int nRestyled, 
		const char* deleted_text, void* data
	);

	//! Callback from zc_ticker to update the sent window
	static void cb_ticker(void* data);

	//! Callback to update spectrogram with latest audio sample
	static void cb_update_spectrogram(void* data);

	//! Callback to redraw the review window with latest spectrogram data
	//! Needs to run in main thread to avoid FLTK crashing when trying to draw from another thread.
	static void cb_redraw(void* data);

private:

	//! \brief Create the widgets for the review window.
	void create_widgets();

	//! \brief Load settings.
	void load_settings();

	//! \brief Save settings.
	void save_settings() const;

	//! \brief Compare the text in two text displays and highlight the differences.
	//! \param user The first text display to compare _ and the one to be highligthed.
	//! \param sent The second text display to compare _ and the one to be compared against.
	void compare_displays(Fl_Text_Display* user, Fl_Text_Display* sent);

	//! \brief Hide the text in a text display by displaying it in background colour.
	void hide_text(Fl_Text_Display* td);
	//! \brief Show the text in a text display by displaying it in foreground colour.
	void show_text(Fl_Text_Display* td);

	//! \brief Configure the spectogram graph widget
	//! This must include validation and correction of the settings, and 
	//! updating the spectrogram with the new settings.
	void configure_spectrogram();

	//! \brief Update decoder and spectrogram controls from settings
	void update_decoder_controls();

	//! \brief Add sent text to the review.
	void add_sent_text(const std::string& text, text_source_t source = text_source_t::SENT_TEXT);

	//! \brief Check and display from text queue. Called every 100 ms.
	void poll_text_queue();

	//! \brief Check and display the decoded text from the monitor. Called every 100 ms.
	void poll_decoded_text();

	//! \brief Callback from decoder in monitor.
	static void cb_decoder_callback(void* data, const std::string& decoded_text);

	// Widgets.
	Fl_Group* g_sent_;     //!< Group for sent text.
	Fl_Text_Display* td_sent_;  //!< Text display for sent text.
	Fl_Check_Button* ck_as_sending_;  //!< Check button to show sent text while sending.
	Fl_Button* btn_show_;  //!< Button to show sent text after sending.

	Fl_Group* g_typed_;    //!< Group for typed text.
	Fl_Text_Editor* td_typed_;  //!< Text display for typed text.
	Fl_Button* btn_compare_typed_;  //!< Button to compare sent text with typed text.

	Fl_Group* g_decoded_;  //!< Group for decoded text.
	Fl_Text_Display* td_decoded_;  //!< Text display for decoded text.
	Fl_Choice* ch_decode_source_;  //!< Choice to select source of audio for decoding.
	Fl_Button* btn_compare_decoded_;  //!< Button to compare sent text with decoded text.
	
	Fl_Group* g_sgram_;              //!< Group for spectogram and controls
	zc_graph_density* spectrogram_;  //!< Spectrogram (sideways waterfall) graph.
	Fl_Choice* ch_fft_size_;         //!< Size of FFT input (number of samples in each chunk)
	Fl_Value_Slider* sl_overlap_;    //!< Overlap between FFT chunks (in eigthth-chunks)
	Fl_Value_Slider* sl_max_freq_;   //!< Maximum audio frequency in display (1 kHz to SAMPLE_RATE/2)
	Fl_Value_Slider* sl_max_time_;   //!< Maximum time span in dispalay.
	Fl_Output* op_freq_bin_;         //!< Frequency bin size (in Hz)
	Fl_Output* op_time_slice_;       //!< Length of a time slice (in ms) - non overlapped part of a chunk. 
	Fl_Output* op_decoded_pitch_;    //!< Decoded pitch 
	Fl_Output* op_decoded_wpm_;      //!< Decoded WPM
	// Settings.
	bool show_as_sending_;  //!< Whether to show sent text while sending.
	audio_source_t decode_source_;  //!< Source of audio for decoding.

	//! Queue of sent text
	zc_async_queue<std::string>* text_queue_;
	//! Queue of decoded text
	zc_async_queue<std::string> decoded_text_queue_;

	//! Spectrogram data.
	zc_graph_::data_set_dens_t* spectrogram_data_capture_;  // Written by monitor thread
	zc_graph_::data_set_dens_t* spectrogram_data_display_;  // Read by FLTK on main thread
	std::mutex spectrogram_mutex_;                          // Protects buffer swap
	std::atomic<bool> spectrogram_data_ready_{false};       // Flag for buffer swap

	//! Main thread ID for thread safety checks
	std::thread::id main_thread_id_;

	//! Latest decoded pitch and WPM (set from monitor thread, read from main thread)
	std::atomic<double> latest_decoded_pitch_{0.0};
	std::atomic<double> latest_decoded_wpm_{0.0};

	//! Check if we're on the main thread, throw exception if not
	void check_main_thread(const char* method_name) const;

};