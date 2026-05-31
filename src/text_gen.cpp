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
#include "zc_fltk.h"

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
	// Load QSO data if needed
	load_qso_data();
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
	macro_cache_.clear();
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

// Generate QSO exchange based on macros and exchange definitions

// Helper method to parse and extract a token from the input string
// Returns the token and updates pos to point after the token
std::string text_gen::parse_token(const std::string& input, size_t& pos) {
    // Skip leading whitespace
    while (pos < input.length() && std::isspace(input[pos])) {
        pos++;
    }

    if (pos >= input.length()) {
        return "";
    }

    // Check for special starting characters
    char start_char = input[pos];
    if (start_char == '$' || start_char == '[' || start_char == '{' || start_char == '(' || start_char == '~') {
        // Find matching closing bracket/brace/paren
        std::map<char, char> brackets = { {'[', ']'}, {'{', '}'}, {'(', ')'}, {'~', '~'} };

        if (start_char == '$') {
            // Macro: read until whitespace or special char
            size_t start = pos;
            pos++; // skip $
            while (pos < input.length() && !std::isspace(input[pos]) &&
                input[pos] != '[' && input[pos] != '{' && input[pos] != '(' && input[pos] != ')' && input[pos] != ']' && input[pos] != '}') {
                pos++;
            }
            return input.substr(start, pos - start);
        }
        else {
            // Bracketed expression
            char end_char = brackets[start_char];
            size_t start = pos;
            int depth = 1;
            pos++;

            while (pos < input.length() && depth > 0) {
                if (input[pos] == start_char) depth++;
                else if (input[pos] == end_char) depth--;
                pos++;
            }
            // If this is a repeated token include the count in the token
			if (start_char == '{' && pos < input.length() && std::isdigit(input[pos])) {
				while (pos < input.length() && std::isdigit(input[pos])) {
					pos++;
				}
			}
            return input.substr(start, pos - start);
        }
    }
    else {
        // Regular word: read until whitespace or special char
        size_t start = pos;
        while (pos < input.length() && !std::isspace(input[pos]) &&
            input[pos] != '[' && input[pos] != '{' && input[pos] != '(' &&
            input[pos] != ')' && input[pos] != ']' && input[pos] != '}' &&
            input[pos] != '~') {
            pos++;
        }
        return input.substr(start, pos - start);
    }
}

// Generate text from a token-list string
std::string text_gen::generate_from_token_list(const token_list& tokens) {
    std::string result;

    for (const auto& token : tokens) {
        if (!result.empty()) {
            result += " ";
        }
        result += generate_from_token(token);
    }

    return result;
}

// Generate text from a single token
std::string text_gen::generate_from_token(const std::string& token) {
    if (token.empty()) {
        return "";
    }

    char first_char = token[0];

    // Handle macro: $macro-name
    if (first_char == '$') {
        std::string macro_name = token.substr(1);
		// Look in macro cache first to avoid repeated generation of the same macro
		auto cache_it = macro_cache_.find(macro_name);
		if (cache_it != macro_cache_.end()) {
			return cache_it->second;
		}
        auto it = qso_macros_.find(macro_name);
        if (it != qso_macros_.end()) {
            std::string result = generate_from_token_list(it->second);
			macro_cache_[macro_name] = result;
            return result;
        }
        return token; // Return as-is if macro not found
    }

    // Handle option-list: [option1|option2|...]
    else if (first_char == '[') {
        return generate_from_option_list(token);
    }

    // Handle repeated-token: {token-list}N
    else if (first_char == '{') {
        return generate_from_repeated_token(token);
    }

    // Handle unordered-list: (token1|token2|...)
    else if (first_char == '(') {
        return generate_from_unordered_list(token);
    }
    
    // Handle regex: ~pattern~
    else if (first_char == '~') {
        return generate_from_regex(token.substr(1, token.length() - 2));
    }

    // Regular word
    else {
        return token;
    }
}

