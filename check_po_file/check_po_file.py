#!/usr/bin/python
# encoding: utf-8
#
# #############################################################################
#
#  Copyright (c) 2010 Peter Körner
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# #############################################################################
#
# Version: 0.8.0
# Author:  Peter Körner <18427@gmx.net>
# Date:    2010-12-05T14:30+01:00
#
# Check PO files' translation strings for mismatched format string specifiers,
#  escape sequences etc.
#
# See the help output for currently available checks.
#
# Assumptions:
#  * Both singular and plural form contain the same items to be checked for
#    mismatches, also in the same order (i.e. plural msgids are ignored)
#  * PO file is formatted correctly and UTF-8 encoded
#  * See help output for the actual items which are checked
#

import codecs
import collections
import getopt
import math
import re
import sys

version = (0, 8, 0, "")


# Available checks
#  Each check has got
#   * [key] a unique one-character ID (corresponding to the command line
#     option character)
#   * a long description
#   * a short description (displayed in help and outfile header)
#   * a type of "match" (items found must match in both original string and
#     translation), "match_count" (same number of items must be in original
#     and translation) or "search" (only search the translation for items)
#   * an item name which is displayed in the output
#   * a setting if the check shall be enabled by default
available_checks = {
        "f" : {"description" : "Find mismatched format specifiers (%<arbitrary character>)",
               "description_short" : "mismatched format specifiers (%<arbitrary character>)",
               "type" : "match",
               "regex" : re.compile(r"%."),
               "item_name" : {"singular" : "format specifier",
                              "plural" : "format specifiers",
                              "unknown" : "format specifier(s)"},
               "default_enabled" : True},
        # Leaving out \' and \" to avoid having too many (bogus) matches ...
        "e" : {"description" : r"Find mismatched escape sequences (\n, \r, \t, \\)",
               "description_short" : r"mismatched escape sequences (\n, \r, \t, \\)",
               "type" : "match",
               "regex" : re.compile(r"\\[nrt\\]"),
               "item_name" : {"singular" : "escape sequence",
                              "plural" : "escape sequences",
                              "unknown" : "escape sequence(s)"},
               "default_enabled" : False},
        # <ellipsis>: \u2026; don't use non-ASCII characters in help output
        "i" : {"description" : "Find mismatched trailing ellipses (..., <ellipsis>",
               "description_short" : "mismatched trailing ellipses (..., <ellipsis>)",
               "type" : "match_count",
               # \u2026 (in this case) is replaced regardless of the string
               #  being a raw string; see
               #  http://docs.python.org/release/2.6.5/reference/lexical_analysis.html#string-literals
               "regex" : re.compile(ur"(\.\.\.|\u2026)$"),
               "item_name" : {"singular" : "trailing ellipsis",
                              "plural" : "trailing ellipses",
                              "unknown" : "trailing ellipsis/ellipses"},
               "default_enabled" : False},
        "w" : {"description" : "Find mismatched leading/trailing whitespace (space, tab)",
               "description_short" : "mismatched leading/trailing whitespace (space, tab)",
               "type" : "match",
               "regex" : re.compile(r"^[ \t]+|[ \t]+$"),
               "item_name" : {"singular" : "occurrence of leading/trailing whitespace",
                              "plural" : "occurrences of leading/trailing whitespace",
                              "unknown" : "occurrence(s) of leading/trailing whitespace"},
               "default_enabled" : False},
        "a" : {"description" : "Find mismatched keyboard accelerators (&<non-&/whitespace character>)",
               "description_short" : "mismatched keyboard accelerators (&<non-&/whitespace character>)",
               "type" : "match_count",
               "regex" : re.compile(r"(?<!&)&[^\s&]"),
               "item_name" : {"singular" : "keyboard accelerator",
                              "plural" : "keyboard accelerators",
                              "unknown" : "keyboard accelerator(s)"},
               "default_enabled" : False},
        "s" : {"description" : "Find double spaces",
               "description_short" : "double spaces",
               "type" : "search",
               "regex" : re.compile(r"  "),
               "item_name" : {"singular" : "double space",
                              "plural" : "double spaces",
                              "unknown" : "double space(s)"},
               "default_enabled" : True},
        "l" : {"description" : "Find space-linebreak escape sequence combinations ( <\\n or \\r>)",
               "description_short" : "space-linebreak escape sequence combinations ( <\\n or \\r>)",
               "type" : "search",
               "regex" : re.compile(r" \\[nr]"),
               "item_name" : {"singular" : "space-linebreak escape sequence combination",
                              "plural" : "space-linebreak escape sequence combinations",
                              "unknown" : "space-linebreak escape sequence combination(s)"},
               "default_enabled" : True},
        "t" : {"description" : "Find tab characters",
               "description_short" : "tab characters",
               "type" : "search",
               "regex" : re.compile(r"\t"),
               "item_name" : {"singular" : "tab character",
                              "plural" : "tab characters",
                              "unknown" : "tab character(s)"},
               "default_enabled" : False}
}


