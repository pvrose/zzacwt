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
#include "review.hpp"

#include "monitor.hpp"
#include "params.hpp"

#include "zc_async_queue.h"
#include "zc_fltk.h"
#include "zc_graph_.h"
#include "zc_settings.h"
#include "zc_ticker.h"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Widget.H>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Text display style map 
static Fl_Text_Display::Style_Table_Entry STYLE_TABLE[] = {
	// {Text colour, Font, Font size, Attribute flags, Background colour}
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR },  // Normal shown text.
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_BGCOLOR, FL_FOREGROUND_COLOR }, // Hidden text.
	{ FL_RED, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR }, // "Error" text - red.
	{ FL_BLACK, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR }, // "Match" text - black.
};
static int STYLE_TABLE_SIZE = sizeof(STYLE_TABLE) / sizeof(STYLE_TABLE[0]);
static char STYLE_NORMAL = 'A';
static char STYLE_HIDDEN = 'B';
static char STYLE_ERROR = 'C';
static char STYLE_MATCH = 'D';

extern zc_ticker* ticker_;
extern monitor* monitor_;
extern double DEFAULT_SAMPLE_RATE;
extern int DEFAULT_FFT_SIZE;
extern double DEFAULT_OVERLAP;
extern double DEFAULT_MAX_PITCH;
extern double DEFAULT_MAX_TIME;

// Constructor
review::review(int W, int H, const char* L) : Fl_Double_Window(W, H, L) {
	// Capture main thread ID for thread safety checks
	main_thread_id_ = std::this_thread::get_id();
	create_widgets();
	load_settings();
	ticker_->add_ticker(this, cb_ticker, 1, false);
}

// Destructor
review::~review() {
}

