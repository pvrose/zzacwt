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
#include "user_if.hpp"

#include "cred_dialog.hpp"
#include "decode_control.hpp"
#include "oscillator.hpp"
#include "mod_mixer.hpp"
#include "noise_gen.hpp"
#include "params.hpp"
#include "review.hpp"
#include "shaper.hpp"
#include "text_gen.hpp"

#include "zc_audio.h"
#include "zc_fltk.h"
#include "zc_settings.h"
#include "zc_wheel_value_slider.h"

#include <FL/Enumerations.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>

#include <algorithm>
#include <map>
#include <string>

// Include platform-specific headers for opening files.
#ifdef _WIN32
#include <Windows.h>
// Undefine min and max macros that may be defined by Windows headers to avoid conflicts with std::min and std::max.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

extern decode_control* decoder_;
extern oscillator* oscillator_;
extern mod_mixer* mod_mixer_;
extern noise_gen* noise_gen_;
extern shaper* shaper_;
extern zc_audio* speaker_;
extern zc_audio* microphone_;
extern text_gen* text_gen_;
extern review* review_;

extern double DEFAULT_RISE_FALL;
extern double DEFAULT_WPM;
extern double MAXIMUM_WPM;
extern double MINIMUM_WPM;
extern void restart_application();

user_if::user_if(int W, int H, const char* L) : Fl_Double_Window(W, H, L)
{
	callback(cb_close, this);
	create_widgets();
}

user_if::~user_if()
{
	// Clean up any resources if necessary (omitted for brevity)
}