# Try to adhere to stdout's encoding if it's set (normal call of the script),
#  use UTF-8 otherwise (redirection to a file, pipe, ...).
encoding = sys.stdout.encoding if sys.stdout.encoding is not None else "UTF-8"


def exit_with_error(message, show_help_message):
    """Print a message, optionally the help text, then exit."""

    print >> sys.stderr, message
    if show_help_message:
        print_help()
    sys.exit(1)


def print_help():
    """Print the help text."""

    help_text = """Usage: %s [OPTION ...] FILE ...

Arguments:
  PO file(s) to check (file encoding: UTF-8)

File checking options:
  Enabling/disabling checks:
  -a, --all-checks          Perform all checks
  -e XYZ, --enable=XYZ      Perform only checks X, Y, Z
  -d XYZ, --disable=XYZ     Perform all checks except X, Y, Z

  Default if none of -a, -e, -d is given: perform checks marked with *.
  -a, -e and -d are mutually exclusive.

  Available checks: Find...\n"""  % sys.argv[0]

    for check_id in sorted(available_checks.keys()):
        check = available_checks[check_id]
        help_text += "   %s%s %s\n" % (
                        check_id,
                        "*" if check["default_enabled"] else " ",
                        check["description_short"])

    help_text += """
  Miscellaneous options:
  -b, --ignore-bom          Do not abort checking if a BOM was found
  -s, --summary             Show summary

General options:
  -h, --help                Display this help text
  -v, --version             Display version information"""

    print help_text


def print_version():
    """Print version information."""

    print sys.argv[0], "%d.%d.%d%s" % version


def print_enc(text, encoding):
    """Print a unicode string using the specified encoding."""

    try:
        sys.stdout.write(text.encode(encoding))
    except UnicodeEncodeError:
        exit_with_error("\nError: Your terminal's encoding cannot represent "
            + "some of the characters to be displayed. You may switch it "
            + "to UTF-8 or redirect the output to a file (forcing UTF-8 "
            + "output).",
            False)


def print_finding(check, path, message, string_pair, items_found, items_expected=None):
    """Print a finding (i.e. translation entry + items found, ...).

    Depending on the type of the check, print various pieces of information,
    like file path, line number, supplied message, strings and items (i.e.
    specifiers, ...)/number of items involved.
    """

    # Print items found
    if check["type"] == "match":
        print_enc(u"\n%s: -- Potential mismatch: %s - expected [%s], got [%s]\n" % (
                  path,
                  message,
                  u", ".join([u"\"%s\"" % x for x in items_expected]),
                  u", ".join([u"\"%s\"" % x for x in items_found])), encoding)
    elif check["type"] == "match_count":
        print_enc(u"\n%s: -- Potential mismatch: %s - expected %d, got %d\n" % (
                  path,
                  message,
                  len(items_expected),
                  len(items_found)), encoding)
    else:
        print_enc(u"\n%s: -- %s: [%s]\n" % (
                  path,
                  message,
                  u", ".join([u"\"%s\"" % x for x in items_found])), encoding)

    # Print original string and the translation which has been checked
    print_enc(u"%s:%d: %s\n" % (path, string_pair[0][0], string_pair[0][1]), encoding)
    print_enc(u"%s:%d: %s\n" % (path, string_pair[1][0], string_pair[1][1]), encoding)


