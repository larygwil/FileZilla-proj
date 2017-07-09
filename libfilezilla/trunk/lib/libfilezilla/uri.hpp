#ifndef LIBFILEZILLA_URI_HEADER
#define LIBFILEZILLA_URI_HEADER

#include "libfilezilla.hpp"

#include <string>

/** \file
 * \brief Declares fz::uri for (de)composing URIs.
 */

namespace fz {

/**
 * \brief The uri class is used to decompose URIs into their individual components.
 *
 * Implements Uniform Resource Identifiers as described in RFC 3986
 */
class FZ_PUBLIC_SYMBOL uri final
{
public:
	uri() = default;
	explicit uri(std::string const& in);

	void clear();

	/**
	 * \brief Splits uri into components.
	 *
	 * Percent-decodes username, pass, host and path
	 * Does not decode query and fragment.
	 **/
	bool parse(std::string in);

	/**
	 * \brief Assembles components into string
	 *
	 * Percent-encodes username, pass, host and path
	 * Does not encode query and fragment.
	 */
	std::string to_string() const;

	/// \brief Returns path and query, separated by question mark.
	std::string get_request() const;

	/// \brief Returns [user[:pass]@]host[:port]
	std::string get_authority(bool with_userinfo) const;

	bool empty() const;

	/// Often refered to as the protocol prefix, e.g. ftp://
	std::string scheme_;
	std::string user_;
	std::string pass_;
	std::string host_;
	unsigned short port_{};
	std::string path_;

	/// THe part of a URI after ? but before #
	std::string query_;

	/// The part of a URI after #
	std::string fragment_;

	/// \brief Checks that the URI is absolut, that is the path starting with a slash.
	bool is_absolute() const { return path_[0] == '/'; }

	/**
	 * \brief Resolve a relative URI reference into an absolute URI given a base URL.
	 *
	 * If the URI is not relative or from a different scheme, it is not changed.
	 */
	void resolve(uri const& base);
private:
	bool parse_authority(std::string && authority);
};

}

#endif