void user_if::create_widgets() {
	int cx = GAP;
	int cy = GAP;
	const int WGROUPS = GAP + 3 * WBUTTON + GAP;
	const int WHELP = 2 * GAP + HBUTTON;

	const int WWGROUP = 2 * WGROUPS - WHELP;

	g_audio_ = new Fl_Group(cx, cy, WWGROUP, 150, "Audio");
	g_audio_->box(FL_BORDER_BOX);
	g_audio_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	cx += GAP + WLABEL;
	cy += HTEXT;
	cb_enable_audio_in_ = new Fl_Check_Button(cx, cy, HBUTTON, HBUTTON, "In:");
	cb_enable_audio_in_->value(0); // Default to disabled
	cb_enable_audio_in_->align(FL_ALIGN_LEFT);
	cb_enable_audio_in_->callback(cb_enable_audio_in, this);
	cb_enable_audio_in_->tooltip("Enable audio input");

	cx += HBUTTON;
	ch_audio_in_device_ = new Fl_Choice(cx, cy, WEDIT, HBUTTON);
	ch_audio_in_device_->tooltip("Select the audio input device");

	cx += ch_audio_in_device_->w();
	ch_sample_rate_ = new Fl_Choice(cx, cy, WBUTTON, HBUTTON, "Sample Rate");
	ch_sample_rate_->align(FL_ALIGN_TOP);
	ch_sample_rate_->callback(cb_sample_rate, this);
	ch_sample_rate_->tooltip("Select the desired sample-rate");

	cx = g_audio_->x() + GAP + WLABEL;
	cy += HBUTTON;

	cb_enable_audio_out_ = new Fl_Check_Button(cx, cy, HBUTTON, HBUTTON, "Out:");
	cb_enable_audio_out_->value(1); // Default to enabled
	cb_enable_audio_out_->align(FL_ALIGN_LEFT);
	cb_enable_audio_out_->callback(cb_enable_audio_out, this);
	cb_enable_audio_out_->tooltip("Enable audio output");

	cx += HBUTTON;
	ch_audio_out_device_ = new Fl_Choice(cx, cy, WEDIT, HBUTTON);
	ch_audio_out_device_->tooltip("Select the audio output device");

	cy += HBUTTON + GAP;
	// End the audio group
	g_audio_->end();
	// Resize the audio group to fit the controls
	g_audio_->resizable(nullptr);
	int audio_height = cy - g_audio_->y();
	g_audio_->size(g_audio_->w(), audio_height);

	cy = g_audio_->y();
	cx = g_audio_->x() + g_audio_->w() + GAP;

	g_help_ = new Fl_Group(cx, cy, WHELP, 150, "Help");
	g_help_->box(FL_BORDER_BOX);
	g_help_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);
	cx += GAP;
	cy += HTEXT;

	bt_help_html_ = new Fl_Button(cx, cy, HBUTTON, HBUTTON);
	bt_help_html_->callback(cb_open_html, this);
	bt_help_html_->tooltip("Open the help documentation as HTML");
	std::string fn_html = file_holder_->get_filename(FILE_ICON_ZZA);
	Fl_PNG_Image* img_html = new Fl_PNG_Image(fn_html.c_str());
	Fl_Image* icon = img_html->copy(HBUTTON, HBUTTON);
	bt_help_html_->image(icon);

	cy += HBUTTON;
	bt_help_pdf_ = new Fl_Button(cx, cy, HBUTTON, HBUTTON);
	bt_help_pdf_->callback(cb_open_pdf, this);
	bt_help_pdf_->tooltip("Open the help documentation as PDF");
	std::string fn_pdf = file_holder_->get_filename(FILE_ICON_PDF);
	Fl_PNG_Image* img_pdf = new Fl_PNG_Image(fn_pdf.c_str());
	Fl_Image* icon_pdf = img_pdf->copy(HBUTTON, HBUTTON);
	bt_help_pdf_->image(icon_pdf);

	cy += HBUTTON + GAP;

	// End the help group
	g_help_->end();
	// Resize the audio group to fit the controls
	g_help_->resizable(nullptr);
	int help_height = cy - g_help_->y();
	g_help_->size(g_help_->w(), help_height);

	cx = g_audio_->x();
	cy = std::max(g_audio_->y() + g_audio_->h(), g_help_->y() + g_help_->h()) + GAP;

	g_content_ = new Fl_Group(cx, cy, WGROUPS, 150, "Content");
	g_content_->box(FL_BORDER_BOX);
	g_content_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	cx += GAP + WBUTTON;
	cy += HTEXT;

	// Create the mode choice control
	ch_mode_ = new Fl_Choice(cx, cy, WSMEDIT, HBUTTON, "Mode:");
	// Populate the mode choice with the content mode strings
	for (const auto& [mode, name] : content_mode_strings_) {
		ch_mode_->add(name.c_str());
	}
	ch_mode_->callback(cb_mode, this);
	ch_mode_->tooltip("Select the type of content to practice");
	ch_mode_->align(FL_ALIGN_LEFT);

	cy += HBUTTON;
	// Create the Block size slider
	sl_tx_size_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Block Size:");
	sl_tx_size_->type(FL_HOR_SLIDER);
	sl_tx_size_->bounds(1, 100);
	sl_tx_size_->step(1.0);
	sl_tx_size_->callback(cb_tx_size, this);
	sl_tx_size_->tooltip("Adjust the block size - number of groups or words");
	sl_tx_size_->align(FL_ALIGN_LEFT);
	sl_tx_size_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	// Create the user text input (only active in USER_TEXT mode)
	in_text_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "User Text:");
	in_text_->callback(cb_text, this);
	in_text_->tooltip("Enter custom text for practice");
	in_text_->align(FL_ALIGN_LEFT);

	cy += HBUTTON;
	// Create the customise button for QSO generation (only active in QSO mode)
	bt_customise_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Customise");
	bt_customise_->callback(cb_customise, this);
	bt_customise_->tooltip("Customise QSO generation - enter your callsign and location");


	cy += HBUTTON + GAP;

	// End the content group
	g_content_->end();
	// Resize the content group to fit the controls
	g_content_->resizable(nullptr);
	int content_height = cy - g_content_->y();
	g_content_->size(g_content_->w(), content_height);

	// Create the speed controls group
	cx = GAP;
	cy += GAP;
	g_speed_ = new Fl_Group(cx, cy,	WGROUPS, 150, "Speed");
	g_speed_->box(FL_BORDER_BOX);
	g_speed_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	// Create speed type choice
	cx += GAP + WBUTTON;
	cy += HTEXT;
	ch_speed_type_ = new Fl_Choice(cx, cy, WSMEDIT, HBUTTON, "Type:");
	for (const auto& [type, name] : speed_type_strings_) {
		ch_speed_type_->add(name.c_str());
	}
	ch_speed_type_->callback(cb_speed_type, this);
	ch_speed_type_->tooltip("Select the speed type for practice:");
	
	cy += HBUTTON;
	// Create dot speed WPM slider
	sl_dot_speed_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Dot Speed:");
	sl_dot_speed_->type(FL_HOR_SLIDER);
	sl_dot_speed_->bounds(MINIMUM_WPM, MAXIMUM_WPM);
	sl_dot_speed_->step(1.0);
	sl_dot_speed_->callback(cb_wpm, this);
	sl_dot_speed_->tooltip("Adjust the dot speed");
	sl_dot_speed_->align(FL_ALIGN_LEFT);
	sl_dot_speed_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	// Create Overall WPM slider
	sl_overall_speed_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Overall WPM:");
	sl_overall_speed_->type(FL_HOR_SLIDER);
	sl_overall_speed_->bounds(MINIMUM_WPM, MAXIMUM_WPM);
	sl_overall_speed_->step(1.0);
	sl_overall_speed_->callback(cb_overall, this);
	sl_overall_speed_->tooltip("Overall: adds spacing between letters or words to achieve target WPM");
	sl_overall_speed_->align(FL_ALIGN_LEFT);
	sl_overall_speed_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON + GAP;
	// End the speed group
	g_speed_->end();
	// Resize the speed group to fit the controls
	g_speed_->resizable(nullptr);
	int speed_height = cy - g_speed_->y();
	g_speed_->size(g_speed_->w(), speed_height);

	cy += GAP;
	cx = GAP;

	// Create the tone controls group
	g_tone_ = new Fl_Group(cx, cy, WGROUPS, 100, "Tone");
	g_tone_->box(FL_BORDER_BOX);
	g_tone_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	cx = g_tone_->x() + GAP + WBUTTON;
	cy += HTEXT;
	// Create volume slider
	sl_volume_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Volume (dB):");
	sl_volume_->type(FL_HOR_SLIDER);
	sl_volume_->bounds(-40, 0);
	sl_volume_->step(1.0);
	sl_volume_->callback(cb_volume, this);
	sl_volume_->tooltip("Adjust the tone volume");
	sl_volume_->align(FL_ALIGN_LEFT);
	sl_volume_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	// Create pitch slider
	sl_pitch_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Pitch (Hz):");
	sl_pitch_->type(FL_HOR_SLIDER);
	sl_pitch_->bounds(350, 1400);
	sl_pitch_->step(1.0);
	sl_pitch_->callback(cb_pitch, this);
	sl_pitch_->tooltip("Adjust the tone pitch");
	sl_pitch_->align(FL_ALIGN_LEFT);
	sl_pitch_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON + GAP;
	// End the tone group
	g_tone_->end();
	// Resize the tone group to fit the controls
	g_tone_->resizable(nullptr);
	int tone_height = cy - g_tone_->y();
	g_tone_->size(g_tone_->w(), tone_height);

	int needed_height = g_tone_->y() + tone_height + GAP;

	cy = g_content_->y();
	cx = g_content_->x() + g_content_->w() + GAP;

	// Create the controls group for play/stop/repeat buttons
	g_controls_ = new Fl_Group(cx, cy, WGROUPS, 100, "Controls");
	g_controls_->box(FL_BORDER_BOX);
	g_controls_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	cx += GAP;
	cy += HTEXT;
	bt_new_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "New");
	bt_new_->callback(cb_new, this);
	bt_new_->tooltip("Start a new session");

	cx += WBUTTON;
	bt_stop_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Clear");
	bt_stop_->callback(cb_stop, this);
	bt_stop_->tooltip("Clear the current session");

	cx += WBUTTON;
	bt_repeat_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "Repeat");
	bt_repeat_->callback(cb_repeat, this);
	bt_repeat_->tooltip("Repeat the current session");

	cy += HBUTTON + GAP;

	// End the controls group
	g_controls_->end();
	// Resize the controls group to fit the buttons
	g_controls_->resizable(nullptr);
	int controls_height = cy - g_controls_->y();
	g_controls_->size(g_controls_->w(), controls_height);

	cy += GAP;
	cx = g_content_->x() + g_content_->w() + GAP;

	// Create the disturber controls group
	g_disturber_ = new Fl_Group(cx, cy, WGROUPS, 200, "Disturbers");
	g_disturber_->box(FL_BORDER_BOX);
	g_disturber_->align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE);

	cy += HTEXT;
	cx += GAP + WBUTTON;

	// Create disturber type choice
	ch_disturber_type_ = new Fl_Choice(cx, cy, WSMEDIT, HBUTTON, "Type:");
	for (const auto& [type, name] : disturber_type_strings_) {
		ch_disturber_type_->add(name.c_str());
	}
	ch_disturber_type_->callback(cb_disturber_type, this);
	ch_disturber_type_->tooltip("Select the type of disturbance");

	// Create sliders for different disturber parameters (timing, softness, noise volume/frequency, drift rate/period)
	cy += HBUTTON;
	sl_timing_dist_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Timing:");
	sl_timing_dist_->type(FL_HOR_SLIDER);
	sl_timing_dist_->bounds(0, 5);
	sl_timing_dist_->step(1.0);
	sl_timing_dist_->callback(cb_timing_dist, this);
	sl_timing_dist_->tooltip("Adds variation to the dit-timing: 0 - none, 5 - -10%~+25%");
	sl_timing_dist_->align(FL_ALIGN_LEFT);
	sl_timing_dist_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_softness_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Softness:");
	sl_softness_->type(FL_HOR_SLIDER);
	sl_softness_->bounds(-(DEFAULT_RISE_FALL * 1000.0F), 3 * (DEFAULT_RISE_FALL * 1000.0F));
	sl_softness_->step(1.0);
	sl_softness_->callback(cb_softness, this);
	sl_softness_->tooltip("Adjusts the rise and fall time (in ms) of the morse tone: normal = 5 ms");
	sl_softness_->align(FL_ALIGN_LEFT);
	sl_softness_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_noise_vol_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Volume (dB):");
	sl_noise_vol_->type(FL_HOR_SLIDER);
	sl_noise_vol_->bounds(-40, 0);
	sl_noise_vol_->step(1.0);
	sl_noise_vol_->callback(cb_noise_vol, this);
	sl_noise_vol_->tooltip("Adjusts the volume of introduced noise");
	sl_noise_vol_->align(FL_ALIGN_LEFT);
	sl_noise_vol_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_noise_severity_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Severity:");
	sl_noise_severity_->type(FL_HOR_SLIDER);
	sl_noise_severity_->bounds(0, 100);
	sl_noise_severity_->step(1.0);
	sl_noise_severity_->callback(cb_noise_severity, this);
	sl_noise_severity_->tooltip("Adjusts the severity of impact noise or inserted tones");
	sl_noise_severity_->align(FL_ALIGN_LEFT);
	sl_noise_severity_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_drift_rate_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Drift (%/s):");
	sl_drift_rate_->type(FL_HOR_SLIDER);
	sl_drift_rate_->bounds(-5, 5);
	sl_drift_rate_->step(0.2);
	sl_drift_rate_->callback(cb_drift_rate, this);
	sl_drift_rate_->tooltip("Adjusts the rate at which the tone drifts (Hz/s)");
	sl_drift_rate_->align(FL_ALIGN_LEFT);
	sl_drift_rate_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_drift_amplitude_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Drift Ampl:");
	sl_drift_amplitude_->type(FL_HOR_SLIDER);
	sl_drift_amplitude_->bounds(0, 50);
	sl_drift_amplitude_->step(1.0);
	sl_drift_amplitude_->callback(cb_drift_amplitude, this);
	sl_drift_amplitude_->tooltip("Adjusts the amplitude of the frequency drift");
	sl_drift_amplitude_->align(FL_ALIGN_LEFT);
	sl_drift_amplitude_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_drift_period_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "Drift Period:");
	sl_drift_period_->type(FL_HOR_SLIDER);
	sl_drift_period_->bounds(1.0, 10.0);
	sl_drift_period_->step(0.1);
	sl_drift_period_->callback(cb_drift_period, this);
	sl_drift_period_->tooltip("Adjusts the period over which the tone drifts");
	sl_drift_period_->align(FL_ALIGN_LEFT);
	sl_drift_period_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_fading_depth_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "QSB Depth:");	
	sl_fading_depth_->type(FL_HOR_SLIDER);
	sl_fading_depth_->bounds(0, 1.0);
	sl_fading_depth_->step(0.01);
	sl_fading_depth_->callback(cb_fading_depth, this);
	sl_fading_depth_->tooltip("Adjusts the depth of fading (0 = no fading, 1 = full fade out)");
	sl_fading_depth_->align(FL_ALIGN_LEFT);
	sl_fading_depth_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON;
	sl_fading_period_ = new zc_wheel_value_slider(cx, cy, WSMEDIT, HBUTTON, "QSB Period:");
	sl_fading_period_->type(FL_HOR_SLIDER);
	sl_fading_period_->bounds(0.1, 10.0);
	sl_fading_period_->step(0.1);
	sl_fading_period_->callback(cb_fading_period, this);
	sl_fading_period_->tooltip("Adjusts the period of fading (in seconds)");
	sl_fading_period_->align(FL_ALIGN_LEFT);
	sl_fading_period_->textsize(DEFAULT_SIZE - 2);

	cy += HBUTTON + GAP;

	// End the disturber group
	g_disturber_->end();
	// Resize the disturber group to fit the controls
	g_disturber_->resizable(nullptr);
	int disturber_height = cy - g_disturber_->y();
	g_disturber_->size(g_disturber_->w(), disturber_height);

	
	// Resize the main window to fit all controls
	resizable(nullptr);
	int total_height = std::max(needed_height, cy) + GAP;
	int total_width = 3 * GAP + 2 * WGROUPS;
	size(total_width, total_height);

	// Initialize widget values from settings
	update_audio_widgets();
	update_content_widgets();
	update_speed_widgets();
	update_disturber_widgets();
	update_tone_widgets();
}