def print_summary(available_checks, check_ids, data, lines_between_headers):
    """Print a summary of data gathered while processing the PO file(s).

    available_checks must contain all available checks' data, check_ids the IDs
    of checks executed. data has to be a dictionary with file paths as keys and
    dictionaries - which contain check IDs and "sum" as keys and the number of
    findings for the respective check/the sum of all findings as values - as
    values. If a file does not have finding numbers (got skipped, ...), None
    may be supplied instead of a dictionary containing the number of findings.
    The column headers are inserted between every block of
    lines_between_headers rows in the findings table (0 to disable intermediate
    headers).
    """

    def generate_header_line(check_ids, widths):
        """Generate a line of column headers."""

        line = []
        line.append("#")
        # Add one space of padding after the path
        line.append(" " * widths["path"] + " ")

        for check_id in sorted(check_ids):
            line.append(check_id.rjust(widths["findings"]))

        line.append("sum".rjust(widths["sum"]))

        return " ".join(line)

    def generate_path_line(path, check_ids, data, widths):
        """Generate a line of output for a specific path."""

        line = []
        findings = data[path]

        line.append("#")
        # Add one space of padding after the path
        line.append(path.rjust(widths["path"]) + " ")

        if findings is not None:
            for check_id in sorted(check_ids):
                current_findings = findings[check_id]
                if current_findings > 0:
                    line.append(str(current_findings).rjust(widths["findings"]))
                else:
                    line.append("-".rjust(widths["findings"]))

            line.append(str(findings["sum"]).rjust(widths["sum"]))
        else:
            for check_id in check_ids:
                line.append("X".rjust(widths["findings"]))
            line.append("X".rjust(widths["sum"]))
            line.append(" (skipped)")

        return " ".join(line)


    # Calculate total number of findings for each check as well as the sum for
    #  all checks; use a dictionary structured like the data dictionary to be
    #  able to use generate_path_line() later on
    total_findings = {"sum" : collections.defaultdict(int)}
    for findings in data.values():
        if findings is not None:
            for check_id in check_ids:
                total_findings["sum"][check_id] += findings[check_id]
            total_findings["sum"]["sum"] += findings["sum"]


    # Get column widths;
    #  sum column needs to be at least three characters wide (for the heading);
    #  also add some padding
    widths = {"path" : 1, "findings" : 1, "sum" : 5}
    for path in data.keys():
        widths["path"] = max(widths["path"], len(path))

    for check_id in check_ids:
        number_width = int(math.log10(max(1, total_findings["sum"][check_id]))) + 1
        widths["findings"] = max(widths["findings"], number_width)

    number_width = int(math.log10(max(1, total_findings["sum"]["sum"]))) + 1
    widths["sum"] = max(widths["sum"], number_width)


    # Get and calculate numbers of files processed/checked/skipped
    file_count = {}
    file_count["total"] = len(data.keys())
    file_count["skipped"] = len(filter(lambda x: x is None, data.values()))
    file_count["checked"] = file_count["total"] - file_count["skipped"]


    # Summary heading
    print "\n# Summary"
    print "# ======="
    print "#"


    # Findings table heading
    print "# Findings per check and file"
    print "# ---------------------------"

    # Column headers, file paths and actual numbers;
    #  insert an empty line and column headers every lines_between_headers
    #  printed lines; make sure the first one is printed even if
    #  lines_between_headers == 0, i.e. intermediate headers are disabled
    if lines_between_headers == 0:
        lines_between_headers = len(data.keys())
    for line_number, path in enumerate(sorted(data.keys())):
        if (line_number % lines_between_headers) == 0:
            print "#"
            print generate_header_line(check_ids, widths)
            print "#"
        print generate_path_line(path, check_ids, data, widths)
    print "#"
    # Only display totals line if more than one file has been checked
    if len(data.keys()) > 1:
        print generate_header_line(check_ids, widths)
        print "#"
        print generate_path_line("sum", check_ids, total_findings, widths)
        print "#"


    # Legend
    print "# Legend:"
    for check_id in sorted(check_ids):
        print "#  %s: %s" % (check_id, available_checks[check_id]["description_short"])
    print "#"

    # Note on reliability
    print "# Note: All of the above numbers/findings may include false"
    print "#       positives/negatives."
    print "#"
    print "#"

    # Number of files processed/checked/skipped
    print "# Files processed"
    print "# ---------------"
    print "#"
    print "# Processed %d file(s) (checked %d, skipped %d)" % (
            file_count["total"], file_count["checked"], file_count["skipped"])
    print "#"


