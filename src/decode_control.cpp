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
#include "decode_control.hpp"

#include "monitor.hpp"
#include "params.hpp"
#include "review.hpp"

#include "zc_async_queue.h"
#include "zc_fltk.h"
#include "zc_graph_.h"
#include "zc_settings.h"
#include "zc_ticker.h"
#include "zc_wheel_value_slider.h"

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

extern zc_ticker* ticker_;
extern monitor* monitor_;
extern review* review_;
extern double DEFAULT_SAMPLE_RATE;
extern int DEFAULT_FFT_SIZE;
extern double DEFAULT_OVERLAP;
extern double DEFAULT_MAX_PITCH;
extern double DEFAULT_MAX_TIME;

// Constructor
decode_control::decode_control(int W, int H, const char* L) : Fl_Double_Window(W, H, L) {
	// Capture main thread ID for thread safety checks
	main_thread_id_ = std::this_thread::get_id();
	create_widgets();
	ticker_->add_ticker(this, cb_ticker, 1, false);
}

// Destructor
decode_control::~decode_control() {
}

// Create the widgets for the decode_control window.
void decode_control::create_widgets() {
	int cx = GAP;
	int cy = GAP;
	int WDISPLAY = 300;
	int WGROUPS = GAP + WBUTTON + WDISPLAY + GAP;
	int HDISPLAY = 120;
	int HGROUPS = HTEXT + HDISPLAY + GAP;
	int HSGRAMS = std::max(HTEXT + HBUTTON * 10 + GAP, HTEXT + HDISPLAY * 2 + GAP);

	g_sgram_ = new Fl_Group(cx, cy, WGROUPS, HSGRAMS, "Scope views");
	g_sgram_->box(FL_BORDER_BOX);
	g_sgram_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_TOP);

	cx += GAP + WLABEL;
	cy += HTEXT;

	ch_decode_source_ = new Fl_Choice(cx, cy, WBUTTON, HBUTTON, "Source");
	// Add options to the choice menu - only audio decodes
	for (const auto& [source, label] : audio_source_strings_) {
		ch_decode_source_->add(label.c_str());
	}
	ch_decode_source_->align(FL_ALIGN_LEFT);
	ch_decode_source_->callback(cb_decode_source, this);
	cy += HBUTTON;

	ch_fft_size_ = new Fl_Choice(cx, cy, WBUTTON, HBUTTON, "FFT Size");
	ch_fft_size_->align(FL_ALIGN_LEFT);
	ch_fft_size_->callback(cb_ch_fft_size, (void*)this);
	ch_fft_size_->tooltip("Select the appropriate FFT size");

	cy += HBUTTON;
	sl_overlap_ = new zc_wheel_value_slider(cx, cy, WBUTTON, HBUTTON, "Overlap");
	sl_overlap_->type(FL_HOR_SLIDER);
	sl_overlap_->align(FL_ALIGN_LEFT);
	sl_overlap_->callback(cb_slider_overlap, (void*)this);
	sl_overlap_->tooltip("Select the sample overlap %");
	sl_overlap_->range(0.0, 87.5);
	sl_overlap_->step(12.5);

	cy += HBUTTON;
	sl_max_freq_ = new zc_wheel_value_slider(cx, cy, WBUTTON, HBUTTON, "Max Freq");
	sl_max_freq_->type(FL_HOR_SLIDER);
	sl_max_freq_->align(FL_ALIGN_LEFT);
	sl_max_freq_->callback(cb_slider_max_pitch, (void*)this);
	sl_max_freq_->tooltip("Select the maximum frequency displayed");
	sl_max_freq_->range(100.0, DEFAULT_SAMPLE_RATE / 2.0);
	sl_max_freq_->step(100.0);

	cy += HBUTTON;
	sl_max_time_ = new zc_wheel_value_slider(cx, cy, WBUTTON, HBUTTON, "Max Time");
	sl_max_time_->type(FL_HOR_SLIDER);
	sl_max_time_->align(FL_ALIGN_LEFT);
	sl_max_time_->callback(cb_slider_max_time, (void*)this);
	sl_max_time_->tooltip("select the maximum time displayed");
	sl_max_time_->range(0.1, 10);
	sl_max_time_->step(0.05);

	cy += HBUTTON;
	sl_squelch_ = new zc_wheel_value_slider(cx, cy, WBUTTON, HBUTTON, "Squelch (%)");
	sl_squelch_->type(FL_HOR_SLIDER);
	sl_squelch_->align(FL_ALIGN_LEFT);
	sl_squelch_->callback(cb_slider_squelch, (void*)this);
	sl_squelch_->tooltip("Select the squelch level");
	sl_squelch_->range(0.0, 100.0);
	sl_squelch_->step(1.0);

	cy += HBUTTON;
	op_freq_bin_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "Step (Hz)");
	op_freq_bin_->align(FL_ALIGN_LEFT);
	op_freq_bin_->tooltip("Displays the frequency resolution in hertz");

	cy += HBUTTON;
	op_time_slice_ = new Fl_Output(cx, cy, WBUTTON, HBUTTON, "Step (ms)");
	op_time_slice_->align(FL_ALIGN_LEFT);
	op_time_slice_->tooltip("Displays the time resilution in milliseconds");

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

	cy += HDISPLAY;
	waveform_ = new zc_graph_cartesian(cx, cy, WSGRAM, HDISPLAY);

	load_settings();
	configure_spectrogram();

	g_sgram_->end();

	cy = g_sgram_->y() + g_sgram_->h() + GAP;

	end();
	// Resize the window to fit the widgets.
	resizable(nullptr);
	size(GAP + WGROUPS + GAP, cy);

}

