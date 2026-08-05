#include "resume_parser.hpp"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <set>

namespace talentmatch {

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------
static std::string tl_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

static std::string tl_trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static bool line_is_empty(const std::string& l) {
    return tl_trim(l).empty();
}

// Split by common skill delimiters: comma, pipe, semicolon, bullet chars
static std::vector<std::string> split_skills(const std::string& text) {
    static const std::regex delim(R"([,|;•·●▪◦])");
    std::sregex_token_iterator it(text.begin(), text.end(), delim, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> parts;
    for (; it != end; ++it) {
        std::string p = tl_trim(it->str());
        // Remove leading "-" or bullet chars
        while (!p.empty() && (p[0] == '-' || p[0] == '\xe2'))
            p = tl_trim(p.substr(1));
        if (!p.empty() && p.size() > 1) parts.push_back(p);
    }
    return parts;
}

// ---------------------------------------------------------------------------
// ResumeParser constructor
// ---------------------------------------------------------------------------
ResumeParser::ResumeParser() {}

// ---------------------------------------------------------------------------
// is_date_line / is_current_date
// ---------------------------------------------------------------------------
bool ResumeParser::is_date_line(const std::string& line) const {
    static const std::regex date_re(
        R"(\b(jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec|january|february|"
        "march|april|june|july|august|september|october|november|december|\d{4})\b)",
        std::regex_constants::icase
    );
    return std::regex_search(line, date_re);
}

bool ResumeParser::is_current_date(const std::string& s) const {
    std::string l = tl_lower(tl_trim(s));
    return l == "present" || l == "current" || l == "now" || l == "ongoing";
}

// ---------------------------------------------------------------------------
// extract_contact
// ---------------------------------------------------------------------------
ContactInfo ResumeParser::extract_contact(const std::string& preamble,
                                           const std::string& full_text) const
{
    ContactInfo c;

    // Use preamble + first 500 chars of full text as search space
    std::string search = preamble + "\n" + full_text.substr(0, std::min<size_t>(500, full_text.size()));

    // Email
    {
        static const std::regex email_re(R"([\w.+\-]+@[\w\-]+\.[\w.]{2,})");
        std::smatch m;
        if (std::regex_search(search, m, email_re)) c.email = m[0].str();
    }
    // Phone
    {
        static const std::regex phone_re(R"(\+?[\d\s\-().]{7,18}\d)");
        std::smatch m;
        if (std::regex_search(search, m, phone_re)) {
            std::string p = m[0].str();
            // Filter: must have ≥7 digits
            int digits = 0;
            for (char ch : p) if (std::isdigit(static_cast<unsigned char>(ch))) ++digits;
            if (digits >= 7) c.phone = tl_trim(p);
        }
    }
    // LinkedIn
    {
        static const std::regex li_re(R"(linkedin\.com/in/[\w\-]+)", std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(search, m, li_re)) c.linkedin_url = m[0].str();
    }
    // GitHub
    {
        static const std::regex gh_re(R"(github\.com/[\w\-]+)", std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(search, m, gh_re)) c.github_url = m[0].str();
    }

    // Full name: first non-empty line of preamble that isn't a URL/email/phone
    {
        auto lines = split_lines(preamble);
        for (const auto& ln : lines) {
            std::string t = tl_trim(ln);
            if (t.empty()) continue;
            // Skip if it contains email-like or URL-like content
            if (t.find('@') != std::string::npos) continue;
            if (t.find("http") != std::string::npos) continue;
            if (t.find("linkedin") != std::string::npos) continue;
            if (t.find("github") != std::string::npos) continue;
            // Name is usually 2–4 words, mostly alpha
            auto words = split_skills(t);  // reuse split
            std::istringstream ss2(t);
            std::vector<std::string> name_words;
            std::string w;
            while (ss2 >> w) name_words.push_back(w);
            if (name_words.size() >= 2 && name_words.size() <= 5) {
                bool looks_like_name = true;
                for (const auto& nw : name_words) {
                    int alph = 0;
                    for (char ch : nw) if (std::isalpha(static_cast<unsigned char>(ch))) ++alph;
                    if (alph < static_cast<int>(nw.size()) * 0.7) { looks_like_name = false; break; }
                }
                if (looks_like_name) { c.full_name = t; break; }
            }
        }
    }

    return c;
}

// ---------------------------------------------------------------------------
// extract_summary
// ---------------------------------------------------------------------------
std::string ResumeParser::extract_summary(const std::string& section_text) const {
    return tl_trim(section_text);
}

// ---------------------------------------------------------------------------
// extract_experience
// ---------------------------------------------------------------------------
std::vector<ExperienceEntry> ResumeParser::extract_experience(const std::string& section_text) const {
    // Pattern: date ranges like "Jan 2020 – Present", "2018-2020", "06/2019 - 12/2021"
    static const std::regex date_range_re(
        R"((\b(?:jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec|january|"
        "february|march|april|june|july|august|september|october|november|"
        "december)?\s*\d{4}\b)\s*[-–—to]+\s*(\b(?:present|current|now|ongoing|"
        "jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec|\d{4})\b[\w\s]*\b))",
        std::regex_constants::icase
    );

    auto lines = split_lines(section_text);
    std::vector<ExperienceEntry> entries;
    ExperienceEntry current;
    bool in_entry = false;

    auto flush = [&]() {
        if (in_entry && (!current.company.empty() || !current.job_title.empty())) {
            entries.push_back(current);
        }
        current = ExperienceEntry{};
        in_entry = false;
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = tl_trim(lines[i]);
        if (line.empty()) continue;

        // Check if this line contains a date range → it's a job header area
        std::smatch dm;
        bool has_date = std::regex_search(line, dm, date_range_re);

        if (has_date) {
            flush();
            in_entry = true;
            current.start_date = tl_trim(dm[1].str());
            current.end_date   = tl_trim(dm[2].str());
            current.is_current = is_current_date(current.end_date);

            // Title and company: look at this line and the line before
            std::string header_line = line;
            // Remove the date portion from the header line
            header_line = std::regex_replace(header_line, date_range_re, "");
            header_line = tl_trim(header_line);

            // Common pattern: "Job Title at Company" or "Job Title, Company"
            {
                static const std::regex at_re(R"(\bat\b)", std::regex_constants::icase);
                std::smatch am;
                if (std::regex_search(header_line, am, at_re)) {
                    current.job_title = tl_trim(header_line.substr(0, am.position()));
                    current.company   = tl_trim(header_line.substr(am.position() + am.length()));
                } else {
                    // Try comma split
                    auto comma = header_line.find(',');
                    if (comma != std::string::npos) {
                        current.job_title = tl_trim(header_line.substr(0, comma));
                        current.company   = tl_trim(header_line.substr(comma + 1));
                    } else {
                        // Use prev line as title if current line is mostly a date
                        if (i > 0) {
                            std::string prev = tl_trim(lines[i - 1]);
                            if (!prev.empty() && !is_date_line(prev)) {
                                current.job_title = prev;
                            }
                        }
                        if (!header_line.empty()) {
                            if (current.job_title.empty()) current.job_title = header_line;
                            else current.company = header_line;
                        }
                    }
                }
            }
        } else if (in_entry) {
            // Bullet/responsibility line
            std::string bullet = line;
            // Strip leading bullet markers
            static const std::regex bullet_re(R"(^[\-•\*·▪◦\xe2]\s*)");
            bullet = std::regex_replace(bullet, bullet_re, "");
            bullet = tl_trim(bullet);
            if (!bullet.empty() && bullet.size() > 5) {
                current.responsibilities.push_back(bullet);
            }
        }
    }
    flush();

    // Last resort: if no entries found, try to detect blocks by looking for
    // company-like lines (capitalized short lines)
    if (entries.empty()) {
        ExperienceEntry fallback;
        fallback.job_title = "Unknown";
        for (const auto& ln : lines) {
            std::string t = tl_trim(ln);
            if (!t.empty() && t.size() > 5) {
                static const std::regex bullet_r(R"(^[\-•\*·]\s*)");
                std::string b = std::regex_replace(t, bullet_r, "");
                if (!b.empty()) fallback.responsibilities.push_back(b);
            }
        }
        if (!fallback.responsibilities.empty()) entries.push_back(fallback);
    }

    return entries;
}

// ---------------------------------------------------------------------------
// extract_education
// ---------------------------------------------------------------------------
std::vector<EducationEntry> ResumeParser::extract_education(const std::string& section_text) const {
    static const std::regex degree_re(
        R"(\b(ph\.?d|doctorate|doctoral|master|m\.tech|mtech|msc|m\.s\.|mba|"
        "b\.tech|btech|bsc|b\.s\.|bachelor|be\b|b\.e\.|associate|diploma|"
        "high school|secondary)\b)",
        std::regex_constants::icase
    );
    static const std::regex gpa_re(R"(gpa\s*:?\s*([\d.]+)\s*/?\s*([\d.]+)?)",
        std::regex_constants::icase);
    static const std::regex year_re(R"(\b(19|20)\d{2}\b)");

    auto lines = split_lines(section_text);
    std::vector<EducationEntry> entries;
    EducationEntry current;
    bool in_entry = false;

    auto flush = [&]() {
        if (in_entry && !current.institution.empty()) {
            entries.push_back(current);
        }
        current = EducationEntry{};
        in_entry = false;
    };

    for (const auto& raw : lines) {
        std::string line = tl_trim(raw);
        if (line.empty()) { continue; }

        std::smatch dm;
        bool has_degree = std::regex_search(line, dm, degree_re);
        bool has_year   = std::regex_search(line, year_re);

        if (has_degree || (has_year && !in_entry)) {
            flush();
            in_entry = true;

            if (has_degree) {
                current.degree = tl_trim(dm[0].str());
                // Field of study: text after the degree keyword
                size_t pos = dm.position() + dm.length();
                std::string rest = tl_trim(line.substr(pos));
                // Remove leading "in", "of", ","
                static const std::regex lead_re(R"(^(in|of|,)\s*)");
                rest = std::regex_replace(rest, lead_re, "");
                // Remove year from rest
                rest = std::regex_replace(rest, year_re, "");
                rest = tl_trim(rest);
                if (rest.size() > 2 && rest.size() < 80) current.field_of_study = rest;
            }

            // Year range
            {
                auto it = std::sregex_iterator(line.begin(), line.end(), year_re);
                auto end = std::sregex_iterator();
                std::vector<std::string> years;
                for (; it != end; ++it) years.push_back((*it)[0].str());
                if (years.size() >= 2) {
                    current.start_date = years[0];
                    current.end_date   = years[1];
                } else if (years.size() == 1) {
                    current.end_date = years[0];
                }
            }

            // Institution: the line itself if it doesn't look like a degree line,
            // or the previous line
            if (current.institution.empty()) {
                // Try to extract from this line (after degree keyword portion)
                // Or use this line if it's institution-like
                // Institution heuristic: long proper-noun-ish line
                std::string candidate = line;
                // Remove year and degree from candidate
                candidate = std::regex_replace(candidate, degree_re, "");
                candidate = std::regex_replace(candidate, year_re, "");
                candidate = tl_trim(candidate);
                if (candidate.size() > 5) current.institution = candidate;
            }
        } else if (in_entry) {
            // Could be institution name on its own line
            if (current.institution.empty() && line.size() > 5 && line.size() < 100) {
                if (!is_date_line(line)) current.institution = line;
            }
            // GPA line
            std::smatch gm;
            if (std::regex_search(line, gm, gpa_re)) {
                std::string num = gm[1].str();
                std::string den = gm[2].matched ? gm[2].str() : "";
                current.gpa = num + (den.empty() ? "" : "/" + den);
            }
            // Check "Present" for is_currently_studying
            if (tl_lower(line).find("present") != std::string::npos ||
                tl_lower(line).find("current") != std::string::npos) {
                current.end_date = "Present";
            }
        }
    }
    flush();

    return entries;
}

// ---------------------------------------------------------------------------
// extract_skills
// ---------------------------------------------------------------------------
std::vector<std::string> ResumeParser::extract_skills(const std::string& section_text) const {
    std::set<std::string> seen;
    std::vector<std::string> skills;

    auto lines = split_lines(section_text);
    for (const auto& ln : lines) {
        // Try comma/pipe/semicolon splitting first
        std::string line = tl_trim(ln);
        // Remove leading category prefix like "Languages: " or "Tools:"
        {
            static const std::regex prefix_re(R"(^[\w\s&/]+:\s*)");
            line = std::regex_replace(line, prefix_re, "");
        }
        if (line.empty()) continue;

        auto parts = split_skills(line);
        if (parts.size() >= 2) {
            for (const auto& p : parts) {
                std::string t = tl_trim(p);
                if (!t.empty() && t.size() >= 2 && t.size() <= 50) {
                    if (seen.insert(tl_lower(t)).second) skills.push_back(t);
                }
            }
        } else {
            // Single skill on a line (bullet style)
            static const std::regex bullet_re(R"(^[\-•\*·▪◦]\s*)");
            std::string cleaned = std::regex_replace(line, bullet_re, "");
            cleaned = tl_trim(cleaned);
            if (!cleaned.empty() && cleaned.size() >= 2 && cleaned.size() <= 50) {
                if (seen.insert(tl_lower(cleaned)).second) skills.push_back(cleaned);
            }
        }
    }

    return skills;
}

// ---------------------------------------------------------------------------
// extract_projects
// ---------------------------------------------------------------------------
std::vector<ProjectEntry> ResumeParser::extract_projects(const std::string& section_text) const {
    static const std::regex url_re(R"((https?://|github\.com/|gitlab\.com/)[\w\-./]+)",
        std::regex_constants::icase);
    static const std::regex tech_re(R"(\b(using|built with|technologies?|tech stack|tools?)\s*:?\s*(.+))",
        std::regex_constants::icase);
    static const std::regex bullet_re(R"(^[\-•\*·▪◦]\s*)");

    auto lines = split_lines(section_text);
    std::vector<ProjectEntry> entries;
    ProjectEntry current;
    bool in_project = false;

    auto flush = [&]() {
        if (in_project && !current.name.empty()) {
            entries.push_back(current);
        }
        current = ProjectEntry{};
        in_project = false;
    };

    for (const auto& raw : lines) {
        std::string line = tl_trim(raw);
        if (line.empty()) { flush(); continue; }

        // Lines starting with a non-bullet short string are likely project names
        bool is_bullet = std::regex_search(line, bullet_re);
        std::string cleaned = std::regex_replace(line, bullet_re, "");
        cleaned = tl_trim(cleaned);

        if (!is_bullet && cleaned.size() <= 80 && cleaned.size() >= 3 && !is_date_line(cleaned)) {
            // Check if it's a project name (no period at end, not all lowercase)
            bool has_upper = false;
            for (char c : cleaned) if (std::isupper(static_cast<unsigned char>(c))) { has_upper = true; break; }
            if (has_upper) {
                flush();
                in_project = true;
                current.name = cleaned;
                // Extract URL if present
                std::smatch um;
                if (std::regex_search(cleaned, um, url_re)) {
                    current.url = um[0].str();
                    current.name = tl_trim(cleaned.substr(0, um.position()));
                }
                continue;
            }
        }

        if (in_project) {
            // URL
            std::smatch um;
            if (std::regex_search(cleaned, um, url_re)) {
                current.url = um[0].str();
            }

            // Technologies
            std::smatch tm;
            if (std::regex_search(cleaned, tm, tech_re)) {
                std::string tech_str = tl_trim(tm[2].str());
                auto techs = split_skills(tech_str);
                for (const auto& t : techs) current.technologies_used.push_back(t);
            } else {
                current.description += cleaned + " ";
            }
        }
    }
    flush();

    return entries;
}

// ---------------------------------------------------------------------------
// extract_certifications
// ---------------------------------------------------------------------------
std::vector<CertificationEntry> ResumeParser::extract_certifications(
    const std::string& section_text) const
{
    static const std::regex year_re(R"(\b(20\d{2})\b)");
    static const std::regex bullet_re(R"(^[\-•\*·▪◦]\s*)");

    auto lines = split_lines(section_text);
    std::vector<CertificationEntry> entries;

    for (const auto& raw : lines) {
        std::string line = tl_trim(raw);
        if (line.empty()) continue;
        line = std::regex_replace(line, bullet_re, "");
        line = tl_trim(line);
        if (line.size() < 3) continue;

        CertificationEntry e;

        // Year
        std::smatch ym;
        if (std::regex_search(line, ym, year_re)) {
            e.issue_date = ym[0].str();
            // Remove year from name search
        }

        // Heuristic: "CertName, IssuerOrg" or "CertName — IssuerOrg"
        static const std::regex sep_re(R"([,\-–—]\s*)");
        std::sregex_token_iterator it(line.begin(), line.end(), sep_re, -1);
        std::sregex_token_iterator end;
        std::vector<std::string> parts;
        for (; it != end; ++it) {
            std::string p = tl_trim(it->str());
            // Remove year from part
            p = std::regex_replace(p, year_re, "");
            p = tl_trim(p);
            if (!p.empty()) parts.push_back(p);
        }

        if (!parts.empty()) {
            e.name = parts[0];
            if (parts.size() >= 2) e.issuing_organization = parts[1];
        } else {
            e.name = line;
        }

        if (!e.name.empty()) entries.push_back(e);
    }

    return entries;
}

// ---------------------------------------------------------------------------
// extract_list  (for languages, awards, publications)
// ---------------------------------------------------------------------------
std::vector<std::string> ResumeParser::extract_list(const std::string& section_text) const {
    std::vector<std::string> result;
    auto parts = split_skills(section_text);
    for (const auto& p : parts) {
        std::string t = tl_trim(p);
        if (!t.empty() && t.size() >= 2) result.push_back(t);
    }
    if (result.empty()) {
        for (const auto& ln : split_lines(section_text)) {
            std::string t = tl_trim(ln);
            // strip bullets
            static const std::regex bullet_re(R"(^[\-•\*·▪◦]\s*)");
            t = std::regex_replace(t, bullet_re, "");
            t = tl_trim(t);
            if (!t.empty()) result.push_back(t);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// parse   —  top-level entry point
// ---------------------------------------------------------------------------
Resume ResumeParser::parse(const std::string& text) const {
    Resume r;
    r.raw_text = text;

    // 1. Detect sections
    auto sections = section_detector_.detect(text);
    std::string preamble = section_detector_.preamble(text);

    // 2. Contact (from preamble + full text)
    r.contact = extract_contact(preamble, text);

    // 3. Process each section
    for (const auto& sec : sections) {
        if      (sec.name == "SUMMARY")         r.summary = extract_summary(sec.text);
        else if (sec.name == "EXPERIENCE")       r.experience = extract_experience(sec.text);
        else if (sec.name == "EDUCATION")        r.education = extract_education(sec.text);
        else if (sec.name == "SKILLS")           r.raw_skills = extract_skills(sec.text);
        else if (sec.name == "PROJECTS")         r.projects = extract_projects(sec.text);
        else if (sec.name == "CERTIFICATIONS")   r.certifications = extract_certifications(sec.text);
        else if (sec.name == "LANGUAGES")        r.languages = extract_list(sec.text);
        else if (sec.name == "AWARDS")           r.awards = extract_list(sec.text);
    }

    // 4. Fallback: extract skills from full text if skills section is empty
    if (r.raw_skills.empty()) {
        // Scan whole document for skill-like comma-separated tokens
        static const std::regex skills_fallback(
            R"((?:skills?|technologies|proficiencies)[:\s]+(.+?)(?:\n\n|\z))",
            std::regex_constants::icase | std::regex_constants::ECMAScript
        );
        std::smatch sm;
        if (std::regex_search(text, sm, skills_fallback)) {
            r.raw_skills = extract_skills(sm[1].str());
        }
    }

    return r;
}

} // namespace talentmatch
