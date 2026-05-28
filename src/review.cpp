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

#include "params.hpp"

#include "zc_drawing.h"
#include "zc_settings.h"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Widget.H>

#include <algorithm>
#include <string>

// Text display style map 
static Fl_Text_Display::Style_Table_Entry STYLE_TABLE[] = {
	// {Text colour, Font, Font size, Attribute flags, Background colour}
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR },  // Normal shown text.
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_BGCOLOR, FL_FOREGROUND_COLOR } // Hidden text.
	// TODO - add styles for highlighting differences between sent and typed/decoded text.
};
static int STYLE_TABLE_SIZE = sizeof(STYLE_TABLE) / sizeof(STYLE_TABLE[0]);


// Constructor
review::review(int W, int H, const char* L) : Fl_Double_Window(W, H, L) {
	create_widgets();
	load_settings();
}

// Destructor
review::~review() {
	// No dynamic memory to clean up.
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
	td_sent_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, 'A', nullptr, nullptr);
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
	td_typed_ = new Fl_Text_Editor(cx, cy, WDISPLAY, HDISPLAY);
	td_typed_->buffer(new Fl_Text_Buffer);
	td_typed_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, 'A', nullptr, nullptr);
	td_typed_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

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
	for (const auto& [source, label] : text_source_strings_) {
		if (source >= text_source_t::DECODED_NONE && source < text_source_t::DECODED_LAST) {
			ch_decode_source_->add(label.c_str());
		}
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
	td_decoded_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, 'A', nullptr, nullptr);
	td_decoded_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

	g_decoded_->end();

	cy = g_decoded_->y() + g_decoded_->h() + GAP;
	end();
	// Resize the window to fit the widgets.
	resizable(nullptr);
	size(GAP + WGROUPS + GAP, cy);

}

// Load settings.
void review::load_settings() {
	zc_settings settings;
	settings.get("Display While Sending", show_as_sending_, false);
	settings.get("Decode Source", decode_source_, text_source_t::DECODED_NONE);
	ck_as_sending_->value(show_as_sending_);
	ch_decode_source_->value(static_cast<int>(decode_source_) - static_cast<int>(text_source_t::DECODED_NONE));
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

// Add sent text to the review.
void review::add_sent_text(const std::string& text, text_source_t source) {
	if (source == text_source_t::SENT_TEXT) {
		Fl_Text_Buffer* buffer = td_sent_->buffer();
		buffer->append(text.c_str());
		Fl_Text_Buffer* highlight_buffer = td_sent_->style_buffer();
		// Highlight the new text as hidden if show_as_sending_ is false, otherwise highlight it as normal.
		char style_char = show_as_sending_ ? 'A' : 'B';
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
		Fl::check(); // Process events to update the display.
	}
	else {
		// TODO - handle other sources of text if needed.
	}
}

// Placeholders for hide_text and show_text functions.
void review::hide_text(Fl_Text_Display* td) {
	Fl_Text_Buffer* highlight_buffer = td->style_buffer();
	// Set the style of all text in the display to the hidden style (style 'B').
	// Create a style string of the same length as the text in the display, filled with 'B', and set it as the new highlight buffer.
	char* style_str = new char[highlight_buffer->length() + 1];
	*std::fill_n(style_str, highlight_buffer->length(), 'B') = '\0';
	highlight_buffer->text(style_str);
	delete[] style_str;
	td->redraw();
	Fl::check();
}
void review::show_text(Fl_Text_Display* td) {
	Fl_Text_Buffer* highlight_buffer = td->style_buffer();
	// Set the style of all text in the display to the normal style (style 'A').
	char* style_str = new char[highlight_buffer->length() + 1];
	*std::fill_n(style_str, highlight_buffer->length(), 'A') = '\0';
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
		td_typed_->style_buffer()->text("");
		td_typed_->redraw();
	}
	else if (source == decode_source_) {
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
	clear_display(text_source_t::TYPED_TEXT);
	clear_display(decode_source_);
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
	// Placeholder for callback function to compare sent text with typed text.
}
void review::cb_decode_source(Fl_Widget* w, void* data) {
	// Placeholder for callback function to select source of audio for decoding.
}
void review::cb_compare_decoded(Fl_Widget* w, void* data) {
	// Placeholder for callback function to compare sent text with decoded text.
}
void review::compare_displays(Fl_Text_Display* td1, Fl_Text_Display* td2) {
	// Placeholder for function to compare the text in two text displays and highlight the differences.
}
