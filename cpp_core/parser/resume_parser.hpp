#pragma once
#include "../include/resume.hpp"
#include "section_detector.hpp"
#include <string>

namespace talentmatch {

// ---------------------------------------------------------------------------
// ResumeParser
//
// Converts plain text (extracted from a PDF by Python/PyMuPDF) into a
// structured Resume object using only deterministic regex and heuristics.
// No LLM, no network, no external API.
// ---------------------------------------------------------------------------
class ResumeParser {
public:
    ResumeParser();

    // Parse resume plain text → structured Resume
    Resume parse(const std::string& text) const;

private:
    SectionDetector section_detector_;

    // Contact extraction
    ContactInfo     extract_contact(const std::string& preamble,
                                    const std::string& full_text) const;

    // Section-specific extractors
    std::string     extract_summary(const std::string& section_text) const;
    std::vector<ExperienceEntry> extract_experience(const std::string& section_text) const;
    std::vector<EducationEntry>  extract_education(const std::string& section_text) const;
    std::vector<std::string>     extract_skills(const std::string& section_text) const;
    std::vector<ProjectEntry>    extract_projects(const std::string& section_text) const;
    std::vector<CertificationEntry> extract_certifications(const std::string& section_text) const;
    std::vector<std::string>     extract_list(const std::string& section_text) const;

    // Date helpers
    bool is_date_line(const std::string& line) const;
    bool is_current_date(const std::string& date_str) const;
};

} // namespace talentmatch
