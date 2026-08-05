#pragma once
#include <string>
#include <vector>
#include <optional>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace talentmatch {

// ---------------------------------------------------------------------------
// Contact information
// ---------------------------------------------------------------------------
struct ContactInfo {
    std::string full_name;
    std::string email;
    std::string phone;
    std::string location;
    std::string linkedin_url;
    std::string github_url;
    std::string portfolio_url;
};

// ---------------------------------------------------------------------------
// Experience entry
// ---------------------------------------------------------------------------
struct ExperienceEntry {
    std::string company;
    std::string job_title;
    std::string location;
    std::string start_date;
    std::string end_date;
    bool        is_current{false};
    std::vector<std::string> responsibilities;
};

// ---------------------------------------------------------------------------
// Education entry
// ---------------------------------------------------------------------------
struct EducationEntry {
    std::string institution;
    std::string degree;
    std::string field_of_study;
    std::string start_date;
    std::string end_date;
    std::string gpa;
};

// ---------------------------------------------------------------------------
// Project entry
// ---------------------------------------------------------------------------
struct ProjectEntry {
    std::string name;
    std::string description;
    std::vector<std::string> technologies_used;
    std::string url;
};

// ---------------------------------------------------------------------------
// Certification entry
// ---------------------------------------------------------------------------
struct CertificationEntry {
    std::string name;
    std::string issuing_organization;
    std::string issue_date;
    std::string credential_id;
};

// ---------------------------------------------------------------------------
// Parsed skill match (from skill engine)
// ---------------------------------------------------------------------------
struct SkillMatch {
    std::string canonical_name;
    std::string category;
    float       confidence{1.0f};
    std::string match_method;  // "exact", "trie", "fuzzy"
};

// ---------------------------------------------------------------------------
// Partial skill match (JD skill partially matched by a candidate skill)
// ---------------------------------------------------------------------------
struct PartialSkillMatch {
    std::string jd_skill;
    std::string matched_as;
    float       confidence{0.0f};
};

// ---------------------------------------------------------------------------
// Skill gap analysis result
// ---------------------------------------------------------------------------
struct SkillGapResult {
    std::vector<std::string>    matched_skills;
    std::vector<std::string>    missing_skills;
    std::vector<PartialSkillMatch> partial_skills;
    float                       match_ratio{0.0f};
};

// ---------------------------------------------------------------------------
// Full parsed resume
// ---------------------------------------------------------------------------
struct Resume {
    ContactInfo contact;
    std::string summary;
    std::vector<ExperienceEntry>     experience;
    std::vector<EducationEntry>      education;
    std::vector<std::string>         raw_skills;
    std::vector<ProjectEntry>        projects;
    std::vector<CertificationEntry>  certifications;
    std::vector<std::string>         languages;
    std::vector<std::string>         awards;
    std::string                      raw_text;  // full extracted text
};

// ---------------------------------------------------------------------------
// Job Description structured form
// ---------------------------------------------------------------------------
struct JobDescription {
    std::string raw_text;
    std::vector<std::string> required_skills;
    std::vector<std::string> preferred_skills;
    std::string seniority_level;   // "junior", "mid", "senior", "lead", etc.
    std::string domain;
};

} // namespace talentmatch