// Generate text from a regex pattern (e.g. for callsigns)
std::string text_gen::generate_from_regex(const std::string& pattern) {
	// For simplicity, this example only handles a few specific regex patterns.
	// A full implementation would need a more robust regex generator.
	return generate_group(3, 7, std::basic_regex<char>(pattern));
}

// Generate from option-list: [option1:weight1|option2:weight2|...]
std::string text_gen::generate_from_option_list(const std::string& token) {
    // Remove brackets
    std::string content = token.substr(1, token.length() - 2);

    // Split by | to get options
    std::vector<std::pair<token_list, int>> options;
    size_t pos = 0;
    size_t start = 0;
    int depth = 0;

    while (pos <= content.length()) {
        if (pos == content.length() || (content[pos] == '|' && depth == 0)) {
            std::string option_str = content.substr(start, pos - start);

            // Check for weight (option:weight)
            size_t colon_pos = option_str.rfind(':');
            int weight = 1;
            std::string option_content = option_str;

            if (colon_pos != std::string::npos) {
                // Check if colon is not inside brackets
                bool inside_brackets = false;
                for (size_t i = colon_pos; i < option_str.length(); i++) {
                    if (option_str[i] == '[' || option_str[i] == '{' || option_str[i] == '(') {
                        inside_brackets = true;
                        break;
                    }
                }

                if (!inside_brackets) {
                    option_content = option_str.substr(0, colon_pos);
                    try {
                        weight = std::stoi(option_str.substr(colon_pos + 1));
                    }
                    catch (...) {
                        weight = 1;
                    }
                }
            }

            // Parse option content into token list
            token_list opt_tokens;
            size_t opt_pos = 0;
            while (opt_pos < option_content.length()) {
                std::string t = parse_token(option_content, opt_pos);
//                if (!t.empty()) {
                    opt_tokens.push_back(t);
//                }
            }

            options.push_back({ opt_tokens, weight });
            start = pos + 1;
        }

        if (pos < content.length()) {
            if (content[pos] == '[' || content[pos] == '{' || content[pos] == '(') depth++;
            else if (content[pos] == ']' || content[pos] == '}' || content[pos] == ')') depth--;
        }
        pos++;
    }

    // Select option based on weights
    if (options.empty()) {
        return "";
    }

    int total_weight = 0;
    for (const auto& opt : options) {
        total_weight += opt.second;
    }

    std::uniform_int_distribution<> weight_dist(0, total_weight - 1);
    int random_val = weight_dist(rng_);
    int cumulative = 0;

    for (const auto& opt : options) {
        cumulative += opt.second;
        if (random_val < cumulative) {
            return generate_from_token_list(opt.first);
        }
    }

    return generate_from_token_list(options[0].first);
}

// Generate from repeated-token: {token-list}N
std::string text_gen::generate_from_repeated_token(const std::string& token) {
    // Find closing brace
    size_t close_brace = token.rfind('}');
    if (close_brace == std::string::npos) {
        return token;
    }

    // Extract content and repeat count
    std::string content = token.substr(1, close_brace - 1);
    std::string count_str = token.substr(close_brace + 1);

    int repeat_count = 1;
    try {
        repeat_count = std::stoi(count_str);
    }
    catch (...) {
        repeat_count = 1;
    }

    // Parse content into token list
    token_list tokens;
    size_t pos = 0;
    while (pos < content.length()) {
        std::string t = parse_token(content, pos);
        if (!t.empty()) {
            tokens.push_back(t);
        }
    }

    // Generate and repeat
    std::string result;
    for (int i = 0; i < repeat_count; i++) {
        if (i > 0) {
            result += " ";
        }
        result += generate_from_token_list(tokens);
    }

    return result;
}