// Create the widgets for the review window.
void review::create_widgets() {
	int cx = GAP;
	int cy = GAP;
	int WDISPLAY = 300;
	int WGROUPS = GAP + WBUTTON + WDISPLAY + GAP;
	int HDISPLAY = 120;
	int HGROUPS = HTEXT + HDISPLAY + GAP;
	g_sent_ = new Fl_Group(cx, cy, WGROUPS, HGROUPS, "Sent Data");
	g_sent_->box(FL_BORDER_BOX);
	g_sent_->align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	cx += GAP;
	cy += HTEXT;
	ck_as_sending_ = new Fl_Check_Button(cx, cy, HBUTTON, HBUTTON, "As sent");
	ck_as_sending_->align(FL_ALIGN_RIGHT);
	ck_as_sending_->callback(cb_as_sending, this);
	cy += HBUTTON;

	btn_show_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Show");
	btn_show_->callback(cb_show, this);
	cy += HBUTTON + GAP;

	cx += WBUTTON;
	cy = g_sent_->y() + GAP;
	td_sent_ = new Fl_Text_Display(cx, cy, WDISPLAY, HDISPLAY);
	td_sent_->buffer(new Fl_Text_Buffer);
	td_sent_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, 'A', cb_unfinished_style, nullptr);
	td_sent_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

	g_sent_->end();

	cx = g_sent_->x();
	cy = g_sent_->y() + g_sent_->h() + GAP;

	g_typed_ = new Fl_Group(cx, cy, WGROUPS, HGROUPS, "Typed Data");
	g_typed_->box(FL_BORDER_BOX);
	g_typed_->align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	cx = g_typed_->x() + GAP;
	cy = g_typed_->y() + HTEXT;

	btn_compare_typed_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Compare");
	btn_compare_typed_->callback(cb_compare_typed, this);

	cx = g_typed_->x() + GAP + WBUTTON;
	cy = g_typed_->y() + HTEXT;

	td_typed_ = new Fl_Text_Editor(cx, cy, WDISPLAY, HDISPLAY);
	td_typed_->buffer(new Fl_Text_Buffer);
	td_typed_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_NORMAL, nullptr, nullptr);
	td_typed_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
	td_typed_->textfont(FL_HELVETICA);
	td_typed_->textsize(FL_NORMAL_SIZE);
	td_typed_->textcolor(FL_FOREGROUND_COLOR);
	td_typed_->buffer()->add_modify_callback(cb_modify, td_typed_);

	g_typed_->end();

	cx = g_typed_->x();
	cy = g_typed_->y() + g_typed_->h() + GAP;

	g_decoded_ = new Fl_Group(cx, cy, WGROUPS, HGROUPS, "Decoded Data");
	g_decoded_->box(FL_BORDER_BOX);
	g_decoded_->align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

	cx += GAP;
	cy += HTEXT;

	ch_decode_source_ = new Fl_Choice(cx, cy, WBUTTON, HBUTTON, "Source");
	// Add options to the choice menu - only audio decodes
	for (const auto& [source, label] : audio_source_strings_) {
		ch_decode_source_->add(label.c_str());
	}
	ch_decode_source_->align(FL_ALIGN_BOTTOM);
	ch_decode_source_->callback(cb_decode_source, this);
	cy += HBUTTON + HTEXT;

	btn_compare_decoded_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Compare");
	btn_compare_decoded_->callback(cb_compare_decoded, this);

	cx = g_decoded_->x() + GAP + WBUTTON;
	cy = g_decoded_->y() + HTEXT;
	td_decoded_ = new Fl_Text_Display(cx, cy, WDISPLAY, HDISPLAY);
	td_decoded_->buffer(new Fl_Text_Buffer);
	td_decoded_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_NORMAL, nullptr, nullptr);
	td_decoded_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

	g_decoded_->end();

	cy = g_decoded_->y() + g_decoded_->h() + GAP;
	cx = g_decoded_->x();

	g_sgram_ = new Fl_Group(cx, cy, WGROUPS, HGROUPS + HBUTTON * 3, "Spectrogram");
	g_sgram_->box(FL_BORDER_BOX);
	g_sgram_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_TOP);

	cx += GAP + WLABEL;
	cy += HTEXT;

	ch_fft_size_ = new Fl_Choice(cx, cy, WBUTTON, HBUTTON, "FFT Size");
	ch_fft_size_->align(FL_ALIGN_LEFT);
	ch_fft_size_->callback(cb_ch_fft_size, (void*)this);
	ch_fft_size_->tooltip("Select the appropriate FFT size");

	cy += HBUTTON;
	sl_overlap_ = new Fl_Value_Slider(cx, cy, WBUTTON, HBUTTON, "Overlap");
	sl_overlap_->type(FL_HOR_SLIDER);
	sl_overlap_->align(FL_ALIGN_LEFT);
	sl_overlap_->callback(cb_slider_overlap, (void*)this);
	sl_overlap_->tooltip("Select the sample overlap %");
	sl_overlap_->range(0.0, 87.5);
	sl_overlap_->step(12.5);

	cy += HBUTTON;
	sl_max_freq_ = new Fl_Value_Slider(cx, cy, WBUTTON, HBUTTON, "Max Freq");
	sl_max_freq_->type(FL_HOR_SLIDER);
	sl_max_freq_->align(FL_ALIGN_LEFT);
	sl_max_freq_->callback(cb_slider_max_pitch, (void*)this);
	sl_max_freq_->tooltip("Select the maximum frequency displayed");
	sl_max_freq_->range(100.0, DEFAULT_SAMPLE_RATE / 2.0);
	sl_max_freq_->step(100.0);

	cy += HBUTTON;
	sl_max_time_ = new Fl_Value_Slider(cx, cy, WBUTTON, HBUTTON, "Max Time");
	sl_max_time_->type(FL_HOR_SLIDER);
	sl_max_time_->align(FL_ALIGN_LEFT);
	sl_max_time_->callback(cb_slider_max_time, (void*)this);
	sl_max_time_->tooltip("select the maximum time displayed");
	sl_max_time_->range(0.1, 10);
	sl_max_time_->step(0.05);

	cy += HBUTTON;
	op_freq_bin_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "Step (Hz)");
	op_freq_bin_->align(FL_ALIGN_LEFT);
	op_freq_bin_->tooltip("Displays the frequency resolution in hertz");

	cy += HBUTTON;
	op_time_slice_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "Step (ms)");
	op_time_slice_->align(FL_ALIGN_LEFT);
	op_time_slice_->tooltip("Displays ythetime resilution in milliseconds");

	cy += HBUTTON;
	op_decoded_pitch_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "Freq (Hz)");
	op_decoded_pitch_->align(FL_ALIGN_LEFT);
	op_decoded_pitch_->tooltip("Displays the frequency bin being decoded");

	cy += HBUTTON;
	op_decoded_wpm_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "WPM");
	op_decoded_wpm_->align(FL_ALIGN_LEFT);
	op_decoded_wpm_->tooltip("Displays the decoded WPM");

	cy = g_sgram_->y() + HTEXT;
	cx += WBUTTON;
	const int WSGRAM = g_sgram_->w() - GAP - (cx - g_sgram_->x());

	spectrogram_ = new zc_graph_density(cx, cy, WSGRAM, HDISPLAY);
	configure_spectrogram();

	g_sgram_->end();

	cy += g_sgram_->h() + GAP;

	end();
	// Resize the window to fit the widgets.
	resizable(nullptr);
	size(GAP + WGROUPS + GAP, cy);

}