// Methods to update the state of the user interface controls based on the current settings
void user_if::update_content_widgets() {
	zc_settings settings;

	// Get the current content mode from settings and set the choice index
	content_mode current_mode;
	settings.get("Content Mode", current_mode, content_mode::LETTERS);
	ch_mode_->value(static_cast<int>(current_mode));

	// Get the current block size from settings
	int tx_size;
	settings.get("Block Size", tx_size, 50);
	sl_tx_size_->value(tx_size);

	if (current_mode == content_mode::USER_TEXT) {
		in_text_->activate();
		sl_tx_size_->deactivate();
	}
	else {
		in_text_->deactivate();
		sl_tx_size_->activate();
	}
}

void user_if::update_speed_widgets() {
	zc_settings settings;

	// Get the current speed type from settings
	speed_type current_speed_type;
	settings.get("Speed Type", current_speed_type, speed_type::NORMAL);
	ch_speed_type_->value(static_cast<int>(current_speed_type));

	// Get the current dot speed from settings
	double dot_speed;
	settings.get("Dot Speed", dot_speed, DEFAULT_WPM);
	if (dot_speed < MINIMUM_WPM) dot_speed = MINIMUM_WPM;
	if (dot_speed > MAXIMUM_WPM) dot_speed = MAXIMUM_WPM;

	sl_dot_speed_->value(dot_speed);
	settings.set("Dot Speed", dot_speed);

	// Get the current Overall WPM setting
	double overall_speed;
	settings.get("Overall WPM", overall_speed, DEFAULT_WPM);
	if (current_speed_type == speed_type::NORMAL) {
		overall_speed = dot_speed; // If not in Farnsworth mode, set Overall WPM to dot speed
	}
	// Restrict the Overall WPM value to be between 6 and dot speed.
	if (overall_speed < MINIMUM_WPM) {
		overall_speed = MINIMUM_WPM;
	}
	else if (overall_speed > dot_speed) {
		overall_speed = dot_speed;
	}
	sl_overall_speed_->value(overall_speed);
	settings.set("Overall WPM", overall_speed);

	// Depending on the speed type, enable/disable the Farnsworth and Wordsworth sliders
	switch (current_speed_type) {
	case speed_type::NORMAL:
		sl_overall_speed_->deactivate();
        break;
	case speed_type::FARNSWORTH:
	case speed_type::WORDSWORTH:
		sl_overall_speed_->activate();
		break;
	default:
		sl_overall_speed_->deactivate();
		break;
	}
}

