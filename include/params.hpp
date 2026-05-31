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

#include "zc_file_holder.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

//! Additional file IDs for the file holder
enum file_types : uint8_t {
	FILE_TEXT_FILE = FILE_USER, //!< Text file for option 4 in text generation
	FILE_WORD_LIST,                 //!< Word list for option 5 in text generation
	FILE_QSO_DATA,				 //!< Data for generating QSO exchanges in option 6 in text generation
};

//! Enumerated type for the different content modes
enum class content_mode
{
	LETTERS,   //!< Random letters mode
	NUMBERS,   //!< Random numbers mode
	MIXED,     //!< Random mixed characters mode
	WORD_LIST,  //!< Random words from word list
	TEXT_ONLY,       //!< Text only mode
	TEXT_PUNCTUATION,   //!< Text with punctuation mode
	QSO,       //!< Random QSOs mode
	CALLSIGNS,  //!< Random callsigns mode
	USER_TEXT, //!< User-specified text mode
	TEST_MODE_A, //!< Test mode A - continually send the same word "VVV" for test purposes
	COUNT      //!< Number of content modes
};
//! Map content mode enum to displayed strings
static inline const std::map<content_mode, std::string> content_mode_strings_ = {
	{content_mode::LETTERS, "5-letter groups"},
	{content_mode::NUMBERS, "5-number groups"},
	{content_mode::MIXED, "5-mixed groups"},
	{content_mode::WORD_LIST, "Word List"},
	{content_mode::TEXT_ONLY, "Text Only"},
	{content_mode::TEXT_PUNCTUATION, "Text + Punctuation"},
	{content_mode::QSO, "QSOs"},
	{content_mode::CALLSIGNS, "Callsigns"},
	{content_mode::USER_TEXT, "User Text"},
	{content_mode::TEST_MODE_A, "Test Mode A (VVV)"},
};

//! Enumerated type for the different speed types
enum class speed_type
{
	NORMAL,    //!< Raw WPM speed
	FARNSWORTH, //!< Farnsworth spacing
	WORDSWORTH, //!< Wordsworth spacing
	COUNT       //!< Number of speed types
};
//! Map speed type enum to displayed strings
static inline const std::map<speed_type, std::string> speed_type_strings_ = {
	{speed_type::NORMAL, "Normal"},
	{speed_type::FARNSWORTH, "Farnsworth"},
	{speed_type::WORDSWORTH, "Wordsworth"},
};

//! Enumerated type for the different disturber types
enum class disturber_type
{
	NONE,         //!< No disturbance
	TIMING,       //!< Timing disturbance
	SOFTNESS,     //!< Rise/fall time disturbance
	NOISE_WHITE,  //!< White noise disturbance
	NOISE_IMPACT, //!< Impact noise disturbance
	NOISE_TONES,  //!< Tones noise disturbance ("plinks")
	DRIFT_STEADY,        //!< Frequency drift disturbance
	DRIFT_CYCLIC,        //!< Cyclic frequency drift disturbance
	FADING,               //!< Fading disturbance (signal strength varies over time)
	COUNT         //!< Number of disturber types
};
//! Map disturber type enum to displayed strings
static inline const std::map<disturber_type, std::string> disturber_type_strings_ = {
	{disturber_type::NONE, "None"},
	{disturber_type::TIMING, "Timing"},
	{disturber_type::SOFTNESS, "Softness"},
	{disturber_type::NOISE_WHITE, "White Noise"},
	{disturber_type::NOISE_IMPACT, "Impact Noise"},
	{disturber_type::NOISE_TONES, "Tones (Plinks)"},
	{disturber_type::DRIFT_STEADY, "Frequency Drift"},
	{disturber_type::DRIFT_CYCLIC, "Cyclic Drift"},
	{disturber_type::FADING, "Fading (QSB)"},
};


//! Enumerated type for the different text sources
enum class text_source_t : uint8_t {
	NO_TEXT,
	SENT_TEXT,
	TYPED_TEXT,
	DECODED_FIRST,
	DECODED_NONE = DECODED_FIRST,
	DECODED_SENT_AUDIO,
	DECODED_MIC_AUDIO,
	DECODED_LAST,
	COUNT = DECODED_LAST
};
//! Map text source enum to displayed strings
//! Note that the text sources that are used for decoding results 
//! are grouped together in the middle of the enum, so that they 
//! can be easily checked for in the code.
static inline const std::map<text_source_t, std::string> text_source_strings_ = {
	{text_source_t::NO_TEXT, "No Text"},
	{text_source_t::SENT_TEXT, "Sent Text"},
	{text_source_t::TYPED_TEXT, "Typed Text"},
	{text_source_t::DECODED_NONE, "No Audio"},
	{text_source_t::DECODED_SENT_AUDIO, "Sent Audio"},
	{text_source_t::DECODED_MIC_AUDIO, "Microphone"},
};


// JSON serialization for enums using the shared maps
namespace nlohmann {
	template <>
	struct adl_serializer<content_mode> {
		static void to_json(json& j, const content_mode& mode) {
			j = content_mode_strings_.at(mode);
		}
		static void from_json(const json& j, content_mode& mode) {
			auto str = j.get<std::string>();
			for (const auto& [val, name] : content_mode_strings_) {
				if (name == str) {
					mode = val;
					return;
				}
			}
			throw std::invalid_argument("Invalid content_mode: " + str);
		}
	};

	template <>
	struct adl_serializer<speed_type> {
		static void to_json(json& j, const speed_type& type) {
			j = speed_type_strings_.at(type);
		}
		static void from_json(const json& j, speed_type& type) {
			auto str = j.get<std::string>();
			for (const auto& [val, name] : speed_type_strings_) {
				if (name == str) {
					type = val;
					return;
				}
			}
			throw std::invalid_argument("Invalid speed_type: " + str);
		}
	};

	template <>
	struct adl_serializer<disturber_type> {
		static void to_json(json& j, const disturber_type& type) {
			j = disturber_type_strings_.at(type);
		}
		static void from_json(const json& j, disturber_type& type) {
			auto str = j.get<std::string>();
			for (const auto& [val, name] : disturber_type_strings_) {
				if (name == str) {
					type = val;
					return;
				}
			}
			throw std::invalid_argument("Invalid disturber_type: " + str);
		}
	};
}

