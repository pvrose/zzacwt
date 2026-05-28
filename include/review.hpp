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

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Text_Display.H>

#include <array>
#include <cstdint>
#include <string>

class Fl_Button;
class Fl_Check_Button;
class Fl_Choice;
class Fl_Group;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Text_Editor;


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
	//! \brief Add sent text to the review.
	void add_sent_text(const std::string& text, text_source_t source = text_source_t::SENT_TEXT);

	// Callbacks for controls.
	static void cb_as_sending(Fl_Widget* w, void* data);
	static void cb_show(Fl_Widget* w, void* data);
	static void cb_compare_typed(Fl_Widget* w, void* data);
	static void cb_decode_source(Fl_Widget* w, void* data);
	static void cb_compare_decoded(Fl_Widget* w, void* data);


private:

	//! \brief Create the widgets for the review window.
	void create_widgets();

	//! \brief Load settings.
	void load_settings();

	//! \brief Save settings.
	void save_settings() const;

	//! \brief Compare the text in two text displays and highlight the differences.
	//! \param td1 The first text display to compare _ and the one to be highligthed.
	//! \param td2 The second text display to compare _ and the one to be compared against.
	void compare_displays(Fl_Text_Display* td1, Fl_Text_Display* td2);

	//! \brief Hide the text in a text display by displaying it in background colour.
	void hide_text(Fl_Text_Display* td);
	//! \brief Show the text in a text display by displaying it in foreground colour.
	void show_text(Fl_Text_Display* td);

	// Widgets.
	Fl_Group* g_sent_;     //<! Group for sent text.
	Fl_Text_Display* td_sent_;  //<! Text display for sent text.
	Fl_Check_Button* ck_as_sending_;  //<! Check button to show sent text while sending.
	Fl_Button* btn_show_;  //<! Button to show sent text after sending.

	Fl_Group* g_typed_;    //<! Group for typed text.
	Fl_Text_Editor* td_typed_;  //<! Text display for typed text.
	Fl_Button* btn_compare_typed_;  //<! Button to compare sent text with typed text.

	Fl_Group* g_decoded_;  //<! Group for decoded text.
	Fl_Text_Display* td_decoded_;  //<! Text display for decoded text.
	Fl_Choice* ch_decode_source_;  //<! Choice to select source of audio for decoding.
	Fl_Button* btn_compare_decoded_;  //<! Button to compare sent text with decoded text.


	// Settings.
	bool show_as_sending_;  //<! Whether to show sent text while sending.
	text_source_t decode_source_;  //<! Source of audio for decoding.

};