// Load settings.
void decode_control::load_settings() {
	zc_settings settings;
	settings.get("Sample Rate", sample_rate_, DEFAULT_SAMPLE_RATE);
	settings.get("Decode Source", decode_source_, audio_source_t::NO_AUDIO);
	ch_decode_source_->value(static_cast<int>(decode_source_));
	if (sl_max_freq_->value() > sample_rate_) sl_max_freq_->value(sample_rate_);
	sl_max_freq_->range(0.0, sample_rate_);
}

// Save settings.
void decode_control::save_settings() const {
	zc_settings settings;
	settings.set("Decode Source", decode_source_);
}

// Check if we're on the main thread, throw exception if not
void decode_control::check_main_thread(const char* method_name) const {
	if (std::this_thread::get_id() != main_thread_id_) {
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg),
			"THREAD SAFETY VIOLATION: %s called from non-main thread!", method_name);
		throw std::runtime_error(error_msg);
	}
}


void decode_control::cb_decode_source(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	r->decode_source_ = static_cast<audio_source_t>(((Fl_Choice*)w)->value());
	zc_settings settings;
	settings.set("Decode Source", r->decode_source_);
	r->update_decoder_controls();
	monitor_->stop_monitor();
	r->configure_spectrogram();
}

// Callback for spectrogram control - FFT size
void decode_control::cb_ch_fft_size(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	Fl_Choice* choice = static_cast<Fl_Choice*>(w);
	int value = choice->value();
	int fft_size = 64 << (value);
	zc_settings settings;
	settings.set("FFT Size", fft_size);
	r->configure_spectrogram();
}

// Callback to set the FFT overlap 
void decode_control::cb_slider_overlap(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	zc_wheel_value_slider* slider = static_cast<zc_wheel_value_slider*>(w);
	double pc_overlap = slider->value();
	zc_settings settings;
	settings.set("FFT Overlap %", pc_overlap);
	r->configure_spectrogram();
}

// Callback to set the display frequency range
void decode_control::cb_slider_max_pitch(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	zc_wheel_value_slider* slider = static_cast<zc_wheel_value_slider*>(w);
	double max_freq = slider->value();
	zc_settings settings;
	settings.set("Spectrogram Frequency Span", max_freq);
	r->configure_spectrogram();
}