// Load settings.
void review::load_settings() {
	zc_settings settings;
	settings.get("Display While Sending", show_as_sending_, false);
	settings.get("Decode Source", decode_source_, audio_source_t::NO_AUDIO);
	ck_as_sending_->value(show_as_sending_);
	ch_decode_source_->value(static_cast<int>(decode_source_));
	if (ck_as_sending_->value()) {
		show_text(td_sent_);
	}
	else {
		hide_text(td_sent_);
	}
}

// Save settings.
void review::save_settings() const {
	zc_settings settings;
	settings.set("Display While Sending", show_as_sending_);
	settings.set("Decode Source", decode_source_);
}

// Check if we're on the main thread, throw exception if not
void review::check_main_thread(const char* method_name) const {
	if (std::this_thread::get_id() != main_thread_id_) {
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), 
			"THREAD SAFETY VIOLATION: %s called from non-main thread!", method_name);
		throw std::runtime_error(error_msg);
	}
}

// Add sent text to the review.
void review::add_sent_text(const std::string& text, text_source_t source) {
	// Thread safety check - must be called from main thread only
	check_main_thread("review::add_sent_text");

	switch (source) {
	case text_source_t::SENT_TEXT: {
		Fl_Text_Buffer* buffer = td_sent_->buffer();
		buffer->append(text.c_str());
		Fl_Text_Buffer* highlight_buffer = td_sent_->style_buffer();
		// Highlight the new text as hidden if show_as_sending_ is false, otherwise highlight it as normal.
		char style_char = show_as_sending_ ? STYLE_NORMAL : STYLE_HIDDEN;
		char* style_str = new char[text.size() + 1];
		// Fill the style string with the appropriate style character and null-terminate it.
		*std::fill_n(style_str, text.size(), style_char) = '\0';
		highlight_buffer->append(style_str);
		delete[] style_str;
		// Scroll to the end of the display to show the new text.
		// Get the line number of the last line in the display and scroll to it.
		int last_line = td_sent_->line_start(buffer->length() - 1);
		td_sent_->scroll(last_line, 0);
		// Mark the display for redraw to show the new text.
		td_sent_->redraw();
		break;
	}
	case text_source_t::DECODED_TEXT: {
		Fl_Text_Buffer* buffer = td_decoded_->buffer();
		buffer->append(text.c_str());
		Fl_Text_Buffer* highlight_buffer = td_decoded_->style_buffer();
		// Highlight the new text as hidden if show_as_sending_ is false, otherwise highlight it as normal.
		char style_char = STYLE_NORMAL;
		char* style_str = new char[text.size() + 1];
		// Fill the style string with the appropriate style character and null-terminate it.
		*std::fill_n(style_str, text.size(), style_char) = '\0';
		highlight_buffer->append(style_str);
		delete[] style_str;
		// Scroll to the end of the display to show the new text.
		// Get the line number of the last line in the display and scroll to it.
		int last_line = td_decoded_->line_start(buffer->length() - 1);
		td_decoded_->scroll(last_line, 0);
		// Mark the display for redraw to show the new text.
		td_decoded_->redraw();
		break;
	}
	}
}