void user_if::update_disturber_widgets() {
	zc_settings settings;

	// Get the current disturber type from settings
	disturber_type current_disturber_type;
	settings.get("Disturber Type", current_disturber_type, disturber_type::NONE);
	ch_disturber_type_->value(static_cast<int>(current_disturber_type));

	// Get timing disturbance setting
	int timing_dist;
	settings.get("Timing Disturbance", timing_dist, 0);
	sl_timing_dist_->value(timing_dist);

	// Get softness setting
	double softness;
	settings.get<double>("Softness", softness, (DEFAULT_RISE_FALL * 1000.0F));
	sl_softness_->value(softness);

	// Get noise volume setting
	int noise_vol;
	settings.get("Noise Volume", noise_vol, -40);
	sl_noise_vol_->value(noise_vol);

	// Get noise severity setting
	int noise_severity;
	settings.get("Noise Severity", noise_severity, 0);
	if (noise_severity < 0) noise_severity = 0;
	else if (noise_severity > 100) noise_severity = 100;
	settings.set("Noise Severity", noise_severity);
	sl_noise_severity_->value(noise_severity);

	// Get drift rate setting
	double drift_rate;
	settings.get("Drift Rate", drift_rate, 0.0);
	sl_drift_rate_->value(drift_rate);

	// Get drift amplitude setting
	int drift_amplitude;
	settings.get("Drift Amplitude", drift_amplitude, 0);
	if (drift_amplitude < 0) drift_amplitude = 0;
	else if (drift_amplitude > 50) drift_amplitude = 50;
	sl_drift_amplitude_->value(drift_amplitude);
	settings.set("Drift Amplitude", drift_amplitude);

	// Get drift period setting
	double drift_period;
	settings.get("Drift Period", drift_period, 1.0);
	if (drift_period < 1.0) drift_period = 1.0;
	if (drift_period > 10.0) drift_period = 10.0;
	sl_drift_period_->value(drift_period);
	settings.set("Drift Period", drift_period);

	// Get fading depth setting
	double fading_depth;
	settings.get("Fading Depth", fading_depth, 0.0);
	if (fading_depth < 0.0) fading_depth = 0.0;
	else if (fading_depth > 1.0) fading_depth = 1.0;
	sl_fading_depth_->value(fading_depth);
	settings.set("Fading Depth", fading_depth);

	// Get fading period setting
	double fading_period;
	settings.get("Fading Period", fading_period, 1.0);
	if (fading_period < 0.1) fading_period = 0.1;
	else if (fading_period > 10.0) fading_period = 10.0;
	sl_fading_period_->value(fading_period);
	settings.set("Fading Period", fading_period);

	// Depending on the disturber type, enable/disable the relevant sliders
	switch (current_disturber_type) {
	case disturber_type::NONE:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::TIMING:
		sl_timing_dist_->activate();
		sl_softness_->deactivate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::SOFTNESS:
		sl_timing_dist_->deactivate();
		sl_softness_->activate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::NOISE_WHITE:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->activate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::NOISE_IMPACT:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->activate();
		sl_noise_severity_->activate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::NOISE_TONES:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->activate();
		sl_noise_severity_->activate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::DRIFT_STEADY:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->activate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::DRIFT_CYCLIC:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->activate();
		sl_drift_period_->activate();
		sl_fading_depth_->deactivate();
		sl_fading_period_->deactivate();
		break;
	case disturber_type::FADING:
		sl_timing_dist_->deactivate();
		sl_softness_->deactivate();
		sl_noise_vol_->deactivate();
		sl_noise_severity_->deactivate();
		sl_drift_rate_->deactivate();
		sl_drift_amplitude_->deactivate();
		sl_drift_period_->deactivate();
		sl_fading_depth_->activate();
		sl_fading_period_->activate();
		break;
	}
}

