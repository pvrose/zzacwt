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
#include "zc_ticker.h"

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
#include <cstring>
#include <string>

// Text display style map 
static Fl_Text_Display::Style_Table_Entry STYLE_TABLE[] = {
	// {Text colour, Font, Font size, Attribute flags, Background colour}
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR },  // Normal shown text.
	{ FL_FOREGROUND_COLOR, FL_HELVETICA, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_BGCOLOR, FL_FOREGROUND_COLOR }, // Hidden text.
	{ FL_RED, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR }, // "Missing" text - red underline.
	{ FL_BLUE, FL_HELVETICA, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_STRIKE_THROUGH, FL_BACKGROUND_COLOR }, // "Extra" text - blue strike-through
	{ FL_BLACK, FL_HELVETICA, FL_NORMAL_SIZE, Fl_Text_Display::ATTR_STRIKE_THROUGH, FL_BACKGROUND_COLOR }, // "Wrong" text - black strike-through.
	{ FL_BLACK, FL_HELVETICA, FL_NORMAL_SIZE, 0, FL_BACKGROUND_COLOR }, // "Match" text - black underline.
};
static int STYLE_TABLE_SIZE = sizeof(STYLE_TABLE) / sizeof(STYLE_TABLE[0]);
static char STYLE_NORMAL = 'A';
static char STYLE_HIDDEN = 'B';
static char STYLE_MISSING = 'C';
static char STYLE_EXTRA = 'D';
static char STYLE_WRONG = 'E';
static char STYLE_MATCH = 'F';

extern zc_ticker* ticker_;

// Constructor
review::review(int W, int H, const char* L) : Fl_Double_Window(W, H, L) {
	create_widgets();
	load_settings();
	ticker_->add_ticker(this, cb_ticker, 5, false);
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

	cy += HBUTTON;
	td_correct_ = new Fl_Text_Display(cx, cy, WBUTTON, HBUTTON);
	td_correct_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_MATCH, nullptr, nullptr);
	td_correct_->buffer(new Fl_Text_Buffer);
	td_correct_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
	td_correct_->buffer()->text("Correct");
	td_correct_->style_buffer()->text("FFFFFFF");
	td_correct_->scrollbar_size(1);

	cy += HBUTTON;
	td_missing_ = new Fl_Text_Display(cx, cy, WBUTTON, HBUTTON);
	td_missing_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_MISSING, nullptr, nullptr);
	td_missing_->buffer(new Fl_Text_Buffer);
	td_missing_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
	td_missing_->buffer()->text("Missing");
	td_missing_->style_buffer()->text("CCCCCCC");
	td_missing_->scrollbar_size(1);

	cy += HBUTTON;
	td_extra_ = new Fl_Text_Display(cx, cy, WBUTTON, HBUTTON);
	td_extra_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_EXTRA, nullptr, nullptr);
	td_extra_->buffer(new Fl_Text_Buffer);
	td_extra_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
	td_extra_->buffer()->text("Extra");
	td_extra_->style_buffer()->text("DDDDDDD");
	td_extra_->scrollbar_size(1);

	cy += HBUTTON;
	td_wrong_ = new Fl_Text_Display(cx, cy, WBUTTON, HBUTTON);
	td_wrong_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_WRONG, nullptr, nullptr);
	td_wrong_->buffer(new Fl_Text_Buffer);
	td_wrong_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
	td_wrong_->buffer()->text("Wrong");
	td_wrong_->style_buffer()->text("EEEEEEE");
	td_wrong_->scrollbar_size(1);

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
	td_decoded_->highlight_data(new Fl_Text_Buffer, STYLE_TABLE, STYLE_TABLE_SIZE, STYLE_NORMAL, nullptr, nullptr);
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
		Fl::awake();
//		Fl::awake(cb_sent_text_updated, this); // Schedule an update of the sent text display in the main thread.
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
	clear_display(decode_source_);
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
	// Placeholder for callback function to select source of audio for decoding.
}
void review::cb_compare_decoded(Fl_Widget* w, void* data) {
	// Placeholder for callback function to compare sent text with decoded text.
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
	char* user_text = user->buffer()->text();
	char* sent_text = sent->buffer()->text();

	size_t user_len = std::strlen(user_text);
	size_t sent_len = std::strlen(sent_text);
	char* new_user = new char[2 * (user_len + sent_len) + 1];
	char* new_style = new char[2 * (user_len + sent_len) + 1];

	char* user_ix = user_text;
	char* sent_ix = sent_text;
	char* style_ix = new_style;
	char* new_user_ix = new_user;
	int number_matched = 0;
	while (*user_ix != '\0' && *sent_ix != '\0') {
		// Get number of matching characters from the current position.	
		if (number_matched) {
			if (*user_ix == *sent_ix) {
				*new_user_ix++ = *user_ix++;
				*style_ix++ = STYLE_MATCH;
				sent_ix++;
				number_matched++;
				continue;
			}
		}
		number_matched = 0;
		if (strncmp(user_ix, sent_ix, M) == 0) {
			strncpy(new_user_ix, user_ix, M);
			strncpy(new_user_ix, user_ix, M);
			memset(style_ix, STYLE_MATCH, M);
			user_ix += M;
			sent_ix += M;
			new_user_ix += M;
			style_ix += M;
		}
		else {
			bool match_found = false;
			for (int i = 1; i <= N; i++) {
				if (strncmp(user_ix, sent_ix + i, M) == 0) {
					// Copy sent to new user and mark the mismatched character as "Missing".
					strncpy(new_user_ix, sent_ix, i);
					memset(style_ix, STYLE_MISSING, i);
					new_user_ix += i;
					style_ix += i;
					match_found = true;
					sent_ix += i;
					break;
				}
				else if (strncmp(user_ix + i, sent_ix, M) == 0) {
					// Mark the mismatched characters as "Extra" and step the user data by one character.
					strncpy(new_user_ix, user_ix, i);
					memset(style_ix, STYLE_EXTRA, i);
					new_user_ix += i;
					style_ix += i;
					match_found = true;
					user_ix += i;
					break;
				}
			}
			if (!match_found) {
				// Mark the mismatched character in user as "Wrong" and step both data by one character.
				*new_user_ix++ = *user_ix++;
				*style_ix++ = STYLE_WRONG;
				// Copy sent character to new user and mark as "Missing".
				*new_user_ix++ = *sent_ix++;
				*style_ix++ = STYLE_MISSING;
			}
		}
	}
	// If there is remaining text in user, mark it as "Extra".	
	while (*user_ix != '\0') {
		*new_user_ix++ = *user_ix++;
		*style_ix++ = STYLE_EXTRA;
	}
	// If there is remaining text in sent, copy it to user and mark it as "Missing".
	while (*sent_ix != '\0') {
		*new_user_ix++ = *sent_ix++;
		*style_ix++ = STYLE_MISSING;
	}
	// Null-terminate the new user and style strings.
	*new_user_ix = '\0';
	*style_ix = '\0';
	// Update the user display with the new user string and style string.
	user->buffer()->text(new_user);
	user->style_buffer()->text(new_style);

	delete[] new_user;
	delete[] new_style;
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

// Callback to refresh sent text display every 500 ms.
void review::cb_ticker(void* data) {
	review* r = static_cast<review*>(data);
	Fl::check();
}