// Placeholders for hide_text and show_text functions.
void review::hide_text(Fl_Text_Display* td) {
	Fl_Text_Buffer* highlight_buffer = td->style_buffer();
	// Set the style of all text in the display to the hidden style (style 'B').
	// Create a style string of the same length as the text in the display, filled with 'B', and set it as the new highlight buffer.
	char* style_str = new char[highlight_buffer->length() + 1];
	*std::fill_n(style_str, highlight_buffer->length(), STYLE_HIDDEN) = '\0';
	highlight_buffer->text(style_str);
	delete[] style_str;
	td->redraw();
	Fl::check();
}
void review::show_text(Fl_Text_Display* td) {
	Fl_Text_Buffer* highlight_buffer = td->style_buffer();
	// Set the style of all text in the display to the normal style (style 'A').
	char* style_str = new char[highlight_buffer->length() + 1];
	*std::fill_n(style_str, highlight_buffer->length(), STYLE_NORMAL) = '\0';
	highlight_buffer->text(style_str);
	delete[] style_str;
	td->redraw();
	Fl::check();
}

// Clear a display of text.
void review::clear_display(text_source_t source) {
	if (source == text_source_t::SENT_TEXT) {
		td_sent_->buffer()->text("");
		td_sent_->style_buffer()->text("");
		td_sent_->redraw();
	}
	else if (source == text_source_t::TYPED_TEXT) {
		td_typed_->buffer()->text("");
		td_typed_->style_buffer()->text("A");
//		td_typed_->take_focus();
		td_typed_->redraw();
	}
	else if (source == text_source_t::DECODED_TEXT) {
		td_decoded_->buffer()->text("");
		td_decoded_->style_buffer()->text("");
		td_decoded_->redraw();
	}
	else {
		// TODO - handle other sources of text if needed.
	}
}

// Clear all displays of text.
void review::clear_all_displays() {
	clear_display(text_source_t::SENT_TEXT);
	clear_display(text_source_t::DECODED_TEXT);
	clear_display(text_source_t::TYPED_TEXT);
}

// Add placeholders for the callback functions and the compare_displays function.
void review::cb_as_sending(Fl_Widget* w, void* data) {
	// Toggle the show_as_sending_ setting and update the display of sent text accordingly.
	review* r = static_cast<review*>(data);
	r->show_as_sending_ = r->ck_as_sending_->value();
	// Save settings
	r->save_settings();
}

void review::cb_show(Fl_Widget* w, void* data) {
	// Change the style of the sent text to normal to show it, and mark the display for redraw.
	review* r = static_cast<review*>(data);
	r->show_text(r->td_sent_);
}

void review::cb_compare_typed(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	r->compare_displays(r->td_typed_, r->td_sent_);
}

void review::cb_decode_source(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	r->decode_source_ = static_cast<audio_source_t>(((Fl_Choice*)w)->value());
	zc_settings settings;
	settings.set("Decode Source", r->decode_source_);
	r->update_decoder_controls();
	monitor_->stop_monitor();
	r->configure_spectrogram();
}

