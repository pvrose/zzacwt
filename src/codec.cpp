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
#include "codec.hpp"

#include <cctype>
#include <iostream>
#include <map>
#include <string>
#include <vector>


//! Map between characters and their Morse code representations.
const std::map<char, std::string> TO_MORSE = {
	{'a', ".-"},
	{'b', "-..."},
	{'c', "-.-."},
	{'d', "-.."},
	{'e', "."},
	{'f', "..-."},
	{'g', "--."},
	{'h', "...."},
	{'i', ".."},
	{'j', ".---"},
	{'k', "-.-"},
	{'l', ".-.."},
	{'m', "--"},
	{'n', "-."},
	{'o', "---"},
	{'p', ".--."},
	{'q', "--.-"},
	{'r', ".-."},
	{'s', "..."},
	{'t', "-"},
	{'u', "..-"},
	{'v', "...-"},
	{'w', ".--"},
	{'x', "-..-"},
	{'y', "-.--"},
	{'z', "--.."},
	{'0', "-----"},
	{'1', ".----"},
	{'2', "..---"},
	{'3', "...--"},
	{'4', "....-"},
	{'5', "....."},
	{'6', "-...."},
	{'7', "--..."},
	{'8', "---.."},
	{'9', "----."},
	{'.', ".-.-.-"},
	{',', "--..--"},
	{'?', "..--.."},
	{'\'', ".----."},
	{'!', "-.-.--"},
	{'/', "-..-."},
	{'(', "-.--."},
	{')', "-.--.-"},
	{'&', ".-..."},
	{':', "---..."},
	{';', "-.-.-."},
	{'=', "-...-"},
	{'+', ".-.-."},
	{'-', "-....-"},
	{'_', "..--.-"},
	{'"', ".-..-."},
	{'$', "...-..-"},
	{'@', ".--.-."}
};

std::map<std::string, char> FROM_MORSE;  //!< Map between Morse code representations and their corresponding characters. This is generated from the TO_MORSE map at runtime.

struct from_morse_initializer {
	from_morse_initializer() {
		for (const auto& pair : TO_MORSE) {
			FROM_MORSE[pair.second] = pair.first;
		}
	}
};

static from_morse_initializer initializer;

//! Encoder
void codec::encode(const std::string& input, std::vector<std::vector<symbol_t>>& symbols)
{
	std::string current_morse;
	for (size_t i = 0; i < input.size(); ++i) {
		char c = tolower(input[i]);
		std::vector<symbol_t> char_symbols;
		if (c == ' ') {
			// If we haven't started a new Morse code character,
			// no need to add a word space, but if we have, do so.
			if (!current_morse.empty()) {
				char_symbols.push_back(symbol_t::WORD_SPACE);
				current_morse.clear();
			}
			continue;
		}
		// Check for a prosign - if so concatenate the Morse code representations of the individual characters
		// in the prosign without adding character spaces, and add a word space at the end if needed.
		if (c == '<') {
			size_t end_pos = input.find('>', i);
			if (end_pos != std::string::npos) {
				std::string prosign = input.substr(i + 1, end_pos - i - 1);
				for (char pc : prosign) {
					pc = toupper(pc);
					auto it = TO_MORSE.find(pc);
					if (it != TO_MORSE.end()) {
						const std::string& morse = it->second;
						for (size_t j = 0; j < morse.size(); ++j) {
							char symbol = morse[j];
							if (symbol == '.') {
								char_symbols.push_back(symbol_t::DOT_MARK);
							}
							else if (symbol == '-') {
								char_symbols.push_back(symbol_t::DASH_MARK);
							}
							if (j < morse.size() - 1) {
								char_symbols.push_back(symbol_t::INTERNAL_SPACE);
							}
						}
						// Not at the end of the input string nor the current word.
						if (i < input.size() - 1 && input[i + 1] != ' ') {
							char_symbols.push_back(symbol_t::INTERNAL_SPACE);
						}
					}
					else {
						std::cerr << "Warning: Unsupported character '" << pc << "' in prosign ignored in encoding." << std::endl;
					}
				}
				
				i = end_pos; // Move index to the end of the prosign
				current_morse.clear();
				// Not at the end of the input string nor the current word.
				if (i < input.size() - 1 && input[i + 1] != ' ') {
					char_symbols.push_back(symbol_t::CHARACTER_SPACE);
				}
				else {
					char_symbols.push_back(symbol_t::WORD_SPACE);
					current_morse.clear();
				}
				symbols.push_back(char_symbols);
				continue;
			}
		}
		auto it = TO_MORSE.find(c);
		if (it != TO_MORSE.end()) {
			// Look up the Morse code representation for the character and
			// convert it to the corresponding symbol_t values.
			const std::string& morse = it->second;
			for (size_t j = 0; j < morse.size(); ++j) {
				char symbol = morse[j];
				if (symbol == '.') {
					char_symbols.push_back(symbol_t::DOT_MARK);
				}
				else if (symbol == '-') {
					char_symbols.push_back(symbol_t::DASH_MARK);
				}
				if (j < morse.size() - 1) {
					char_symbols.push_back(symbol_t::INTERNAL_SPACE);
				}
			}
			current_morse = morse;
			// Not at the end of the input string nor the current word.
			if (i < input.size() - 1 && input[i + 1] != ' ') {
				char_symbols.push_back(symbol_t::CHARACTER_SPACE);
			}
			else {
				char_symbols.push_back(symbol_t::WORD_SPACE);
				current_morse.clear();
			}
			symbols.push_back(char_symbols);
		}
		else {
			std::cerr << "Warning: Unsupported character '" << c << "' ignored in encoding." << std::endl;
		}
	}
}

//! Decoder
void codec::decode(const std::vector<symbol_t>& symbols, std::string& output)
{
	std::string current_morse;
	for (size_t i = 0; i < symbols.size(); ++i) {
		symbol_t symbol = symbols[i];
		if (symbol == symbol_t::DOT_MARK) {
			current_morse += '.';
		}
		else if (symbol == symbol_t::DASH_MARK) {
			current_morse += '-';
		}
		else if (symbol == symbol_t::CHARACTER_SPACE || symbol == symbol_t::WORD_SPACE) {
			if (!current_morse.empty()) {
				//printf("Decoding Morse code: '%s'\n", current_morse.c_str());
				auto it = FROM_MORSE.find(current_morse);
				if (it != FROM_MORSE.end()) {
					output += it->second;
				}
				else {
					std::cerr << "Warning: Invalid Morse code '" << current_morse << "' ignored in decoding." << std::endl;
					// Unicode replacement character U+FFFD in UTF-8: 0xEF 0xBF 0xBD
					output += "\xEF\xBF\xBD";
				}
				current_morse.clear();
			}
			if (symbol == symbol_t::WORD_SPACE) {
				output += ' ';
			}
		}
		else if (symbol == symbol_t::INTERNAL_SPACE) {
			continue; // No change to the Morse code representation of the current character.
		}
		else {
			std::cerr << "Warning: Badly formed symbol sequence at index " << i << " ignored in decoding." << std::endl;
		}
	}
}