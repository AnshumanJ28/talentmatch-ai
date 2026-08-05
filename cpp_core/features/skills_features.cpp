#include "skills_features.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace talentmatch {

void SkillsFeatureExtractor::extract(
    const SkillExtractionResult& candidate,
    const SkillExtractionResult& jd_result,
    const SkillGapResult& gap,
    FeatureVector& fv)
{
    const auto& cand_skills = candidate.matched_skills;
    const auto& jd_skills   = jd_result.matched_skills;

    int n_cand = static_cast<int>(cand_skills.size());
    int n_jd   = static_cast<int>(jd_skills.size());
    int n_match = static_cast<int>(gap.matched_skills.size());
    int n_miss  = static_cast<int>(gap.missing_skills.size());
    int n_part  = static_cast<int>(gap.partial_skills.size());

    // Coverage ratios
    double coverage = (n_jd > 0) ? static_cast<double>(n_match) / n_jd : 1.0;
    // Partial coverage (matched + partial / total)
    double partial_cov = (n_jd > 0)
        ? static_cast<double>(n_match + n_part) / n_jd : 1.0;

    // Average confidence
    double avg_conf = 0.0;
    int high_conf_count = 0;
    int exact_count = 0, fuzzy_count = 0;
    if (!cand_skills.empty()) {
        for (const auto& s : cand_skills) {
            avg_conf += s.confidence;
            if (s.confidence >= 0.85) ++high_conf_count;
            if (s.match_method == "exact" || s.match_method == "trie") ++exact_count;
            else if (s.match_method == "fuzzy") ++fuzzy_count;
        }
        avg_conf /= n_cand;
    }
    double high_conf_ratio = (n_cand > 0)
        ? static_cast<double>(high_conf_count) / n_cand : 0.0;
    double unmatched_ratio = (n_cand > 0)
        ? 1.0 - static_cast<double>(exact_count + fuzzy_count) / n_cand : 0.0;

    // Category diversity
    std::unordered_set<std::string> cats;
    for (const auto& s : cand_skills) cats.insert(s.category);
    double cat_diversity = std::min(1.0, static_cast<double>(cats.size()) / 10.0);

    // Missing critical skills (approximation: skills in top half of JD)
    int critical_threshold = std::max(1, n_jd / 2);
    int missing_critical = 0;
    for (const auto& ms : gap.missing_skills) {
        // Check if this was in the "required" part of JD (first half)
        // Simplified: all missing skills are penalized
        ++missing_critical;
    }
    missing_critical = std::min(missing_critical, critical_threshold);

    // Has all required skills
    bool has_all = gap.missing_skills.empty();

    // Rare skill bonus: skills present in candidate but not in JD (unique value)
    // Approximate: if candidate has more skills than JD, bonus
    double rare_bonus = (n_cand > n_jd) ?
        std::min(0.3, static_cast<double>(n_cand - n_jd) / 20.0) : 0.0;

    fv.set("skill_coverage_ratio",    coverage);
    fv.set("critical_skill_coverage", std::min(1.0, partial_cov));
    fv.set("optional_skill_coverage", partial_cov);
    fv.set("num_matched_skills",      std::min(1.0, static_cast<double>(n_match) / 20.0));
    fv.set("num_missing_skills",      1.0 - std::min(1.0, static_cast<double>(n_miss) / 15.0));
    fv.set("num_partial_skills",      std::min(1.0, static_cast<double>(n_part) / 10.0));
    fv.set("rare_skill_bonus",        rare_bonus);
    fv.set("skill_category_diversity",cat_diversity);
    fv.set("avg_skill_confidence",    avg_conf);
    fv.set("high_confidence_skill_ratio", high_conf_ratio);
    fv.set("total_canonical_skills",  std::min(1.0, static_cast<double>(n_cand) / 30.0));
    fv.set("unmatched_skill_ratio",   1.0 - unmatched_ratio); // higher = better match quality
    fv.set("primary_domain_confidence", candidate.primary_domain_confidence);
    fv.set("skills_text_length",      std::min(1.0, static_cast<double>(n_cand) / 25.0));
    fv.set("jd_skill_count",          std::min(1.0, static_cast<double>(n_jd) / 20.0));
    fv.set("jd_critical_skill_count", std::min(1.0, static_cast<double>(critical_threshold) / 10.0));
    fv.set("skill_exact_match_count", std::min(1.0, static_cast<double>(exact_count) / 20.0));
    fv.set("skill_fuzzy_match_count", std::min(1.0, static_cast<double>(fuzzy_count) / 10.0));
    fv.set("skill_missing_critical_count",
           1.0 - std::min(1.0, static_cast<double>(missing_critical) / critical_threshold));
    fv.set("skill_has_all_required",  has_all ? 1.0 : coverage);
}

} // namespace talentmatch