void review::cb_compare_decoded(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	r->compare_displays(r->td_decoded_, r->td_sent_);
}

// Callback for spectrogram control - FFT size
void review::cb_ch_fft_size(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	Fl_Choice* choice = static_cast<Fl_Choice*>(w);
	int value = choice->value();
	int fft_size = 64 << (value);
	zc_settings settings;
	settings.set("FFT Size", fft_size);
	r->configure_spectrogram();
}

// Callback to set the FFT overlap 
void review::cb_slider_overlap(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	Fl_Value_Slider* slider = static_cast<Fl_Value_Slider*>(w);
	double pc_overlap = slider->value();
	zc_settings settings;
	settings.set("FFT Overlap %", pc_overlap);
	r->configure_spectrogram();
}

// Callback to set the display frequency range
void review::cb_slider_max_pitch(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	Fl_Value_Slider* slider = static_cast<Fl_Value_Slider*>(w);
	double max_freq = slider->value();
	zc_settings settings;
	settings.set("Spectrogram Frequency Span", max_freq);
	r->configure_spectrogram();
}

// Callback to set the display time range
void review::cb_slider_max_time(Fl_Widget* w, void* data) {
	review* r = static_cast<review*>(data);
	Fl_Value_Slider* slider = static_cast<Fl_Value_Slider*>(w);
	double max_time = slider->value();
	zc_settings settings;
	settings.set("Spectrogram Time Span", max_time);
	r->configure_spectrogram();
}

// Compare the text in two text displays and highlight the differences.
// Assume that the user is more likely to omit or get wrong rather than 
// add extra characters.
// Check the two strings in the displays from the start. mark the first 
// characters (at least M) that match as "Matched" in user. Step both data by this amount.
// If there is a mismatch, step the sent data by one character and check for
// a match of at least M characters. If there is a match, copy the mismatched
// character to user and marke as "Missing". If there is no match, Step the send data 
// again by one character and check again: repeat for upto N characters or
// until a match is found.
// If no match is found, mark the mismatched character in user as "Wrong"
const int M = 2; // Number of characters to check for a match.
const int N = 3; // Maximum number of characters to step when looking for a match.
void review::compare_displays(Fl_Text_Display* user, Fl_Text_Display* sent) {
	// Convert the text from the user and sent displays as upper case 
	// into new strings for processing, 
	// and create new style strings for the user and sent displays.
	std::string user_upper(zc::to_upper(user->buffer()->text()));
	std::string sent_upper(zc::to_upper(sent->buffer()->text()));
	const char* user_text = user_upper.c_str();
	const char* sent_text = sent_upper.c_str();

	size_t user_len = std::strlen(user_text);
	size_t sent_len = std::strlen(sent_text);
	char* user_style = new char[user_len + 1];
	char* sent_style = new char[sent_len + 1];

	const char* user_ix = user_text;
	const char* sent_ix = sent_text;
	char* user_style_ix = user_style;
	char* sent_style_ix = sent_style;
	int number_matched = 0;
	while (*user_ix != '\0' && *sent_ix != '\0') {
		// If we are on a matching streak, check individual characters
		if (number_matched) {
			if (*user_ix == *sent_ix) {
				*user_style_ix++ = STYLE_MATCH;
				*sent_style_ix++ = STYLE_MATCH;
				user_ix++;
				sent_ix++;
				number_matched++;
				continue;
			}
		}
		// Check that we are about to start a matching streak.
		number_matched = 0;
		if (strncmp(user_ix, sent_ix, M) == 0) {
			memset(user_style_ix, STYLE_MATCH, M);
			memset(sent_style_ix, STYLE_MATCH, M);
			user_ix += M;
			sent_ix += M;
			user_style_ix += M;
			sent_style_ix += M;
			number_matched = M;
		}
		else {
			bool match_found = false;
			for (int i = 1; i <= N; i++) {
				if (strncmp(user_ix, sent_ix + i, M) == 0) {
					// Mark the skipped characters in sent as "Error" and 
					// step the sent data by the number of skipped characters.
					memset(sent_style_ix, STYLE_ERROR, i);
					sent_ix += i;
					sent_style_ix += i;
					match_found = true;
					break;
				}
				else if (strncmp(user_ix + i, sent_ix, M) == 0) {
					// Mark the skipped characters in user as "Error" and step
					// the user data by the number of skipped characters.
					memset(user_style_ix, STYLE_ERROR, i);
					user_ix += i;
					user_style_ix += i;
					match_found = true;
					user_ix += i;
					break;
				}
			}
			if (!match_found) {
				// Mark the mismatched character in both as "Error" and step both data by one character.
				*user_style_ix++ = STYLE_ERROR;
				*sent_style_ix++ = STYLE_ERROR;
				user_ix++;
				sent_ix++;
			}
		}
	}
	// If there is remaining text in user, mark it as "Extra".	
	while (*user_ix != '\0') {
		user_ix++;
		*user_style_ix++ = STYLE_ERROR;
	}
	// If there is remaining text in sent, copy it to user and mark it as "Missing".
	while (*sent_ix != '\0') {
		sent_ix++;
		*sent_style_ix++ = STYLE_ERROR;
	}
	// Null-terminate the new user and style strings.
	*user_style_ix = '\0';
	*sent_style_ix = '\0';
	// Update the user display with the new user string and style string.
	user->style_buffer()->text(user_style);
	// Update the sent display with the new style string.
	sent->style_buffer()->text(sent_style);
	user->redraw();
	sent->redraw();
	Fl::check();

	delete[] user_style;
	delete[] sent_style;
}