// Callback to set the display time range
void decode_control::cb_slider_max_time(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	zc_wheel_value_slider* slider = static_cast<zc_wheel_value_slider*>(w);
	double max_time = slider->value();
	zc_settings settings;
	settings.set("Spectrogram Time Span", max_time);
	r->configure_spectrogram();
}

// Callback to set the squelch level
void decode_control::cb_slider_squelch(Fl_Widget* w, void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	zc_wheel_value_slider* slider = static_cast<zc_wheel_value_slider*>(w);
	double squelch = slider->value();
	zc_settings settings;
	settings.set("Squelch Level", squelch);
	int fft_size = 64 << (r->ch_fft_size_->value());
	double real_squelch = squelch * 0.01 * static_cast<double>(fft_size);
	monitor_->set_squelch_level(real_squelch);
}


// Callback to refresh display every 100 ms.
void decode_control::cb_ticker(void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	// Swap spectrogram buffers if monitor thread has written new data
	if (r->spectrogram_data_ready_.load(std::memory_order_acquire)) {
		std::lock_guard<std::mutex> lock(r->spectrogram_mutex_);
		// Move captured data to display buffer for rendering
		*r->spectrogram_data_display_ = *r->spectrogram_data_capture_;
		// Move captured waveform data to display buffer for rendering
		*r->waveform_data_display_ = *r->waveform_data_capture_;
		r->spectrogram_data_ready_.store(false, std::memory_order_relaxed);
	}
	char text[32]; 
	snprintf(text, sizeof(text), "%.0f", r->latest_decoded_pitch_.load());
	r->op_decoded_pitch_->value(text);
	snprintf(text, sizeof(text), "%.1f", r->latest_decoded_wpm_.load());
	r->op_decoded_wpm_->value(text);
	r->g_sgram_->redraw();
}

// Callback to redraw the decode_control window with latest spectrogram data
void decode_control::cb_redraw(void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	r->spectrogram_->redraw();
	r->waveform_->redraw();
}

// Configure the spectrogram based on the current settings.
void decode_control::configure_spectrogram() {
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
	if (spectrogram_data_display_) delete spectrogram_data_display_;
	spectrogram_data_display_ = new zc_graph_::data_set_dens_t;
	// Set the X-values 
	double time_per_ffts = static_cast<double>(fft_size) * (1.0 - overlap * 0.01) / sample_rate_;
	size_t num_time_ffts = static_cast<size_t>(max_time / time_per_ffts);
	spectrogram_data_display_->x_values.resize(num_time_ffts);
	double t = 0.0;
	for (size_t ix = 0; ix < num_time_ffts; ix++) {
		spectrogram_data_display_->x_values[ix] = t;
		t += time_per_ffts;
	}
	// Set the Y-values - these are the frequencies corresponding to each FFT bin.
	double freq_bin = sample_rate_ / static_cast<double>(fft_size);
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
	// generate a colour map with 16 levels, logarithmic, black at -50 dB.
	zc_graph_::colour_map_t map = { 16, true, -50.0 };
	spectrogram_->add_data_set(2, spectrogram_data_display_, map);
	spectrogram_->end_config();

	// Create capture buffer (copy of display buffer for double-buffering)
	if (spectrogram_data_capture_) delete spectrogram_data_capture_;
	spectrogram_data_capture_ = new zc_graph_::data_set_dens_t(*spectrogram_data_display_);

	// Configure the waveform display
	waveform_->start_config();
	// Axis 0 - time
	waveform_->set_axis_params(0, zc_graph_::SI_PREFIX, "s", "Time");
	waveform_->set_axis_ranges(0, time_range, time_range, time_range);
	// Axis 1 - amplitude
	waveform_->set_axis_params(1, zc_graph_::NO_MODIFIER, "", "Amplitude");
	zc_graph_::range_t amp_range = { -1.0, 1.0 };
	waveform_->set_axis_ranges(1, amp_range, amp_range, amp_range);
	if (waveform_data_display_) delete waveform_data_display_;
	waveform_data_display_ = new std::vector<zc_graph_::data_point_t>;
	// Set the X-values for the waveform display to time and Y-values to zero.
	double time_per_waves = 1.0 / sample_rate_;
	size_t num_wave_samples = static_cast<size_t>(max_time / time_per_waves);
	waveform_data_display_->resize(num_wave_samples);
	t = 0.0;
	for (size_t ix = 0; ix < num_wave_samples; ix++) {
		(*waveform_data_display_)[ix] = { t, 0.0 };
		t += time_per_waves;
	}
	// Remove any existing data.
	waveform_->clear_data_sets();
	// Add the waveform data set to the waveform display.
	waveform_->add_data_set(1, waveform_data_display_, { FL_BLACK, 1, FL_SOLID });
	// End configuration of the waveform display.
	waveform_->end_config();

	// Create a capture buffer (copy of display buffer for double-buffering) for the waveform display.
	if (waveform_data_capture_) delete waveform_data_capture_;
	waveform_data_capture_ = new std::vector<zc_graph_::data_point_t>(*waveform_data_display_);

	// CRITICAL: Set display buffer and callbacks BEFORE starting monitor thread
	// to prevent race condition where thread tries to access uninitialized buffer
	monitor_->set_display_buffer(spectrogram_data_capture_, waveform_data_capture_, cb_update_spectrogram, this);
	monitor_->set_decode_callback(cb_decoder_callback, this);
	monitor_->set_monitoring_source(decode_source_);

	// Now it's safe to start the monitor processing thread
	monitor_->start_monitor(max_z);

}