def check_for_mismatch(check, items_found, items_expected):
    """Check two item lists for mismatches.

    Return the result and a message describing the mismatch.
    """

    mismatch_found = False
    message = ""

    if len(items_expected) > len(items_found):
        message = u"too few %s" % check["item_name"]["plural"]
        mismatch_found = True
    elif len(items_expected) < len(items_found):
        message = u"too many %s" % check["item_name"]["plural"]
        mismatch_found = True
    # Don't check for exact matches if not needed
    elif check["type"] != "match_count" and items_expected != items_found:
        message = u"unexpected %s" % check["item_name"]["unknown"]
        mismatch_found = True

    return mismatch_found, message


def check_file(path, checks, ignore_bom):
    """Execute checks on the non-fuzzy, non-obsolete entries of the file.

    Returns a tuple;
    first element: True if an error occured (file could not be parsed and
    processed, ...) or anything mismatching etc. has been found, False
    otherwise (note that the checks may find too much or too little due to
    simplifications made);
    second element: dictionary with number of findings per check and a sum over
    all executed checks if no error occurred, None otherwise (same note
    applies).

    Ignores an existing byte order mark (and displays a warning) if ignore_bom
    is True, displays an error message and exits otherwise. Also exits when the
    file could not be read.
    """

    # Get the PO file's lines
    try:
        f = codecs.open(path, encoding="utf-8", mode="r")
    except IOError, (errno, strerror):
        exit_with_error("\nError: Cannot read file \"%s\" (%s)" % (path, strerror), False)
    else:
        lines = f.read().splitlines()
        f.close()


    # Search for byte order mark and handle it appropriately, depending on if
    #  it's to be ignored or not
    if lines[0].startswith(codecs.BOM_UTF8.decode("utf-8")):
        if ignore_bom:
            print "\n%s: W  Warning: Byte order mark (BOM) found." % path
            print "%s:    Please save the .po file UTF-8 encoded, without BOM." % path
            print "%s:    Ignoring the BOM for now (called with -b)." % path
            lines[0] = lines[0][1:]
        else:
            print "\n%s: E  Error: Byte order mark (BOM) found." % path
            print "%s:    Please save the .po file UTF-8 encoded, without BOM." % path
            print "%s:    To temporarily ignore the BOM, call this script with -b." % path
            print "%s:    Skipping this file." % path
            return True, None


    # Add line numbers and remove empty lines
    lines = [[index + 1, line] for index, line in enumerate(lines[:]) if len(line) > 0]


    # Join lines
    joined_lines = []
    current_line = u""
    current_line_number = 0
    for line_number, line in lines:
        if line.startswith(u"\""):
            current_line = current_line[:-1] + line[1:]
        else:
            joined_lines.append([current_line_number, current_line])
            current_line = line
            current_line_number = line_number

    # Remove first line if it's empty
    if len(joined_lines[0][1]) == 0:
        del joined_lines[0]

    # Add last line if it's not empty
    if len(current_line) > 0:
        joined_lines.append([current_line_number, current_line])

    lines = joined_lines


    # Collect the entries
    entries = []
    current_entry = []
    # Use a DFA for extracting the interesting elements which belong to
    #  an entry.
    # Takes the beginning (and the rest, in case of fuzzy) of the line into
    #  account to decide on the transition and additional action to perform.
    # Note: This DFA does *not* validate the file's syntax - it's simplified
    #  quite a bit; also there seems to be no real specification of PO files...
    #
    # Accepted syntax:
    #  (comment* fuzzy? comment* (msgctxt? msgid+ msgstr+ | obsolete obsolete+))*
    #
    # Requiring two or more obsolete (#~) elements in succession is one of the
    #  simplifications - the actual type (ctxt, id, str) is not checked in
    #  this case.
    #
    # Loosely based on examples at
    #  http://www.gnu.org/software/gettext/manual/gettext.html#PO-Files
    #
    # Note: Dropping "msgid_plural"s and the likes is done by just doing a
    #  transition, but no further action for the respective line.
    #
    # DFA transitions:
    #
    #  line_type  line content
    #          0  comment:   #\n|(# |#.|#:|#\|)<substring>\n|#,<substring not containing "fuzzy">\n
    #          1  fuzzy:     #,<substring containing "fuzzy">\n
    #          2  obsolete:  #~<substring>\n
    #          3  msgctxt:   msgctxt<substring>\n
    #          4  msgid:     msgid<substring>\n
    #          5  msgstr:    msgstr<substring>\n
    #
    #  (Actual line ending is not checked - only the beginning of the line is
    #   used -, but the lines are already split up, so effectively there's an
    #   \n.)
    #
    #  State |  comment     fuzzy   obsolete    msgctxt msgid   msgstr
    #  ------+--------------------------------------------------------
    #      0 |  0           4       8           1       2 *     10
    #      1 |  10          10      10          10      2 *     10
    #      2 |  10          10      10          10      2       3 *
    #      3 |  0           4       8           1       2 *     3 *
    #      4 |  4           10      8           5       6       10
    #  ------+--------------------------------------------------------
    #      5 |  10          10      10          10      6       10
    #      6 |  10          10      10          10      6       7
    #      7 |  0           4       8           1       2 *     7
    #      8 |  10          10      9           10      10      10
    #      9 |  0           4       9           1       2 *     10
    #  ------+--------------------------------------------------------
    #   E/10 |  10          10      10          10      10      10
    #
    #  *: handle corresponding line
    #
    # Transitions to actually use;
    #  first tuple entry: next state, second entry: perform action?
    transitions = [
        [ (0, False),  (4, False),  (8, False),  (1, False),  (2, True),  (10, False)],
        [(10, False), (10, False), (10, False), (10, False),  (2, True),  (10, False)],
        [(10, False), (10, False), (10, False), (10, False),  (2, False),  (3, True)],
        [ (0, False),  (4, False),  (8, False),  (1, False),  (2, True),   (3, True)],
        [ (4, False), (10, False),  (8, False),  (5, False),  (6, False), (10, False)],
        [(10, False), (10, False), (10, False), (10, False),  (6, False), (10, False)],
        [(10, False), (10, False), (10, False), (10, False),  (6, False),  (7, False)],
        [ (0, False),  (4, False),  (8, False),  (1, False),   (2, True),  (7, False)],
        [(10, False), (10, False),  (9, False), (10, False), (10, False), (10, False)],
        [ (0, False),  (4, False),  (9, False),  (1, False),   (2, True), (10, False)],
        [(10, False), (10, False), (10, False), (10, False), (10, False), (10, False)]
    ]

    state = 0
    line_type = 0
    for line_number, line in lines:

        # Determine the current line's type
        #  Reordered for better performance
        if line.startswith((u"#:", u"# ", u"#.", u"#|")) or line == u"#":
            line_type = 0
        elif line.startswith(u"#,"):
            if u"fuzzy" in line:
                line_type = 1
            else:
                line_type = 0
        elif line.startswith(u"msgid"):
            line_type = 4
        elif line.startswith(u"msgstr"):
            line_type = 5
        elif line.startswith(u"msgctxt"):
            line_type = 3
        elif line.startswith(u"#~"):
            line_type = 2
        else:
            line_type = 0
            state = 10      # Error state, won't be left on the next transition

        transition = transitions[state][line_type]

        # Perform an action if necessary (handle the current line/entry)
        # Could be nicer...
        if transition[1]:
            # Strip prefix ("msgid \"" etc.) and suffix ("\"") and append both
            #  original and the resulting string to the current translation
            #  entry.
            #  Assumes that the strings are properly enclosed in quotation marks
            content_start = line.find("\"") + 1
            if line_type == 4:
                entries.append(current_entry[:])
                current_entry = []
                current_entry.append([line_number, line, line[content_start:-1]])
            elif line_type == 5:
                current_entry.append([line_number, line, line[content_start:-1]])

        state = transition[0]

        # Has the error state been reached?
        if state == 10:
            print_enc(u"\n%s: E  Parsing error. Invalid PO file?\n" % path, encoding)
            print_enc(u"%s:%d: %s\n" % (path, line_number, line), encoding)
            return True, None


    # Remove first entry if it's empty
    if len(entries[0]) == 0:
        del entries[0]

    # Add last entry if it's not empty
    if len(current_entry) > 0:
        entries.append(current_entry[:])


    # Any findings?
    found_something = False

    number_of_findings = collections.defaultdict(int)

    # Perform tests, extracting items from the strings etc.
    for entry in entries:
        # Skip string if the msgid is empty (check stripped version)
        if len(entry[0][2]) < 1:
            continue

        # Dictionary for collecting the expected items; keys: check IDs,
        #  values: lists of expected items per check
        items_expected = {}

        # Extract specifiers, escape sequences, ... from first string ...
        for check_id, check in checks["match*"]:
            items_expected[check_id] = check["regex"].findall(entry[0][2])

        # ... and check if all of them are existing in the other strings
        for line_number, translation, translation_stripped in entry[1:]:
            # Skip translation if it's empty
            if len(translation_stripped) < 1:
                continue

            # Extract specifiers, escape sequences, ... from translation, check
            #  for mismatches and output them if needed
            for check_id, check in checks["match*"]:
                items_found = check["regex"].findall(translation_stripped)
                mismatch_found, message = check_for_mismatch(check,
                                                             items_found,
                                                             items_expected[check_id])
                if mismatch_found:
                    found_something = True
                    number_of_findings[check_id] += 1
                    print_finding(check,
                                  path,
                                  message,
                                  (entry[0][0:2], [line_number, translation]),
                                  items_found,
                                  items_expected[check_id])

            # Search for additional items of interest and output them if found
            for check_id, check in checks["search"]:
                items_found = check["regex"].findall(translation_stripped)
                if len(items_found) > 0:
                    found_something = True
                    number_of_findings[check_id] += 1
                    print_finding(check,
                                  path,
                                  "Found " + check["item_name"]["unknown"],
                                  (entry[0][0:2], [line_number, translation]),
                                  items_found)

    number_of_findings["sum"] = sum(number_of_findings.values())
    return found_something, number_of_findings


