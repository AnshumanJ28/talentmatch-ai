#pragma once
#include <string>
#include <vector>
#include <regex>

namespace talentmatch {

struct Section {
    std::string name;       // normalized section name
    int         start_line; // inclusive
    int         end_line;   // exclusive
    std::string text;       // raw text of the section
};

// ---------------------------------------------------------------------------
// SectionDetector
//
// Scans resume plain-text line by line and identifies standard sections
// by matching heading patterns. Handles common variants:
//   "WORK EXPERIENCE", "Professional Experience", "Experience", etc.
// ---------------------------------------------------------------------------
class SectionDetector {
public:
    SectionDetector();

    // Detects all sections in the resume text.
    // The last section extends to the end of the document.
    std::vector<Section> detect(const std::string& text) const;

    // Returns text before the first recognized section (usually contact block)
    std::string preamble(const std::string& text) const;

private:
    // Returns the normalized section name if the line is a heading, else ""
    std::string classify_heading(const std::string& line) const;

    struct HeadingPattern {
        std::vector<std::string> keywords;  // any of these trigger the section
        std::string              canonical; // the name we normalize to
    };

    std::vector<HeadingPattern> patterns_;
    std::regex                  all_caps_re_;   // ALL-CAPS line detector
};

} // namespace talentmatch
