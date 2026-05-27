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
#include "text_gen.hpp"

#include "params.hpp"

#include "zc_file_holder.h"
#include "zc_settings.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <ios>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

const std::basic_regex<char> REGEX_CALLSIGN("([2-9][A-Z]|[A-Z]|[A-Z][0-9])([0-9][A-Z]{1,3})");
const std::basic_regex<char> REGEX_LETTERS("[A-Z]{5}");
const std::basic_regex<char> REGEX_DIGITS("[0-9]{5}");
const std::basic_regex<char> REGEX_LETTERS_DIGITS("[A-Z0-9]{5}");

//! Constructor
text_gen::text_gen()
	: rng_(std::random_device{}())
{
	// Initialise settings
	apply_settings();
	// Load word list if needed
	load_word_list();
	if (mode_ == content_mode::TEST_MODE_A) {
		// Automatically start test mode A when selected. 
		generate_new_sequence();
	}
}

//! Destructor
text_gen::~text_gen()
{
	// Close text file stream if open
	if (text_file_stream_.is_open()) {
		text_file_stream_.close();
	}
}

//! Generate a new sequence of words according to the settings.
void text_gen::generate_new_sequence() {
	current_sequence_.clear();
	next_word_index_ = 0;
	// Read settings and update internal state as they may have changed.
	apply_settings();
	// Generate sequence based on mode
	switch (mode_) {
	case content_mode::LETTERS:
		for (int i = 0; i < num_groups_; ++i) {
			current_sequence_.push_back(generate_group(5, 5, REGEX_LETTERS));
		}
		break;
	case content_mode::NUMBERS:
		for (int i = 0; i < num_groups_; ++i) {
			current_sequence_.push_back(generate_group(5, 5, REGEX_DIGITS));
		}
		break;
	case content_mode::MIXED:
		for (int i = 0; i < num_groups_; ++i) {
			current_sequence_.push_back(generate_group(5, 5, REGEX_LETTERS_DIGITS));
		}
		break;
	case content_mode::CALLSIGNS:
		for (int i = 0; i < num_groups_; ++i) {
			current_sequence_.push_back(generate_group(3, 7, REGEX_CALLSIGN));
		}
		break;
	case content_mode::WORD_LIST:
		current_sequence_ = generate_word_list_words();
		break;
	case content_mode::TEXT_ONLY:
		text_file_position_ = text_file_new_position_; // Restore saved position in text file
		current_sequence_ = generate_text_sentence(false);
		break;
	case content_mode::TEXT_PUNCTUATION:
		text_file_position_ = text_file_new_position_; // Restore saved position in text file
		current_sequence_ = generate_text_sentence(true);
		break;
	case content_mode::QSO:
		current_sequence_ = generate_qso_exchange();
		break;
	case content_mode::USER_TEXT:
		current_sequence_ = generate_user_text();
		break;
	case content_mode::TEST_MODE_A:
		for (int i = 0; i < num_groups_; ++i) {
			current_sequence_.push_back("VVV");
		} 
		break;
	default:
		break;
	}
}

//! Repeat the existing sequence of words.
void text_gen::repeat_sequence() {
	next_word_index_ = 0;
}

//! Stop the current sequence and clear the internal state.
void text_gen::stop_sequence() {
	next_word_index_ = current_sequence_.size();
}

//! Get the next word in the sequence. Return an empty string if the sequence is finished.
std::string text_gen::get_next_word() {
	if (next_word_index_ < current_sequence_.size()) {
		return current_sequence_[next_word_index_++];
	}
	else {
		return "";
	}
}

//! Generate a group of characters.
//! \param min_length Minimum length of the group.
//! \param max_length Maximum length of the group.
//! \param regex Regular expression defining the allowed characters in the group.
//! The function generates random groups of characters until one matches the provided regex.
//! Only generate characters that are ASCII printable and not whitespace,
//! i.e. characters in the range 33-126 in the ASCII table.
std::string text_gen::generate_group(int min_length, int max_length, const std::basic_regex<char>& regex) {
	std::string group;
	std::uniform_int_distribution<> length_dist(min_length, max_length);
	std::uniform_int_distribution<> char_dist(33, 126); // ASCII printable characters
	do {
		int length = length_dist(rng_);
		group.clear();
		for (int i = 0; i < length; ++i) {
			char c = static_cast<char>(char_dist(rng_));
			group += c;
		}
	} while (!std::regex_match(group, regex));
	return group;
}

