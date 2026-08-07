#include "xgboost_ranker.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>

// Optional XGBoost C API support
// Compile with -DHAVE_XGBOOST and link -lxgboost to enable ML ranking.
// Without it, the linear fallback is used automatically.
#ifdef HAVE_XGBOOST
#include <xgboost/c_api.h>
#endif

namespace talentmatch {

// ---------------------------------------------------------------------------
// CategoryScoreComputer
// Weighted average of relevant features per category → 0-100 score
// ---------------------------------------------------------------------------
CategoryScores CategoryScoreComputer::compute(const FeatureVector& fv) {
    CategoryScores s;

    // SKILLS (0-100): weighted blend of coverage, confidence, diversity
    s.skills = 100.0 * (
        0.40 * fv.get("skill_coverage_ratio") +
        0.20 * fv.get("critical_skill_coverage") +
        0.15 * fv.get("avg_skill_confidence") +
        0.10 * fv.get("skill_category_diversity") +
        0.10 * fv.get("skill_has_all_required") +
        0.05 * fv.get("rare_skill_bonus")
    );

    // EXPERIENCE (0-100)
    s.experience = 100.0 * (
        0.30 * fv.get("relevant_experience_months") +
        0.15 * fv.get("max_seniority_score") +
        0.15 * fv.get("career_progression_score") +
        0.10 * fv.get("is_currently_employed") +
        0.10 * fv.get("action_verb_density") +
        0.10 * fv.get("quantified_achievement_count") +
        0.05 * fv.get("has_leadership_keywords") +
        0.05 * fv.get("employment_gap_months")
    );

    // Penalties
    if (fv.get("job_hopper_flag") > 0.5) s.experience -= 10.0;
    if (fv.get("overqualified_flag") > 0.5) s.experience -= 10.0;

    // EDUCATION (0-100)
    s.education = 100.0 * (
        0.35 * fv.get("highest_degree_level") +
        0.30 * fv.get("degree_field_match") +
        0.15 * fv.get("degree_requirement_met") +
        0.10 * fv.get("gpa_normalized") +
        0.05 * fv.get("prestigious_institution_flag") +
        0.05 * fv.get("education_recency_score")
    );

    // PROJECTS (0-100)
    s.projects = 100.0 * (
        0.35 * fv.get("project_tech_overlap") +
        0.25 * fv.get("project_domain_relevance") +
        0.15 * fv.get("num_projects") +
        0.10 * fv.get("project_link_ratio") +
        0.10 * fv.get("avg_technologies_per_project") +
        0.05 * fv.get("has_open_source")
    );

    // SEMANTIC (0-100): retrieval signals
    s.semantic = 100.0 * (
        0.40 * fv.get("cosine_similarity") +
        0.25 * fv.get("tfidf_cosine_similarity") +
        0.20 * fv.get("bm25_score") +
        0.15 * fv.get("keyword_overlap_ratio")
    );

    // RESUME QUALITY (0-100)
    s.resume_quality = 100.0 * (
        0.20 * fv.get("section_completeness_score") +
        0.20 * fv.get("action_verb_density_global") +
        0.15 * fv.get("quantified_achievements_total") +
        0.15 * fv.get("resume_length_score") +
        0.10 * fv.get("readability_score") + 
        0.10 * fv.get("has_summary") +
        0.05 * fv.get("has_links") +
        0.05 * fv.get("has_contact_info")
    );

    // Severe penalty for keyword stuffing
    if (fv.get("keyword_stuffing_penalty") > 0.5) s.resume_quality -= 30.0;

    // Clamp all to [0, 100]
    auto clamp100 = [](double v){ return std::max(0.0, std::min(100.0, v)); };
    s.skills         = clamp100(s.skills);
    s.experience     = clamp100(s.experience);
    s.education      = clamp100(s.education);
    s.projects       = clamp100(s.projects);
    s.semantic       = clamp100(s.semantic);
    s.resume_quality = clamp100(s.resume_quality);

    return s;
}

// ---------------------------------------------------------------------------
// XGBoostRanker
// ---------------------------------------------------------------------------
XGBoostRanker::XGBoostRanker(const std::string& model_path)
    : model_path_(model_path)
{
    model_loaded_ = try_load_xgboost();
}

XGBoostRanker::~XGBoostRanker() {
#ifdef HAVE_XGBOOST
    if (booster_) {
        XGBoosterFree(static_cast<BoosterHandle>(booster_));
        booster_ = nullptr;
    }
#endif
}

bool XGBoostRanker::try_load_xgboost() {
#ifdef HAVE_XGBOOST
    // Check model file exists
    std::ifstream f(model_path_);
    if (!f.good()) return false;

    BoosterHandle handle;
    if (XGBoosterCreate(nullptr, 0, &handle) != 0) return false;
    if (XGBoosterLoadModel(handle, model_path_.c_str()) != 0) {
        XGBoosterFree(handle);
        return false;
    }
    booster_ = handle;
    return true;
#else
    return false;
#endif
}

double XGBoostRanker::xgb_predict(const FeatureVector& fv) const {
#ifdef HAVE_XGBOOST
    if (!booster_) return linear_fallback(fv);

    auto flat = fv.to_flat_array();
    DMatrixHandle dmat;
    XGDMatrixCreateFromMat(flat.data(), 1, static_cast<bst_ulong>(flat.size()),
                            std::numeric_limits<float>::quiet_NaN(), &dmat);

    bst_ulong out_len = 0;
    const float* out_result = nullptr;
    XGBoosterPredict(static_cast<BoosterHandle>(booster_), dmat, 0, 0, 0, &out_len, &out_result);
    double prob = (out_len > 0) ? static_cast<double>(out_result[0]) : 0.5;

    XGDMatrixFree(dmat);
    return std::max(0.0, std::min(1.0, prob));
#else
    return linear_fallback(fv);
#endif
}

// ---------------------------------------------------------------------------
// linear_fallback
//
// Deterministic weighted combination of category scores.
// Weights are domain-expert tuned; replace with XGBoost for ML-driven ranking.
// ---------------------------------------------------------------------------
double XGBoostRanker::linear_fallback(const FeatureVector& fv) {
    CategoryScores cat = CategoryScoreComputer::compute(fv);

    // Category weights (must sum to 1.0)
    const double w_skills   = 0.30;
    const double w_exp      = 0.28;
    const double w_semantic = 0.20;
    const double w_edu      = 0.10;
    const double w_projects = 0.07;
    const double w_quality  = 0.05;

    double score = (cat.skills         * w_skills +
                    cat.experience      * w_exp +
                    cat.semantic        * w_semantic +
                    cat.education       * w_edu +
                    cat.projects        * w_projects +
                    cat.resume_quality  * w_quality) / 100.0;

    return std::max(0.0, std::min(1.0, score));
}

double XGBoostRanker::predict(const FeatureVector& fv) const {
    if (model_loaded_) return xgb_predict(fv);
    return linear_fallback(fv);
}

} // namespace talentmatch
