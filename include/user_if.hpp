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

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>

#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <stdexcept>

class Fl_Button;
class Fl_Check_Button;
class Fl_Choice;
class Fl_Group;
class Fl_Input;
class Fl_Text_Editor;
class zc_wheel_value_slider;
class Fl_Widget;

//! \brief Class user_if - the user interface for the CW trainer.
class user_if : public Fl_Double_Window
{
public:
	user_if(int W, int H, const char* L = 0);
	~user_if();

	// Callbacks for user interface controls
	static void cb_mode(Fl_Widget* w, void* data);
	static void cb_tx_size(Fl_Widget* w, void* data);
	static void cb_text(Fl_Widget* w, void* data);
	static void cb_speed_type(Fl_Widget* w, void* data);
	static void cb_wpm(Fl_Widget* w, void* data);
	static void cb_overall(Fl_Widget* w, void* data);
	static void cb_disturber_type(Fl_Widget* w, void* data);
	static void cb_timing_dist(Fl_Widget* w, void* data);
	static void cb_softness(Fl_Widget* w, void* data);
	static void cb_noise_vol(Fl_Widget* w, void* data);
	static void cb_noise_severity(Fl_Widget* w, void* data);
	static void cb_drift_rate(Fl_Widget* w, void* data);
	static void cb_drift_amplitude(Fl_Widget* w, void* data);
	static void cb_drift_period(Fl_Widget* w, void* data);
	static void cb_fading_period(Fl_Widget* w, void* data);
	static void cb_fading_depth(Fl_Widget* w, void* data);
	static void cb_default(Fl_Widget* w, void* data);
	static void cb_volume(Fl_Widget* w, void* data);
	static void cb_pitch(Fl_Widget* w, void* data);
	static void cb_new(Fl_Widget* w, void* data);
	static void cb_stop(Fl_Widget* w, void* data);
	static void cb_repeat(Fl_Widget* w, void* data);
	static void cb_customise(Fl_Widget* w, void* data);
	static void cb_enable_audio_in(Fl_Widget* w, void* data);
	static void cb_enable_audio_out(Fl_Widget* w, void* data);
	static void cb_open_html(Fl_Widget* w, void* v);
	static void cb_open_pdf(Fl_Widget* w, void* v);
	static void cb_sample_rate(Fl_Widget* w, void* v);
	//! Callback on close - closes the application.
	static void cb_close(Fl_Widget* w, void* data);

	//! Override the handle method to catch CTRL/+ and CTRL/- for adjusting font size.
	int handle(int event) override;

private:

	Fl_Group* g_audio_;                 //!< Group for audio controls
	Fl_Check_Button* cb_enable_audio_in_;  //!< Enable audio input checkbox
	Fl_Check_Button* cb_enable_audio_out_; //!< Enable audio output checkbox
	Fl_Choice* ch_audio_in_device_;  //!< Audio input device choice
	Fl_Choice* ch_audio_out_device_; //!< Audio output device choice
	Fl_Choice* ch_sample_rate_;      //!< Actual sample rate

	Fl_Group* g_help_;                 //!< Group for help buttons
	Fl_Button* bt_help_html_;          //!< Open HTML User guide
	Fl_Button* bt_help_pdf_;           //!< Open PDF user guide.

	Fl_Group* g_content_;               //!< Group for content
	Fl_Choice* ch_mode_;                //!< Mode choice
	zc_wheel_value_slider* sl_tx_size_;       //!< Transmission size slider
	Fl_Input* in_text_;                 //!< User input for text mode
	Fl_Button* bt_customise_;             //!< Customise button for QSO generation - gets user's credentials

	Fl_Group* g_speed_;                 //!< Group for speed controls
	Fl_Choice* ch_speed_type_;          //!< Speed type choice
	zc_wheel_value_slider* sl_dot_speed_;     //!< Dot speed slider
	zc_wheel_value_slider* sl_overall_speed_; //!< Overall speed slider

	Fl_Group* g_disturber_;             //!< Group for disturber controls
	Fl_Choice* ch_disturber_type_;      //!< Disturber type choice
	zc_wheel_value_slider* sl_timing_dist_;   //!< Timing disturbance slider
	zc_wheel_value_slider* sl_softness_;      //!< Rise/fall time disturbance slider
	zc_wheel_value_slider* sl_noise_vol_;     //!< Noise disturbance slider
	zc_wheel_value_slider* sl_noise_severity_;    //!< Noise severity slider
	zc_wheel_value_slider* sl_drift_rate_;    //!< Frequency drift rate slider
	zc_wheel_value_slider* sl_drift_amplitude_;   //!< Frequency drift amplitude slider
	zc_wheel_value_slider* sl_drift_period_;  //!< Frequency drift period slider
	zc_wheel_value_slider* sl_fading_period_;  //!< Fading period slider
	zc_wheel_value_slider* sl_fading_depth_;   //!< Fading depth slider
	Fl_Button* bt_default_;             //!< Reset default values

	Fl_Group* g_tone_;                  //!< Group for volume and pitch of tone
	zc_wheel_value_slider* sl_volume_;        //!< Volume slider
	zc_wheel_value_slider* sl_pitch_;         //!< Pitch slider

	Fl_Group* g_controls_;              //!< Group for play/stop/repeat controls
	Fl_Button* bt_new_;                //!< New button
	Fl_Button* bt_stop_;               //!< Stop button
	Fl_Button* bt_repeat_;             //!< Repeat button

	// Methods to update the state of the user interface controls based on the current settings
	void update_content_widgets();     //!< Update content mode and related widgets
	void update_speed_widgets();       //!< Update speed type and related widgets
	void update_disturber_widgets();  //!< Update disturber type and related widgets
	void update_tone_widgets();       //!< Update tone settings and related widgets
	void update_audio_widgets();      //!< Update audio input/output device choices

	//! Create the user interface controls and set their initial states
	void create_widgets();

	//! Apply the current settings to the oscillator.
	void apply_oscillator_settings();
	//! Apply the current settings to the shaper.
	void apply_shaper_settings();
	//! Apply the current settings to the noise generator.
	void apply_noise_settings();
	//! Apply the current settings to the speaker.
	void apply_speaker_settings();
	//! Apply the current settings to the microphone.
	void apply_microphone_settings();

	//! Open the provided help file
	static void open_help_file(const std::string& full_filename);

	//! Speaker port number
	int speaker_port_;
	//! Microphone port number
	int microphone_port_;

public:

};
