#include "section_detector.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace talentmatch {

// ---------------------------------------------------------------------------
// All recognized heading patterns → canonical section names
// ---------------------------------------------------------------------------
SectionDetector::SectionDetector()
    : all_caps_re_(R"(^[A-Z][A-Z\s&\/\-:]{3,}$)")
{
    patterns_ = {
        {{"summary", "objective", "profile", "about me", "professional summary",
          "career objective", "executive summary"},
         "SUMMARY"},
        {{"experience", "work experience", "employment", "work history",
          "professional experience", "career history", "employment history",
          "work & experience", "relevant experience"},
         "EXPERIENCE"},
        {{"education", "academic background", "academic history",
          "educational background", "qualifications", "academic qualifications"},
         "EDUCATION"},
        {{"skills", "technical skills", "core competencies", "competencies",
          "key skills", "skills & expertise", "areas of expertise",
          "technical expertise", "technologies", "skill set",
          "tools & technologies", "proficiencies"},
         "SKILLS"},
        {{"projects", "personal projects", "key projects", "project experience",
          "notable projects", "selected projects", "portfolio", "academic projects"},
         "PROJECTS"},
        {{"certifications", "certificates", "licenses", "professional certifications",
          "training & certifications", "accreditations"},
         "CERTIFICATIONS"},
        {{"awards", "achievements", "honors", "accomplishments",
          "recognition", "awards & achievements"},
         "AWARDS"},
        {{"publications", "research", "papers", "conference papers"},
         "PUBLICATIONS"},
        {{"languages", "language proficiency"},
         "LANGUAGES"},
        {{"volunteer", "volunteering", "community"},
         "VOLUNTEER"},
        {{"references", "professional references"},
         "REFERENCES"},
    };
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool is_short_line(const std::string& line) {
    // Headings are usually shorter than ~60 chars
    return line.size() <= 60;
}

// ---------------------------------------------------------------------------
// classify_heading
//
// Returns canonical section name if the line looks like a section heading,
// otherwise returns "".
// ---------------------------------------------------------------------------
std::string SectionDetector::classify_heading(const std::string& raw_line) const {
    std::string line = trim(raw_line);
    if (line.empty() || !is_short_line(line)) return "";

    // Strip trailing colon common in headings ("EXPERIENCE:")
    if (!line.empty() && line.back() == ':') line.pop_back();
    std::string lower = to_lower(trim(line));

    for (const auto& pat : patterns_) {
        for (const auto& kw : pat.keywords) {
            if (lower == kw) return pat.canonical;
        }
    }

    // Partial match: line starts with keyword + possible decoration
    for (const auto& pat : patterns_) {
        for (const auto& kw : pat.keywords) {
            if (lower.substr(0, kw.size()) == kw) {
                if (lower.size() == kw.size() ||
                    lower[kw.size()] == ' ' ||
                    lower[kw.size()] == '/' ||
                    lower[kw.size()] == '&') {
                    return pat.canonical;
                }
            }
        }
    }

    return "";
}

// ---------------------------------------------------------------------------
// detect
// ---------------------------------------------------------------------------
std::vector<Section> SectionDetector::detect(const std::string& text) const {
    std::vector<std::string> lines;
    {
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            lines.push_back(line);
        }
    }

    // Find heading lines
    struct HeadingHit {
        int         line_idx;
        std::string canonical;
    };
    std::vector<HeadingHit> hits;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        std::string canon = classify_heading(lines[i]);
        if (!canon.empty()) {
            // Avoid duplicate consecutive detections
            if (!hits.empty() && hits.back().canonical == canon &&
                i - hits.back().line_idx <= 2) {
                continue;
            }
            hits.push_back({i, canon});
        }
    }

    // Build sections from hits
    std::vector<Section> sections;
    for (int h = 0; h < static_cast<int>(hits.size()); ++h) {
        int start = hits[h].line_idx + 1;  // content starts after heading
        int end   = (h + 1 < static_cast<int>(hits.size()))
                  ? hits[h + 1].line_idx
                  : static_cast<int>(lines.size());

        std::string section_text;
        for (int l = start; l < end; ++l) {
            section_text += lines[l] + "\n";
        }

        sections.push_back({hits[h].canonical, start, end, trim(section_text)});
    }

    return sections;
}

// ---------------------------------------------------------------------------
// preamble
// ---------------------------------------------------------------------------
std::string SectionDetector::preamble(const std::string& text) const {
    std::vector<std::string> lines;
    {
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) lines.push_back(line);
    }

    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (!classify_heading(lines[i]).empty()) {
            std::string pre;
            for (int j = 0; j < i; ++j) pre += lines[j] + "\n";
            return trim(pre);
        }
    }
    return text;
}

} // namespace talentmatch
