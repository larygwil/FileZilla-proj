#ifndef LIBFILEZILLA_ENCODE_HEADER
#define LIBFILEZILLA_ENCODE_HEADER

#include "libfilezilla.hpp"

#include <string>
#include <vector>

/** \file
 * \brief Functions to encode/decode strings
 *
 * Defines functions to deal with hex, base64 and percent encoding.
 */

namespace fz {

/** \brief Converts a hex digit to decimal int
 *
 * Example: '9' becomes 9, 'b' becomes 11, 'D' becomes 13
 *
 * Returns -1 if input is not a valid hex digit.
 */
template<typename Char>
int hex_char_to_int(Char c)
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'Z') {
		return c - 'A' + 10;
	}
	else if (c >= '0' && c <= '9') {
		return c - '0';
	}
	return -1;
}

template<typename String>
std::vector<uint8_t> hex_decode(String const& in)
{
	std::vector<uint8_t> ret;
	if (!(in.size() % 2)) {
		ret.reserve(in.size() / 2);
		for (size_t i = 0; i < in.size(); i += 2) {
			int high = hex_char_to_int(in[i]);
			int low = hex_char_to_int(in[i + 1]);
			if (high == -1 || low == -1) {
				return std::vector<uint8_t>();
			}
			ret.push_back(static_cast<uint8_t>((high << 4) + low));
		}
	}

	return ret;
}

/** \brief Converts an integer to the corresponding lowercase hex digit
*
* Example: 9 becomes '9', 11 becomes 'b'
*
* Undefined output if input is less than 0 or larger than 15
*/
template<typename Char = char, bool Lowercase = true>
Char int_to_hex_char(int d)
{
	if (d > 9) {
		return static_cast<Char>((Lowercase ? 'a' : 'A') + d - 10);
	}
	else {
		return static_cast<Char>('0' + d);
	}
}

template<typename String, typename InString, bool Lowercase = true>
String hex_encode(InString const& data)
{
	static_assert(sizeof(typename InString::value_type) == 1, "Input must be a container of 8 bit values");
	String ret;
	ret.reserve(data.size() * 2);
	for (auto const& c : data) {
		ret.push_back(int_to_hex_char<typename String::value_type, Lowercase>(static_cast<unsigned char>(c) >> 4));
		ret.push_back(int_to_hex_char<typename String::value_type, Lowercase>(static_cast<unsigned char>(c) & 0xf));
	}

	return ret;
}

/// \brief Encodes raw input string to base64
std::string FZ_PUBLIC_SYMBOL base64_encode(std::string const& in);

/// \brief Decodes base64, ignores whitespace. Returns empty string on invalid input.
std::string FZ_PUBLIC_SYMBOL base64_decode(std::string const& in);


/**
 * \brief Percent-enodes string.
 *
 * The characters A-Z, a-z, 0-9, hyphen, underscore, period, tilde are not percent-encoded, optionally slashes arne't encoded either.
 * Every other character is encoded.
 *
 * \param keep_slashes If set, slashes are not encoded.
 */
std::string FZ_PUBLIC_SYMBOL percent_encode(std::string const& s, bool keep_slashes = false);
std::string FZ_PUBLIC_SYMBOL percent_encode(std::wstring const& s, bool keep_slashes = false);

/**
 * \brief Percent-enodes wide-character. Non-ASCII characters are converted to UTF-8 befor they are encoded.
 *
 * \sa \ref fz::percent-encode
 */
std::wstring FZ_PUBLIC_SYMBOL percent_encode_w(std::wstring const& s, bool keep_slashes = false);

/**
 * \brief Percent-decodes string.
 *
 * If the string cannot be decoded, an empty string is returned.
 */
std::string FZ_PUBLIC_SYMBOL percent_decode(std::string const& s);

}

#endif
