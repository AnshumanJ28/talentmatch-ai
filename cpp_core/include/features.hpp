#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

namespace talentmatch {

// ---------------------------------------------------------------------------
// Canonical feature name ordering.
// This order MUST match the training schema used by the XGBoost model.
// Adding a feature here requires retraining.
// ---------------------------------------------------------------------------
static const std::vector<std::string> FEATURE_NAMES = {
    // --- Experience (15) ---
    "total_experience_months",
    "relevant_experience_months",
    "num_positions",
    "avg_tenure_months",
    "max_tenure_months",
    "employment_gap_months",
    "num_gaps",
    "is_currently_employed",
    "max_seniority_score",
    "avg_seniority_score",
    "has_leadership_keywords",
    "num_promotions",
    "quantified_achievement_count",
    "action_verb_density",
    "career_progression_score",

    // --- Education (10) ---
    "num_degrees",
    "highest_degree_level",
    "degree_field_match",
    "gpa_normalized",
    "has_gpa",
    "is_currently_studying",
    "num_institutions",
    "education_recency_score",
    "prestigious_institution_flag",
    "degree_requirement_met",

    // --- Projects (8) ---
    "num_projects",
    "avg_technologies_per_project",
    "project_link_ratio",
    "project_tech_overlap",
    "project_domain_relevance",
    "has_open_source",
    "avg_project_desc_length",
    "project_quantification_score",

    // --- Certifications (7) ---
    "num_certifications",
    "num_unique_issuers",
    "has_certifications",
    "jd_cert_match",
    "cert_recency_score",
    "has_cloud_cert",
    "has_security_cert",

    // --- Resume Quality (10) ---
    "section_completeness_score",
    "action_verb_density_global",
    "quantified_achievements_total",
    "avg_bullet_length",
    "total_word_count",
    "resume_length_score",
    "formatting_consistency",
    "has_summary",
    "has_links",
    "has_contact_info",

    // --- Skills (20) ---
    "skill_coverage_ratio",
    "critical_skill_coverage",
    "optional_skill_coverage",
    "num_matched_skills",
    "num_missing_skills",
    "num_partial_skills",
    "rare_skill_bonus",
    "skill_category_diversity",
    "avg_skill_confidence",
    "high_confidence_skill_ratio",
    "total_canonical_skills",
    "unmatched_skill_ratio",
    "primary_domain_confidence",
    "skills_text_length",
    "jd_skill_count",
    "jd_critical_skill_count",
    "skill_exact_match_count",
    "skill_fuzzy_match_count",
    "skill_missing_critical_count",
    "skill_has_all_required",

    // --- Retrieval (15) ---
    "bm25_score",
    "tfidf_cosine_similarity",
    "keyword_overlap_count",
    "keyword_overlap_ratio",
    "cosine_similarity",
    "jd_term_coverage",
    "resume_jd_length_ratio",
    "bigram_overlap_ratio",
    "title_match_score",
    "summary_jd_similarity",
    "jd_unique_term_count",
    "resume_unique_term_count",
    "jd_tech_term_count",
    "resume_tech_term_count",
    "tf_weighted_keyword_score",
};

// ---------------------------------------------------------------------------
// Feature vector: key = feature name, value = normalized [0, 1] scalar.
// ---------------------------------------------------------------------------
struct FeatureVector {
    std::unordered_map<std::string, double> features;

    // Get feature value, default to 0.0 if missing
    double get(const std::string& name) const {
        auto it = features.find(name);
        return (it != features.end()) ? it->second : 0.0;
    }

    // Set feature value (clamps to [0, 1] range)
    void set(const std::string& name, double value) {
        features[name] = std::max(0.0, std::min(1.0, value));
    }

    // Set raw value without clamping
    void set_raw(const std::string& name, double value) {
        features[name] = value;
    }

    // Convert to ordered flat float array matching FEATURE_NAMES schema
    std::vector<float> to_flat_array() const {
        std::vector<float> arr;
        arr.reserve(FEATURE_NAMES.size());
        for (const auto& name : FEATURE_NAMES) {
            arr.push_back(static_cast<float>(get(name)));
        }
        return arr;
    }

    // Ensure all features in FEATURE_NAMES have a value (fill missing with 0)
    void fill_missing() {
        for (const auto& name : FEATURE_NAMES) {
            if (features.find(name) == features.end()) {
                features[name] = 0.0;
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Sub-scores per category (0–100 scale for the output)
// ---------------------------------------------------------------------------
struct CategoryScores {
    double skills{0.0};
    double experience{0.0};
    double education{0.0};
    double projects{0.0};
    double semantic{0.0};
    double resume_quality{0.0};
};

// ---------------------------------------------------------------------------
// Feature importance entry (used by XGBoost ranker + explainability)
// ---------------------------------------------------------------------------
struct FeatureImportance {
    std::string name;
    double      importance{0.0};
    double      feature_value{0.0};
};

} // namespace talentmatch
