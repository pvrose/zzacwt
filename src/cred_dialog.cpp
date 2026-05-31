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

#include "cred_dialog.hpp"

#include "zc_drawing.h"

#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>

#include <cstring>
#include <map>
#include <string>

// Constructor
cred_dialog::cred_dialog(int W, int H, const char* L)
	: Fl_Double_Window(W, H, L)
{
	// Create input fields and buttons here
	int cx = GAP + WLABEL;
	int cy = GAP;
	callsign_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Callsign:");
	callsign_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	name_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Name:");
	name_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	location_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Location:");
	location_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	rig_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Rig:");
	rig_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	ant_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Antenna:");
	ant_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	power_input_ = new Fl_Input(cx, cy, WSMEDIT, HBUTTON, "Power:");
	power_input_->align(FL_ALIGN_LEFT);
	cy += HBUTTON;
	bt_ok_ = new Fl_Button(cx, cy, WBUTTON, HBUTTON, "OK");
	bt_ok_->callback(cb_ok, this);
	// Set up callbacks for buttons here (not implemented in this snippet)

	// Finalize dialog setup
	end();
	// Adjust size to fit contents
	resizable(nullptr);
	resize(x(), y(), cx + WSMEDIT + GAP, cy + HBUTTON + GAP);
}

// Destructor
cred_dialog::~cred_dialog()
{
	// Clean up resources if needed (FLTK will handle widget cleanup)
}

// Get the user's credentials as a map of field names to values.
std::map<std::string, std::string> cred_dialog::get_credentials() const
{
	std::map < std::string, std::string> result;
	if (strlen(callsign_input_->value()) > 0) result["CALL"] = callsign_input_->value();
	if (strlen(name_input_->value()) > 0) result["NAME"] = name_input_->value();
	if (strlen(location_input_->value()) > 0) result["QTH"] = location_input_->value();
	if (strlen(rig_input_->value()) > 0) result["RIG"] = rig_input_->value();
	if (strlen(ant_input_->value()) > 0) result["ANT"] = ant_input_->value();
	if (strlen(power_input_->value()) > 0) result["PWR"] = power_input_->value();
	return result;
}

// Set the input fields based on a map of field names to values.
void cred_dialog::set_credentials(const std::map<std::string, std::string>& creds)
{
	if (creds.find("CALL") != creds.end()) callsign_input_->value(creds.at("CALL").c_str());
	if (creds.find("NAME") != creds.end()) name_input_->value(creds.at("NAME").c_str());
	if (creds.find("QTH") != creds.end()) location_input_->value(creds.at("QTH").c_str());
	if (creds.find("RIG") != creds.end()) rig_input_->value(creds.at("RIG").c_str());
	if (creds.find("ANT") != creds.end()) ant_input_->value(creds.at("ANT").c_str());
	if (creds.find("PWR") != creds.end()) power_input_->value(creds.at("PWR").c_str());
}

// Callback for OK button - should hide dialog if valid.
void cred_dialog::cb_ok(Fl_Widget* w, void* data)
{
	Fl_Window* dialog = (Fl_Window*)data;
	dialog->default_callback(dialog, data); // Call the default callback to hide the dialog
}