void user_if::update_tone_widgets() {
	zc_settings settings;

	// Get the current volume from settings
	int volume;
	settings.get("Volume (dB)", volume, 0);
	sl_volume_->value(volume);

	// Get the current pitch from settings
	int pitch;
	settings.get("Pitch (Hz)", pitch, 700);
	sl_pitch_->value(pitch);
}

void user_if::update_audio_widgets() {
	zc_settings settings;
	double sample_rate;
	settings.get("Sample Rate", sample_rate, DEFAULT_SAMPLE_RATE);
	// Get the current audio input enabled state from settings
	bool audio_in_enabled;
	settings.get("Audio Input Enabled", audio_in_enabled, false);
	cb_enable_audio_in_->value(audio_in_enabled ? 1 : 0);
	// Get the current audio output enabled state from settings
	bool audio_out_enabled;
	settings.get("Audio Output Enabled", audio_out_enabled, false);
	cb_enable_audio_out_->value(audio_out_enabled ? 1 : 0);
	std::string selected_input_device;
	settings.get<std::string>("Audio Input Device", selected_input_device, "");
	// Populate the audio input device choice with available devices
#ifdef _WIN32
	std::string selected_audio_host = "Windows DirectSound";
#else
	std::string selected_audio_host = "ALSA";
#endif
	settings.get<std::string>("Audio Manager", selected_audio_host, selected_audio_host);
	ch_audio_in_device_->clear();
	auto input_devices = microphone_->get_ports();
	int i = 0;
	for (const auto& device : input_devices) {
		if (device.audio_host == selected_audio_host) {
			ch_audio_in_device_->add(device.port_name.c_str());
			if (device.port_name == selected_input_device) {
				ch_audio_in_device_->value(i);
				microphone_port_ = { selected_audio_host, selected_input_device };
			}
			i++;
		}
	}
	// Populate the audio output device choice with available devices
	std::string selected_output_device;
	settings.get<std::string>("Audio Output Device", selected_output_device, "");
	ch_audio_out_device_->clear();
	auto output_devices = speaker_->get_ports();
	i = 0;
	for (const auto& device : output_devices) {
		if (device.audio_host == selected_audio_host) {
			ch_audio_out_device_->add(device.port_name.c_str());
			if (device.port_name == selected_output_device) {
				ch_audio_out_device_->value(i);
				speaker_port_ = { selected_audio_host, selected_output_device };
			}
			i++;
		}
	}
	// populate the sample rate.
	ch_sample_rate_->clear();
	i = 0;
	unsigned int irate = static_cast<unsigned int>(sample_rate);
	char text[10];
	for (const auto& rate : sample_rates_) {
		snprintf(text, sizeof(text), "%d", rate);
		ch_sample_rate_->add(text);
		if (sample_rate == rate) {
			ch_sample_rate_->value(i);
		}
		i++;
	}
	if (audio_out_enabled) {
		ch_audio_out_device_->deactivate();
	}
	else {
		ch_audio_out_device_->activate();
	}
	if (audio_in_enabled) {
		ch_audio_in_device_->deactivate();
	}
	else {
		ch_audio_in_device_->activate();
	}
	if (audio_out_enabled && audio_in_enabled) {
		ch_sample_rate_->deactivate();
	}
	else {
		ch_sample_rate_->activate();
	}
}	