// Callback if text_editor is modified
void review::cb_modify(
	int pos, int nInserted,
	int nDeleted, int nRestyled,
	const char* deleted_text, void* data
) {
	Fl_Text_Display* td = static_cast<Fl_Text_Display*>(data);
	// If the text was entered, set its style to normal (style 'A').
	if (nInserted > 0) {
		char* style_str = new char[nInserted + 1];
		*std::fill_n(style_str, nInserted, STYLE_NORMAL) = '\0';
		td->style_buffer()->insert(pos, style_str);
		delete[] style_str;
	}
	if (nDeleted > 0) {
		// If the text was deleted, remove the corresponding style characters.
		td->style_buffer()->remove(pos, pos + nDeleted);
	}
}

// Callback to refresh display every 100 ms.
void review::cb_ticker(void* data) {
	review* r = static_cast<review*>(data);
	r->poll_text_queue();
	r->poll_decoded_text();
	// Swap spectrogram buffers if monitor thread has written new data
	if (r->spectrogram_data_ready_.load(std::memory_order_acquire)) {
		std::lock_guard<std::mutex> lock(r->spectrogram_mutex_);
		// Move captured data to display buffer for rendering
		*r->spectrogram_data_display_ = *r->spectrogram_data_capture_;
		r->spectrogram_data_ready_.store(false, std::memory_order_relaxed);
	}
	r->g_sgram_->redraw();
	r->td_decoded_->redraw();
}

// Callback to redraw the review window with latest spectrogram data
void review::cb_redraw(void* data) {
	review* r = static_cast<review*>(data);
	r->spectrogram_->redraw();
}