//! Read settings and update internal state accordingly.
void text_gen::apply_settings() {
	// Read content mode and number of groups from settings
	zc_settings settings;
	settings.get("Content Mode", mode_, content_mode::LETTERS);
	settings.get("Block Size", num_groups_, 10);
	settings.get<std::string>("User Text", user_text_, "Hello World");
	settings.get<std::streampos>("Text File Position", text_file_new_position_, 0);
}

//! Save settings that need to be preserved between sessions (e.g. text file position).
void text_gen::save_settings() {
	// Save position in text file for next time
	if (mode_ == content_mode::TEXT_ONLY || mode_ == content_mode::TEXT_PUNCTUATION) {
		zc_settings settings;
		settings.set("Text File Position", text_file_new_position_);
	}
}

//! Generate a word from the text file.
std::vector<std::string> text_gen::generate_text_sentence(bool include_punctuation) {
	std::vector<std::string> sentence = {"To", "be", include_punctuation ? "implemented!" : "implemented"};
	// Open text file if not already open
	if (!text_file_stream_.is_open()) {
		std::string dummy;
		file_holder_->get_file(FILE_TEXT_FILE, text_file_stream_, dummy);
		if (!text_file_stream_) {
			return sentence; // Return placeholder sentence if file couldn't be opened
		}
	}
	// Seek to saved position in text file
	text_file_stream_.clear(); // Clear any error flags
	text_file_stream_.seekg(text_file_position_);
	// If seek failed, reset to beginning of file
	if (!text_file_stream_) {
		text_file_stream_.clear();
		text_file_stream_.seekg(0);
		text_file_position_ = 0;
	}
	sentence.clear();
	std::string word;
	while (text_file_stream_ >> word) {
		// Check if this word ends with sentence-ending punctuation
		bool ends_sentence = !word.empty() && 
							 (word.back() == '.' || word.back() == '!' || word.back() == '?');

		if (!include_punctuation) {
			// Remove punctuation from word
			word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
		}

		// Only add non-empty words (punctuation removal might leave empty string)
		if (!word.empty()) {
			sentence.push_back(word);
		}

		// Stop after sentence-ending punctuation or if we hit max size
		if (ends_sentence || sentence.size() >= static_cast<size_t>(num_groups_)) {
			break;
		}
	}
	// Save current position in text file for next time
	text_file_new_position_ = text_file_stream_.tellg();
	save_settings();
	return sentence;
}

//! Generate a word from the word list file.
std::vector<std::string> text_gen::generate_word_list_words() {
	std::vector<std::string> words;
	// Get words at random from the word list until we have enough
	std::uniform_int_distribution<> dist(0, word_list_words_.size() - 1);
	for (int i = 0; i < num_groups_; ++i) {
		int index = dist(rng_);
		words.push_back(word_list_words_[index]);
	}	
	return words;
}

//! Generate a QSO exchange.
//! For simplicity, this function generates a fixed QSO exchange. In a real implementation,
//! you would want to generate random callsigns, signal reports, and messages according to typical QSO formats.
std::vector<std::string> text_gen::generate_qso_exchange() {
	//! \todo Implement random generation of QSO exchanges with 
	//! realistic callsigns, signal reports, and messages.
	return { "CQ", "DE", "GM3ZZA", "GM3ZZA", "5NN", "TU" };
}

//! Generate user defined text.
//! The user text is split into words and these are selected in order.
std::vector<std::string> text_gen::generate_user_text() {
	std::vector<std::string> words;
	std::istringstream iss(user_text_);
	std::string word;
	while (iss >> word) {
		words.push_back(word);
	}
	return words;
}

//! Read word-list file and store words in internal state.
void text_gen::load_word_list() {
	word_list_words_.clear();
	std::ifstream word_list_file;
	std::string dummy;
	file_holder_->get_file(FILE_WORD_LIST, word_list_file, dummy);
	if (!word_list_file) {
		word_list_words_ = { "To", "be", "implemented" }; // Placeholder words if file can't be opened
		return; // If file couldn't be opened, just use placeholder words
	}
	std::string word;
	while (std::getline(word_list_file, word)) {
		if (!word.empty()) {
			word_list_words_.push_back(word);
		}
	}
}