// Apply settings methods

void user_if::apply_oscillator_settings()
{
	oscillator_->apply_settings();
}

void user_if::apply_shaper_settings()
{
	shaper_->apply_settings();
}

void user_if::apply_noise_settings()
{
	noise_gen_->apply_settings();
}

void user_if::apply_speaker_settings()
{
	if (cb_enable_audio_out_->value()) {
		if (!speaker_->enabled()) {
    		speaker_->use_port(speaker_port_);
		}
	}
	else {
		if (speaker_->enabled()) speaker_->disconnect_port();
	}
}

void user_if::apply_microphone_settings()
{
	if (cb_enable_audio_in_->value()) {
		microphone_->use_port(microphone_port_);
	}
	else {
		if (microphone_->enabled()) microphone_->disconnect_port();
	}
}

// Callback implementations (placeholders)

void user_if::cb_mode(Fl_Widget* w, void* data)
{
	// Save the selected content mode to settings and update the UI
	Fl_Choice* ch = static_cast<Fl_Choice*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int mode_index = ch->value();
	if (mode_index >= 0 && mode_index < static_cast<int>(content_mode::COUNT)) {
		content_mode selected_mode = static_cast<content_mode>(mode_index);
		settings.set("Content Mode", selected_mode);
		ui->update_content_widgets();
		if (selected_mode == content_mode::TEST_MODE_A) {
			// Automatically start test mode A when selected. 
			text_gen_->generate_new_sequence();
		}
	}
}

void user_if::cb_tx_size(Fl_Widget* w, void* data)
{
	// Save the block size to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int tx_size = static_cast<int>(sl->value());
	settings.set("Block Size", tx_size);
	ui->update_content_widgets();
}

void user_if::cb_text(Fl_Widget* w, void* data)
{
	// Save the user text to settings
	Fl_Input* in = static_cast<Fl_Input*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	const char* text = in->value();
	if (text) {
		settings.set("User Text", std::string(text));
	}
	ui->update_content_widgets();
}

void user_if::cb_speed_type(Fl_Widget* w, void* data)
{
	// Save the selected speed type to settings and update the UI
	Fl_Choice* ch = static_cast<Fl_Choice*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int type_index = ch->value();
	if (type_index >= 0 && type_index < static_cast<int>(speed_type::COUNT)) {
		speed_type selected_type = static_cast<speed_type>(type_index);
		settings.set("Speed Type", selected_type);
		ui->update_speed_widgets();
		ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect speed type changes
	}
}

void user_if::cb_wpm(Fl_Widget* w, void* data)
{
	// Save the dot speed WPM value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int wpm = static_cast<int>(sl->value());
	settings.set("Dot Speed", wpm);
	ui->update_speed_widgets();
	ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect WPM changes
}

void user_if::cb_overall(Fl_Widget* w, void* data)
{
	// Save the overall WPM value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int overall = static_cast<int>(sl->value());
	settings.set("Overall WPM", overall);
	ui->update_speed_widgets();
	ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect Farnsworth changes
}

void user_if::cb_disturber_type(Fl_Widget* w, void* data)
{
	// Save the selected disturber type to settings and update the UI
	Fl_Choice* ch = static_cast<Fl_Choice*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int type_index = ch->value();
	if (type_index >= 0 && type_index < static_cast<int>(disturber_type::COUNT)) {
		disturber_type selected_type = static_cast<disturber_type>(type_index);
		settings.set("Disturber Type", selected_type);
		ui->update_disturber_widgets();
		ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect disturber type changes
		ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect disturber type changes
		ui->apply_noise_settings(); // Apply noise settings immediately to reflect disturber type changes
	}
}

void user_if::cb_timing_dist(Fl_Widget* w, void* data)
{
	// Save the timing disturbance value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int timing_dist = static_cast<int>(sl->value());
	settings.set("Timing Disturbance", timing_dist);
	ui->update_disturber_widgets();
	ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect timing disturbance changes
}

