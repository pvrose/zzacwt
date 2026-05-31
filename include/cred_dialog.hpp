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

#include <map>
#include <string>

class Fl_Button;
class Fl_Input;

//! \brief Dialog class for setting user's credentials for QSO generation in text_gen.
//! This dialog allows the user to enter their callsign, name, location, and equipment details,
//! which are then used to generate realistic QSO exchanges in the text generator module.
//! 
class cred_dialog : public Fl_Double_Window
{
public:
	cred_dialog(int W, int H, const char* L = 0);
	~cred_dialog();

	std::map<std::string, std::string> get_credentials() const; //!< Get the user's credentials as a map of field names to values.
	void set_credentials(const std::map<std::string, std::string>& creds); //!< Set the input fields based on a map of field names to values.

	static void cb_ok(Fl_Widget* w, void* data); //!< Callback for OK button - should hide dialog if valid.

private:
	Fl_Input* callsign_input_; //!< Input field for user's callsign
	Fl_Input* name_input_;     //!< Input field for user's name
	Fl_Input* location_input_; //!< Input field for user's location
	Fl_Input* rig_input_;      //!< Input field for user's rig details
	Fl_Input* ant_input_;      //!< Input field for user's antenna details
	Fl_Input* power_input_;    //!< Input field for user's power details
	Fl_Button* bt_ok_;    //!< OK button
};