// Configure the spectrogram based on the current settings.
void review::configure_spectrogram() {
	zc_settings settings;
	int fft_size = DEFAULT_FFT_SIZE;
	settings.get("FFT Size", fft_size, fft_size);
	double overlap = DEFAULT_OVERLAP;
	settings.get("FFT Overlap %", overlap, overlap);
	double max_freq = DEFAULT_MAX_PITCH;
	settings.get("Spectrogram Frequency Span", max_freq, max_freq);
	double max_time = DEFAULT_MAX_TIME;
	settings.get("Spectrogram Time Span", max_time, max_time);

	update_decoder_controls();
	spectrogram_->start_config();
	// Axis 0 - time
	spectrogram_->set_axis_params(0, zc_graph_::SI_PREFIX, "s", "Time");
	zc_graph_::range_t time_range = { 0.0, max_time };
	spectrogram_->set_axis_ranges(0, time_range, time_range, time_range);
	// Axis 1 - frequency
	spectrogram_->set_axis_params(1, zc_graph_::SI_PREFIX, "Hz", "Frequency");
	zc_graph_::range_t freq_range = { 0, max_freq };
	spectrogram_->set_axis_ranges(1, freq_range, freq_range, freq_range);
	// Axis 2 - magnitude
	spectrogram_->set_axis_params(2, zc_graph_::NO_MODIFIER);

	double max_z = static_cast<double>(fft_size);
	zc_graph_::range_t mag_range = { 0.0, static_cast<double>(fft_size) };
	spectrogram_->set_axis_ranges(2, mag_range, mag_range, mag_range);
	spectrogram_data_display_ = new zc_graph_::data_set_dens_t;
	// Set the X-values 
	double time_per_sample = static_cast<double>(fft_size) * (1.0 - overlap * 0.01) / DEFAULT_SAMPLE_RATE;
	size_t num_time_samples = static_cast<size_t>(max_time / time_per_sample);
	spectrogram_data_display_->x_values.resize(num_time_samples);
	double t = 0.0;
	for (size_t ix = 0; ix < num_time_samples; ix++) {
		spectrogram_data_display_->x_values[ix] = t;
		t += time_per_sample;
	}
	// Set the Y-values - these are the frequencies corresponding to each FFT bin.
	double freq_bin = DEFAULT_SAMPLE_RATE / static_cast<double>(fft_size);
	double f = 0.0;
	size_t num_freq_bins = (max_freq / freq_bin) + 1;
	spectrogram_data_display_->y_values.resize(num_freq_bins);
	for (size_t iy = 0; iy < num_freq_bins; iy++) {
		spectrogram_data_display_->y_values[iy] = f;
		f += freq_bin;
	}
	spectrogram_data_display_->z_values.resize(
		spectrogram_data_display_->x_values.size() * 
		spectrogram_data_display_->y_values.size());
	std::vector<Fl_Color> map = { FL_BLACK, FL_RED, FL_YELLOW, FL_GREEN, FL_CYAN, FL_BLUE, FL_MAGENTA, FL_WHITE };
	spectrogram_->add_data_set(2, spectrogram_data_display_, map);
	spectrogram_->end_config();

	// Create capture buffer (copy of display buffer for double-buffering)
	spectrogram_data_capture_ = new zc_graph_::data_set_dens_t(*spectrogram_data_display_);

	// CRITICAL: Set display buffer and callbacks BEFORE starting monitor thread
	// to prevent race condition where thread tries to access uninitialized buffer
	monitor_->set_display_buffer(spectrogram_data_capture_, cb_update_spectrogram, this);
	monitor_->set_decode_callback(cb_decoder_callback, this);
	monitor_->set_monitoring_source(decode_source_);

	// Now it's safe to start the monitor processing thread
	monitor_->start_monitor(max_z);

}