void user_if::cb_softness(Fl_Widget* w, void* data)
{
	// Save the softness value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int softness = static_cast<int>(sl->value());
	settings.set("Softness", softness);
	ui->update_disturber_widgets();
	ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect softness changes
}

void user_if::cb_noise_vol(Fl_Widget* w, void* data)
{
	// Save the noise volume value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int noise_vol = static_cast<int>(sl->value());
	settings.set("Noise Volume", noise_vol);
	ui->update_disturber_widgets();
	ui->apply_noise_settings(); // Apply noise settings immediately to reflect noise volume changes
}

void user_if::cb_noise_severity(Fl_Widget* w, void* data)
{
	// Save the noise severity value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int noise_severity = static_cast<int>(sl->value());
	settings.set("Noise Severity", noise_severity);
	ui->update_disturber_widgets();
	ui->apply_noise_settings(); // Apply noise settings immediately to reflect noise severity changes
}

void user_if::cb_drift_rate(Fl_Widget* w, void* data)
{
	// Save the drift rate value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	double drift_rate = sl->value();
	settings.set("Drift Rate", drift_rate);
	ui->update_disturber_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect drift rate changes
}

void user_if::cb_drift_amplitude(Fl_Widget* w, void* data)
{
	// Save the drift amplitude value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int drift_amplitude = static_cast<int>(sl->value());
	settings.set("Drift Amplitude", drift_amplitude);
	ui->update_disturber_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect drift rate changes
}

void user_if::cb_drift_period(Fl_Widget* w, void* data)
{
	// Save the drift period value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	double drift_period = sl->value();
	settings.set("Drift Period", drift_period);
	ui->update_disturber_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect drift period changes
}

void user_if::cb_fading_depth(Fl_Widget* w, void* data)
{
	// Save the fading depth value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	double fading_depth = sl->value();
	settings.set("Fading Depth", fading_depth);
	ui->update_disturber_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect fading depth changes
}

void user_if::cb_fading_period(Fl_Widget* w, void* data)
{
	// Save the fading period value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	double fading_period = sl->value();
	settings.set("Fading Period", fading_period);
	ui->update_disturber_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect fading period changes
}

void user_if::cb_default(Fl_Widget* w, void* data)
{
	// Reset disturber settings to defaults
	(void)w;
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	settings.set("Disturber Type", disturber_type::NONE);
	settings.set("Timing Disturbance", 0);
	settings.set("Softness", DEFAULT_RISE_FALL * 1000.0);
	settings.set("Noise Volume", -40);
	settings.set("Noise Severity", 0);
	settings.set("Drift Rate", 0);
	settings.set("Drift Period", 0);
	ui->update_disturber_widgets();
	ui->apply_shaper_settings(); // Apply shaper settings immediately to reflect disturber reset
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect disturber reset
	ui->apply_noise_settings(); // Apply noise settings immediately to reflect disturber reset
}

void user_if::cb_volume(Fl_Widget* w, void* data)
{
	// Save the volume value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int volume = static_cast<int>(sl->value());
	settings.set("Volume (dB)", volume);
	ui->update_tone_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect volume changes
}

void user_if::cb_pitch(Fl_Widget* w, void* data)
{
	// Save the pitch value to settings
	Fl_Value_Slider* sl = static_cast<Fl_Value_Slider*>(w);
	user_if* ui = static_cast<user_if*>(data);
	zc_settings settings;
	int pitch = static_cast<int>(sl->value());
	settings.set("Pitch (Hz)", pitch);
	ui->update_tone_widgets();
	ui->apply_oscillator_settings(); // Apply oscillator settings immediately to reflect pitch changes
}

void user_if::cb_new(Fl_Widget* w, void* data)
{
	// Start a new session
	(void)w;
	user_if* ui = static_cast<user_if*>(data);
	review_->clear_all_displays();
	ui->apply_noise_settings();
	ui->apply_shaper_settings();
	ui->apply_oscillator_settings();
	ui->apply_speaker_settings();
	text_gen_->generate_new_sequence();
}

void user_if::cb_stop(Fl_Widget* w, void* data)
{
	// Stop the current session
	(void)w;
	user_if* ui = static_cast<user_if*>(data);
	text_gen_->stop_sequence(); // Stop the text generator to stop any ongoing sequence
	mod_mixer_->clear(); // Clear the mod mixer to stop any ongoing modulation
}

void user_if::cb_repeat(Fl_Widget* w, void* data)
{
	// Repeat the current session
	(void)w;
	user_if* ui = static_cast<user_if*>(data);
	review_->clear_all_displays();
	ui->apply_noise_settings();
	ui->apply_shaper_settings();
	ui->apply_oscillator_settings();
	ui->apply_speaker_settings();
	text_gen_->repeat_sequence();
}

// Callback on close - close all windows and exit the application
void user_if::cb_close(Fl_Widget* w, void* data)
{
	(void)w;
	(void)data;
	// Hide the review window if it's open
	if (review_) {
		review_->hide();
	}
	// Hide the decoder if it's open
	if (decoder_) {
		decoder_->hide();
	}
	// Call default close behavior to close the main window and exit the application
	Fl_Window::default_callback((Fl_Window*)w, data);
}

