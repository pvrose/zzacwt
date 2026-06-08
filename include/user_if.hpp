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
class Fl_Choice;
class Fl_Group;
class Fl_Input;
class Fl_Text_Editor;
class Fl_Value_Slider;

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
	//! Callback on close - closes the application.
	static void cb_close(Fl_Widget* w, void* data);

	//! Override the handle method to catch CTRL/+ and CTRL/- for adjusting font size.
	int handle(int event) override;

private:

	Fl_Group* g_content_;               //!< Group for content
	Fl_Choice* ch_mode_;                //!< Mode choice
	Fl_Value_Slider* sl_tx_size_;       //!< Transmission size slider
	Fl_Input* in_text_;                 //!< User input for text mode
	Fl_Button* bt_customise_;             //!< Customise button for QSO generation - gets user's credentials

	Fl_Group* g_speed_;                 //!< Group for speed controls
	Fl_Choice* ch_speed_type_;          //!< Speed type choice
	Fl_Value_Slider* sl_dot_speed_;     //!< Dot speed slider
	Fl_Value_Slider* sl_overall_speed_; //!< Overall speed slider

	Fl_Group* g_disturber_;             //!< Group for disturber controls
	Fl_Choice* ch_disturber_type_;      //!< Disturber type choice
	Fl_Value_Slider* sl_timing_dist_;   //!< Timing disturbance slider
	Fl_Value_Slider* sl_softness_;      //!< Rise/fall time disturbance slider
	Fl_Value_Slider* sl_noise_vol_;     //!< Noise disturbance slider
	Fl_Value_Slider* sl_noise_severity_;    //!< Noise severity slider
	Fl_Value_Slider* sl_drift_rate_;    //!< Frequency drift rate slider
	Fl_Value_Slider* sl_drift_amplitude_;   //!< Frequency drift amplitude slider
	Fl_Value_Slider* sl_drift_period_;  //!< Frequency drift period slider
	Fl_Value_Slider* sl_fading_period_;  //!< Fading period slider
	Fl_Value_Slider* sl_fading_depth_;   //!< Fading depth slider
	Fl_Button* bt_default_;             //!< Reset default values

	Fl_Group* g_tone_;                  //!< Group for volume and pitch of tone
	Fl_Value_Slider* sl_volume_;        //!< Volume slider
	Fl_Value_Slider* sl_pitch_;         //!< Pitch slider

	Fl_Group* g_controls_;              //!< Group for play/stop/repeat controls
	Fl_Button* bt_new_;                //!< New button
	Fl_Button* bt_stop_;               //!< Stop button
	Fl_Button* bt_repeat_;             //!< Repeat button

	// Methods to update the state of the user interface controls based on the current settings
	void update_content_widgets();     //!< Update content mode and related widgets
	void update_speed_widgets();       //!< Update speed type and related widgets
	void update_disturber_widgets();  //!< Update disturber type and related widgets
	void update_tone_widgets();       //!< Update tone settings and related widgets

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

public:

};
