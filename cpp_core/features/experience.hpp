#pragma once
#include "../include/features.hpp"
#include "../include/resume.hpp"
#include <string>

namespace talentmatch {

// ---------------------------------------------------------------------------
// ExperienceFeatureExtractor
// Produces 15 experience-related features
// ---------------------------------------------------------------------------
class ExperienceFeatureExtractor {
public:
    static void extract(const Resume& resume, const std::string& jd_text,
                        FeatureVector& fv);

private:
    static double compute_seniority_score(const std::string& job_title);
    static double compute_experience_months(const std::string& start, const std::string& end);
    static bool   has_leadership(const std::string& text);
    static int    count_quantified(const std::vector<std::string>& bullets);
    static double action_verb_density(const std::vector<std::string>& bullets);
    static double jd_seniority_level(const std::string& jd_text);
    static double career_progression(const std::vector<ExperienceEntry>& exp);
};

} // namespace talentmatch