// Generate from unordered-list: (token1|token2|...)
std::string text_gen::generate_from_unordered_list(const std::string& token) {
    // Remove parentheses
    std::string content = token.substr(1, token.length() - 2);

    // Split by | to get token lists
    std::vector<token_list> lists;
    size_t pos = 0;
    size_t start = 0;
    int depth = 0;

    while (pos <= content.length()) {
        if (pos == content.length() || (content[pos] == '|' && depth == 0)) {
            std::string list_str = content.substr(start, pos - start);

            // Parse into token list
            token_list tokens;
            size_t list_pos = 0;
            while (list_pos < list_str.length()) {
                std::string t = parse_token(list_str, list_pos);
                if (!t.empty()) {
                    tokens.push_back(t);
                }
            }

            lists.push_back(tokens);
            start = pos + 1;
        }

        if (pos < content.length()) {
            if (content[pos] == '[' || content[pos] == '{' || content[pos] == '(') depth++;
            else if (content[pos] == ']' || content[pos] == '}' || content[pos] == ')') depth--;
        }
        pos++;
    }

    // Shuffle the lists randomly
    std::vector<int> indices;
    for (size_t i = 0; i < lists.size(); i++) {
        indices.push_back(i);
    }

    for (size_t i = indices.size() - 1; i > 0; i--) {
        std::uniform_int_distribution<> shuffle_dist(0, static_cast<int>(i));
        size_t j = shuffle_dist(rng_);
        std::swap(indices[i], indices[j]);
    }

    // Generate in random order
    std::string result;
    for (size_t i = 0; i < indices.size(); i++) {
        if (i > 0) {
            result += " ";
        }
        result += generate_from_token_list(lists[indices[i]]);
    }

    return result;
}

// Generate a QSO exchange
std::vector<std::string> text_gen::generate_qso_exchange() {
    if (qso_exchanges_.empty()) {
        // Return default if no exchanges defined
        return { "cq", "de", "gm3zza", "gm3zza", "5nn", "tu" };
    }

    // Select random exchange
    std::uniform_int_distribution<> exchange_dist(0, static_cast<int>(qso_exchanges_.size()) - 1);
    int index = exchange_dist(rng_);
    const token_list& exchange = qso_exchanges_[index];

    // Generate the exchange
    std::string generated = generate_from_token_list(exchange);

    // Split result into words
    std::vector<std::string> result;
    std::istringstream iss(generated);
    std::string word;
    while (iss >> word) {
        result.push_back(word);
    }

    return result;
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
			word_list_words_.push_back(zc::to_lower(word));
		}
	}
}

//! Read QSO exchange definitions from file and store in internal state.
void text_gen::load_qso_data() {
	qso_macros_.clear();
	qso_exchanges_.clear();
	std::ifstream qso_data_file;
	std::string dummy;
	file_holder_->get_file(FILE_QSO_DATA, qso_data_file, dummy);
	if (!qso_data_file) {
		// Add default exchange if file can't be opened
		qso_exchanges_.push_back({ "cq", "de", "gm3zza", "gm3zza", "5nn", "tu" });
		return;
	}
	std::string line;
	while (std::getline(qso_data_file, line)) {
		if (!line.empty()) {
			// Check if this is a macro definition: $macro-name token-list
			if (line[0] == '$') {
				size_t space_pos = line.find(' ');
				if (space_pos != std::string::npos) {
					std::string macro_name = line.substr(1, space_pos - 1);
					std::string macro_content = line.substr(space_pos + 1);
					// Parse macro content into token list
					token_list tokens;
					size_t pos = 0;
					while (pos < macro_content.length()) {
						std::string t = parse_token(macro_content, pos);
						if (!t.empty()) {
							tokens.push_back(t);
						}
					}
					qso_macros_[macro_name] = tokens;
				}
			}
			else if (line[0] == '@') {
				// Regular exchange definition: token-list
				token_list exchange_tokens;
				size_t pos = 1; // Skip the '@' character
				while (pos < line.length()) {
					std::string t = parse_token(line, pos);
					if (!t.empty()) {
						exchange_tokens.push_back(t);
					}
				}
				qso_exchanges_.push_back(exchange_tokens);
			}
		}
	}
}