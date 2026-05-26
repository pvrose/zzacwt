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

#include <fstream>
#include <ios>
#include <random>
#include <regex>
#include <string>
#include <vector>

//! \file text_gen.hpp

//! \brief Text generator class.
//! 
//! This class generates one "word" at a time, where a "word" is a sequence of characters
//! (letters, digits, punctuation) that can be used for Morse code practice. 
//! 
//! Depending on the settings this word can be one of the following types:
//! 1. A 5-letter group consisting of random letters (A-Z).
//! 2. A 5-number group consisting of random digits (0-9).
//! 3. A 5-character group consisting of random letters and digits.
//! 4. A word selected sequentially from a piece of plain text (e.g. a book)
//! with punctuation removed. The text is read from a file and the words 
//! are selected in order. 
//! 5. As option 4, but with punctuation included.
//! 6. A word selected in sequence from a randomly generated QSO exchange.
//! 7. A callsign randomly generated according to the standard format.
//! 8. User defined text, which is read from an input widget. 
//! The text is split into words and these are selected in order.
//! 9. Test mode A - continually send the same word "VVV" for test purposes.
//! 
//! The module will be asked to generated a new sequence of words or
//! repeat the existing sequence.
//! 
//! A sequence is defined n each case as:
//! - For options 1-3 and 7, a sequence is a number of groups defined in settings.
//! - For options 4-5, a sequence is a sentence from the text file.
//! - For option 6, a sequence is a QSO exchange.
//! - For option 8, a sequence is the user-supplied text split into words.
//! 
//! In option 4, the position in the text file is saved and restored between sessions.
//! 
class text_gen
{
public:
	text_gen();
	~text_gen();
	//! Generate a new sequence of words according to the settings.
	void generate_new_sequence();
	//! Repeat the existing sequence of words.
	void repeat_sequence();
	//! Stop the current sequence and clear the internal state.
	void stop_sequence();
	//! Get the next word in the sequence. Return an empty string 
	//! if the sequence is finished.
	std::string get_next_word();

private:
	//! Generate a word from the text file.
	std::vector<std::string> generate_text_sentence(bool include_punctuation);
	//! Generate a QSO exchange.
	std::vector<std::string> generate_qso_exchange();
	//! Generate user defined text.
	std::vector<std::string> generate_user_text();
	//! Generate a group of characters.
	//! \param min_length Minimum length of the group.
	//! \param max_length Maximum length of the group.
	//! \param regex Regular expression defining the allowed characters in the group.
	std::string generate_group(int min_length, int max_length, const std::basic_regex<char>& regex);
	// Current sequence of words
	std::vector<std::string> current_sequence_;
	// Index of the next word to return
	size_t next_word_index_;

	//! Read settings and update internal state accordingly.
	void apply_settings();
	//! Test type to generate based on settings.
	content_mode mode_;
	//! Number of groups to generate for options 1-3 and 7.
	int num_groups_;
	//! User defined text for option 8.
	std::string user_text_;
	//! Position in the text file for option 4.
	std::streampos text_file_position_;

	//! File stream for reading text file in option 4.
	std::ifstream text_file_stream_;

	//! Random number engine for generating random content.
	std::mt19937 rng_;
};