// Update the spectrogram controls to reflect the current settings.
void decode_control::update_decoder_controls() {
	zc_settings settings;
	int fft_size = DEFAULT_FFT_SIZE;
	settings.get("FFT Size", fft_size, fft_size);
	double overlap = DEFAULT_OVERLAP;
	settings.get("FFT Overlap %", overlap, overlap);
	double max_freq = DEFAULT_MAX_PITCH;
	settings.get("Spectrogram Frequency Span", max_freq, max_freq);
	double max_time = DEFAULT_MAX_TIME;
	settings.get("Spectrogram Time Span", max_time, max_time);
	double freq_bin = sample_rate_ / static_cast<double>(fft_size);
	double time_per_sample = static_cast<double>(fft_size) * (1.0 - overlap * 0.01) / sample_rate_;

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
	}
	else {
		ch_fft_size_->deactivate();
		sl_overlap_->deactivate();
		sl_max_freq_->deactivate();
		sl_max_time_->deactivate();
		spectrogram_->activate();
	}
	ch_decode_source_->value(static_cast<int>(decode_source_));

}

// Callback to update the spectrogram with new data.
// Called from monitor thread - just set flag and request redraw
void decode_control::cb_update_spectrogram(void* data) {
	decode_control* r = static_cast<decode_control*>(data);
	// Signal that new spectrogram data is ready for swap
	r->spectrogram_data_ready_.store(true, std::memory_order_release);
	// Note: redraw() is safe to call from worker threads - it only sets damage flags
	r->spectrogram_->redraw();
	r->waveform_->redraw();
}

// Callback to update the decoded text with new data.
// CRITICAL: This is called from monitor's processing thread, NOT the main thread!
// Only queue data and update atomics - do NOT touch FLTK widgets here!
void decode_control::cb_decoder_callback(void* data, const std::string& text) {
	decode_control* r = static_cast<decode_control*>(data);
	// Store freq/WPM in atomics - will be read and displayed on main thread
	double freq = monitor_->get_selected_bin_pitch();
	r->latest_decoded_pitch_.store(freq, std::memory_order_relaxed);
	double wpm = monitor_->get_wpm();
	r->latest_decoded_wpm_.store(wpm, std::memory_order_relaxed);
	review_->push_decoded_text(text);
}