if __name__ == "__main__":

    # Command line option/argument parsing

    # Short and long command line options
    # ":" means "requires argument"
    short_options =  "hvae:d:bs"
    long_options = ["help", "version", "all-checks", "enable=", "disable=", "ignore-bom", "summary"]

    # Parse command line
    try:
        (options, arguments) = getopt.gnu_getopt(sys.argv[1:], short_options, long_options)
    except getopt.GetoptError, e:
        exit_with_error("Error: " + str(e), True)

    # Sets of IDs of the enabled/disabled checks
    enabled_check_ids = set()
    disabled_check_ids = set()

    # Ignore byte order mark?
    ignore_bom = False

    # Show summary?
    show_summary = False

    # Process specified options
    check_options_specified = set()
    for option, value in options:
        if option in ("-h", "--help"):
            print_help()
            sys.exit(0)
        elif option in ("-v", "--version"):
            print_version()
            sys.exit(0)
        elif option in ("-a", "--all-checks"):
            enabled_check_ids.update(available_checks.keys())
            check_options_specified.add("a")
        elif option in ("-e", "--enable"):
            enabled_check_ids.update(list(value))
            check_options_specified.add("e")
        elif option in ("-d", "--disable"):
            disabled_check_ids.update(list(value))
            check_options_specified.add("d")
        elif option in ("-b", "--ignore-bom"):
            ignore_bom = True
        elif option in ("-s", "--summary"):
            show_summary = True


    # Still not exited -> files shall be processed ...

    # Make sure there actually are file arguments
    if len(arguments) < 1:
        exit_with_error("Error: file name(s) missing", True)


    # Process check ID options

    # Check that -a, -e and -d are not specified at the same time
    if len(check_options_specified) > 1:
        exit_with_error("Error: -a, -e and -d are mutually exclusive", True)

    # Check for invalid check IDs
    invalid_check_ids = (enabled_check_ids | disabled_check_ids) - set(available_checks.keys())
    if len(invalid_check_ids) > 0:
        exit_with_error("Error: invalid check(s): " + "".join(sorted([x for x in invalid_check_ids])),
                        True)

    # If checks shall be disabled, subtract the to-be-disabled ones from the
    #  available checks; if none of -a, -e or -d is given, enable default checks
    if len(disabled_check_ids) > 0:
        enabled_check_ids = set(available_checks.keys()) - disabled_check_ids
    elif not len(enabled_check_ids) > 0:
        enabled_check_ids = set([check_id for check_id in available_checks
                                 if available_checks[check_id]["default_enabled"]])

    # There should be at least one enabled check ...
    if len(enabled_check_ids) < 1:
        exit_with_error("Error: no checks to be processed", True)

    # Done collecting enabled checks' IDs into enabled_checks_ids.
    #  Now create a dictionary containing two lists of checks and their IDs -
    #  one for "match"-like checks, one for "search" checks. The actual checks
    #  are sorted by their IDs to generate a "stable" output, i.e. perform them
    #  in the same order every time the script is executed (apart from checks
    #  being enabled/disabled and thus their output being added/removed).
    enabled_checks_all = [(x, available_checks[x]) for x in sorted(enabled_check_ids)]
    enabled_checks = {"match*" : [x for x in enabled_checks_all if x[1]["type"] in ("match", "match_count")],
                      "search" : [x for x in enabled_checks_all if x[1]["type"] == "search"]}


    # Print the encoding and enabled checks
    print "# Encoding: %s" % encoding
    print "#"
    print "# Enabled checks: Find..."
    for check_id in sorted(list(enabled_check_ids)):
        print "#  %s: %s" % (check_id, available_checks[check_id]["description_short"])
    print "#"

    # Was there any error or have any mismatches/... been found?
    problems_found = False

    summary_data = {}

    # Check (unique) files specified as arguments
    paths = sorted(list(set(arguments)))
    for path in paths:
        new_problems, file_summary_data = check_file(path, enabled_checks, ignore_bom)
        problems_found = new_problems or problems_found
        summary_data[path] = file_summary_data

    if show_summary:
        print_summary(available_checks, enabled_check_ids, summary_data, 10)

    sys.exit(1 if problems_found else 0)
