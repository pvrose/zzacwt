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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

//! \file codec.hpp

//! Morse code symbols representing the various states of the audio envelope.
enum class symbol_t : uint8_t {
	DOT_MARK,          //!< Mark value for the length of a dot
	DASH_MARK,         //!< Mark value for the length of a dash 
	INTERNAL_SPACE,    //!< Space value between dots and dashes of a character.
	CHARACTER_SPACE,   //!< Space value between characters.
	WORD_SPACE,        //!< Space value between words
	// Invalid symbols
	NOISE,             //!< A mark or space less than a minimum duration (say 20% dit-time).
	STUCK_MARK,        //!< A mark greater than a maximum duration
	STUCK_SPACE,       //!< A space greater than a maximum duration.
	UNFINISHED,        //!< The symbol has not yet finished (or timed out)
};

static inline const std::map<symbol_t, std::string> symbol_strings_ = {
	{symbol_t::DOT_MARK, "DOT_MARK"},
	{symbol_t::DASH_MARK, "DASH_MARK"},
	{symbol_t::INTERNAL_SPACE, "INTERNAL_SPACE"},
	{symbol_t::CHARACTER_SPACE, "CHARACTER_SPACE"},
	{symbol_t::WORD_SPACE, "WORD_SPACE"},
	{symbol_t::NOISE, "NOISE"},
	{symbol_t::STUCK_MARK, "STUCK_MARK"},
	{symbol_t::STUCK_SPACE, "STUCK_SPACE"},
	{symbol_t::UNFINISHED, "UNFINISHED"}
};

//! \brief Class codec 
//! 
//! This class provides coding between strings and vector arrays of symbols.
//! 
//! Encoding is done by converting each character in the input string
//! to its Morse code representation using the TO_MORSE map and
//! then converting the Morse code symbols to the corresponding symbol_t values.
//! Unsupported characters are ignored and a warning is logged.
//! 
//! Mapping from Morse code to symbol_t values is as follows:
//! - Not final dot: DOT_MARK, INTERNAL_SPACE
//! - Not final dash: DASH_MARK, INTERNAL_SPACE
//! - Final dot: DOT_MARK, CHARACTER_SPACE or WORD_SPACE
//! - Final dash: DASH_MARK, CHARACTER_SPACE or WORD_SPACE
//! The distinction between character space and word space is made based on the
//! presence of a space character in the input string. 
//! If a space character is encountered or it's the end of the input string,
//! the final symbol for the preceding character is followed by a WORD_SPACE
//! symbol, otherwise it is followed by a CHARACTER_SPACE symbol.
//! 
//! Decoding is done by converting each symbol_t value in the input vector
//! to its corresponding Morse code symbol and then converting the Morse code
//! representation to the corresponding character using the FROM_MORSE map.
//! Badly formed symbol_t sequences that do not correspond to valid
//! Morse code representations are ignored and a warning is logged.
//! Morse code representations that do not correspond to valid characters
//! are represented by the Unicode replacement character (U+FFFD) in the output string.
//! 
//! Mapping from symbol_t values to Morse code is as follows:
//! - DOT_MARK followed by any SPACE: "." added to the Morse code representation of the current character.
//! - DASH_MARK followed by any SPACE: "-" added to the Morse code representation of the current character.
//! - INTERNAL_SPACE: No change to the Morse code representation of the current character.
//! - CHARACTER_SPACE: The Morse code representation of the current character
//!   is complete and is added to the output string as the corresponding character from the FROM_MORSE map.
//!   The Morse code representation for the next character is started.
//! - WORD_SPACE: The Morse code representation of the current character
//!  is complete and is added to the output string as the corresponding character from the FROM_MORSE map.
//!  A space character is added to the output string to represent the word space and the Morse code 
//!  representation for the next character is started.
//! - Any MARK followed by the end of the input vector is treated as if it were followed by a WORD_SPACE symbol, i.e. the Morse code representation of the current character
//! is complete and is added to the output string as the corresponding character from the FROM_MORSE map and a space character is added to the output string to represent the word space.
//! - Any MARK followed by a MARK is treated as badly formed.
//! - INTERNAL_SPACE or CHARACTER_SPACE followed by any SPACE is treated as badly formed.
//! 
class codec
{
public:
	//! Encode a string into a vector of vector of symbol_t values representing the Morse code.
	//! \param input The input string to be encoded.
	//! \param symbols The vector to store the encoded symbol_t values. 
	//!        Each inner vector represents a single character's Morse code.
	static void encode(const std::string& input, std::vector<std::vector<symbol_t>>& symbols);
	//! Decode a vector of symbol_t values representing Morse code into a string.
	//! \param symbols The vector of symbol_t values to be decoded.
	//! \param output The string to store the decoded characters.
	static void decode(const std::vector<symbol_t>& symbols, std::string& output);
};