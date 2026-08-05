#pragma once
#include "../include/features.hpp"
#include "../include/resume.hpp"
#include "../ranking/xgboost_ranker.hpp"
#include <string>
#include <vector>

namespace talentmatch {

// ---------------------------------------------------------------------------
// ExplanationEngine
//
// Generates deterministic positive/negative explanation factors from the
// feature vector and category scores. No LLM. No random text.
//
// Rules:
//   - For each feature group, check value against thresholds
//   - Good signals → top_positive_factors (max 5)
//   - Gap signals  → top_negative_factors (max 5)
//   - Template strings are filled with actual feature values for specificity
// ---------------------------------------------------------------------------
class ExplanationEngine {
public:
    struct ExplanationResult {
        std::vector<std::string> positive_factors;
        std::vector<std::string> negative_factors;
    };

    ExplanationResult explain(const FeatureVector& fv,
                               const CategoryScores& scores,
                               const SkillGapResult& gap) const;

private:
    struct Rule {
        std::string feature_name;
        double      positive_threshold; // above this → positive factor
        double      negative_threshold; // below this → negative factor
        std::string positive_template;  // {value} is replaced with actual value
        std::string negative_template;
    };

    static std::vector<Rule> build_rules();

    static std::string fmt_pct(double v);        // 0.87 → "87%"
    static std::string fmt_months(double v);     // 0.5 * 240 → "10 years"
};

} // namespace talentmatch