// Callback for the customise button in QSO mode - opens a dialog to enter callsign and location
void user_if::cb_customise(Fl_Widget* w, void* data)
{
	(void)w;
	user_if* ui = static_cast<user_if*>(data);
	cred_dialog* dialog = new cred_dialog(400, 300, "Customise QSO");
	dialog->show();
	// Create existing callsign and location variables to pass to the dialog
	std::map<std::string, std::string> existing_credentials = text_gen_->get_qso_user_macros();
	dialog->set_credentials(existing_credentials);
	while (dialog->visible()) {
		Fl::wait();
	}
	text_gen_->set_qso_user_macros(dialog->get_credentials());
	delete dialog;
}

// Callback for the enable audio input checkbox - enables or disables audio input and saves the setting
void user_if::cb_enable_audio_in(Fl_Widget* w, void* data)
{
	user_if* ui = static_cast<user_if*>(data);
	Fl_Check_Button* cb = static_cast<Fl_Check_Button*>(w);
	zc_settings settings;
	if (cb->value()) {
		const char* device = ui->ch_audio_in_device_->text();
		ui->microphone_port_.port_name = ui->ch_audio_in_device_->value();
		if (device && device[0]) {
			std::string selected_input_device = device;
			settings.set("Audio Input Enabled", true);
			settings.set("Audio Input Device", selected_input_device);
			ui->update_audio_widgets();
			ui->apply_microphone_settings();
		}
	}
	else {
		settings.set("Audio Input Enabled", false);
		ui->update_audio_widgets();
		ui->apply_microphone_settings();
	}
}

// Callback for the enable audio output checkbox - enables or disables audio output and saves the setting
void user_if::cb_enable_audio_out(Fl_Widget* w, void* data)
{
	user_if* ui = static_cast<user_if*>(data);
	Fl_Check_Button* cb = static_cast<Fl_Check_Button*>(w);
	zc_settings settings;
	if (cb->value()) {
		const char* device = ui->ch_audio_out_device_->text();
		ui->speaker_port_.port_name = ui->ch_audio_out_device_->value();
		if (device && device[0]) {
			std::string selected_output_device = device;
			settings.set("Audio Output Enabled", true);
			settings.set("Audio Output Device", selected_output_device);
			ui->update_audio_widgets();
			ui->apply_speaker_settings();
		}
	}
	else {
		settings.set("Audio Output Enabled", false);
		ui->update_audio_widgets();
		ui->apply_speaker_settings();
	}
}

// Callback function for the "Sample Rate" choice
void user_if::cb_sample_rate(Fl_Widget* w, void* data) {
	Fl_Choice* ch = static_cast<Fl_Choice*>(w);
	user_if* ui = static_cast<user_if*>(data);
	int ix = ch->value();
	unsigned int rate = sample_rates_[ix];
	zc_settings settings;
	settings.set("Sample Rate", rate);
	ui->update_audio_widgets();
	ui->apply_speaker_settings();
}

// Callback function for the "Open HTML" button.
void user_if::cb_open_html(Fl_Widget* widget, void* data) {
	std::string full_path = file_holder_->get_directory(FDD_DOCUMENTS) +
		"userguide/html/index.html";
	open_help_file(full_path);
}

// Callback function for the "Open PDF" button.
void user_if::cb_open_pdf(Fl_Widget* widget, void* data) {
	std::string full_path = file_holder_->get_directory(FDD_DOCUMENTS) +
		"userguide/ZZACWT.pdf";
	open_help_file(full_path);
}

void user_if::open_help_file(const std::string& full_filename) {
#ifdef _WIN32
	HINSTANCE result = ShellExecute(NULL, "open", full_filename.c_str(), NULL, NULL, SW_SHOWNORMAL);
	if ((intptr_t)result <= 32) {
		printf("ZZACWT: Error opening HTML %s. Error code: %d",
			full_filename.c_str(),
			(int)(intptr_t)result);
	}
#else 
	std::string cmd = "xdg-open \"" + full_filename + "\"";
	int res = system(cmd.c_str());
	if (res != 0) {
		printf("ZZACWT: Error opening HTML %s. Error code: %d",
			full_filename.c_str(),
			res);
	}
#endif
}


// Override the handle method to catch the CTRL/+ and CTRL/-.
int user_if::handle(int event) {
	switch (event) {
	case FL_KEYDOWN:
	{
		int key = Fl::event_key();
		if (Fl::event_state(FL_CTRL)) {
			bool changed = false;
			if (key == '+' || key == '=') { // CTRL + '+' or CTRL + '=' to increase font size.
				changed = zc::change_base_size(true, false);
			}
			else if (key == '-') { // CTRL + '-' to decrease font size.
				changed = zc::change_base_size(false, false);
			}
			else if (key == '0') { // CTRL + '0' to reset font size to default.
				changed = zc::set_base_size(DEFAULT_DEFAULT_SIZE, false);
			}
			if (changed) {
				{
					zc_settings settings;
					settings.set("Base Size", DEFAULT_SIZE); // Reset base size to default to ensure the change takes effect with the new font size.
				}
				if (fl_choice("Changing the font size requires a restart. Do you want to restart now?", "No", "Yes", nullptr) == 1) {
					restart_application();
				}
			}
			return 1; // Return true to indicate the event has been handled
		}
		break;
	}
	}
	return Fl_Window::handle(event); // Call base class handler for unhandled events
}