// Update the spectrogram controls to reflect the current settings.
void review::update_decoder_controls() {
	zc_settings settings;
	int fft_size = DEFAULT_FFT_SIZE;
	settings.get("FFT Size", fft_size, fft_size);
	double overlap = DEFAULT_OVERLAP;
	settings.get("FFT Overlap %", overlap, overlap);
	double max_freq = DEFAULT_MAX_PITCH;
	settings.get("Spectrogram Frequency Span", max_freq, max_freq);
	double max_time = DEFAULT_MAX_TIME;
	settings.get("Spectrogram Time Span", max_time, max_time);
	double freq_bin = DEFAULT_SAMPLE_RATE / static_cast<double>(fft_size);
	double time_per_sample = static_cast<double>(fft_size) * (1.0 - overlap * 0.01) / DEFAULT_SAMPLE_RATE;

	int fft_index = 0;
	int temp = fft_size;
	while (temp > 64) {
		fft_index++;
		temp >>= 1;
	}
	ch_fft_size_->add("64");
	ch_fft_size_->add("128");
	ch_fft_size_->add("256");
	ch_fft_size_->add("512");
	ch_fft_size_->add("1024");

	ch_fft_size_->value(fft_index);
	sl_overlap_->value(overlap);
	sl_max_freq_->value(max_freq);
	sl_max_time_->value(max_time);
	char text[10];
	std::snprintf(text, sizeof(text), "%.0f", freq_bin);
	op_freq_bin_->value(text);
	std::snprintf(text, sizeof(text), "%.1f", time_per_sample * 1000.0);
	op_time_slice_->value(text);

	settings.get("Decode Source", decode_source_, audio_source_t::NO_AUDIO);
	if (decode_source_ == audio_source_t::NO_AUDIO) {
		ch_fft_size_->activate();
		sl_overlap_->activate();
		sl_max_freq_->activate();
		sl_max_time_->activate();
		spectrogram_->deactivate();
		btn_compare_decoded_->deactivate();
		td_decoded_->deactivate();
	}
	else {
		ch_fft_size_->deactivate();
		sl_overlap_->deactivate();
		sl_max_freq_->deactivate();
		sl_max_time_->deactivate();
		spectrogram_->activate();
		btn_compare_decoded_->activate();
		td_decoded_->activate();
	}
	ch_decode_source_->value(static_cast<int>(decode_source_));

}

// Callback to update the spectrogram with new data.
// Called from monitor thread - just set flag and request redraw
void review::cb_update_spectrogram(void* data) {
	review* r = static_cast<review*>(data);
	// Signal that new spectrogram data is ready for swap
	r->spectrogram_data_ready_.store(true, std::memory_order_release);
	// Note: redraw() is safe to call from worker threads - it only sets damage flags
	r->spectrogram_->redraw();
}

// Callback to update the decoded text with new data.
// CRITICAL: This is called from monitor's processing thread, NOT the main thread!
// Only queue data and update atomics - do NOT touch FLTK widgets here!
void review::cb_decoder_callback(void* data, const std::string& text) {
	review* r = static_cast<review*>(data);
	r->decoded_text_queue_.push(text);
	// Store freq/WPM in atomics - will be read and displayed on main thread
	double freq = monitor_->get_selected_bin_pitch();
	r->latest_decoded_pitch_.store(freq, std::memory_order_relaxed);
	double wpm = monitor_->get_wpm();
	r->latest_decoded_wpm_.store(wpm, std::memory_order_relaxed);
}

// Add output text to display
void review::poll_text_queue() {
	std::string text;
	while (text_queue_->try_pop(text)) {
		add_sent_text(text, text_source_t::SENT_TEXT);
	}
}

// Set the text queue
void review::add_sent_text_queue(zc_async_queue<std::string>* queue) {
	text_queue_ = queue;
}

// Add decoded text to display
void review::poll_decoded_text() {
	std::string text;
	while (decoded_text_queue_.try_pop(text)) {
		add_sent_text(text, text_source_t::DECODED_TEXT);
	}
	// Safely update FLTK widgets with latest values from monitor thread
	double freq = latest_decoded_pitch_.load(std::memory_order_relaxed);
	double wpm = latest_decoded_wpm_.load(std::memory_order_relaxed);
	if (freq > 0.0 || wpm > 0.0) {
		char temp[10];
		snprintf(temp, sizeof(temp), "%.0f", freq);
		op_decoded_pitch_->value(temp);
		snprintf(temp, sizeof(temp), "%.1f", wpm);
		op_decoded_wpm_->value(temp);
	}
}