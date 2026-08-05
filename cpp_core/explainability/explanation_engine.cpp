#include "explanation_engine.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

namespace talentmatch {

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------
std::string ExplanationEngine::fmt_pct(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(0) << (v * 100.0) << "%";
    return ss.str();
}

std::string ExplanationEngine::fmt_months(double v_normalized) {
    // v_normalized is feature value [0,1]; total_experience_months cap = 240
    int months = static_cast<int>(v_normalized * 240.0);
    if (months >= 12) {
        int years = months / 12;
        int rem   = months % 12;
        if (rem == 0) return std::to_string(years) + " yr" + (years > 1 ? "s" : "");
        return std::to_string(years) + " yr " + std::to_string(rem) + " mo";
    }
    return std::to_string(months) + " month" + (months != 1 ? "s" : "");
}

// ---------------------------------------------------------------------------
// Rule table
// ---------------------------------------------------------------------------
std::vector<ExplanationEngine::Rule> ExplanationEngine::build_rules() {
    return {
        // Skills
        {"skill_coverage_ratio",    0.80, 0.40,
         "Excellent skill match — {value} of required skills aligned",
         "Skill gap — only {value} of required skills found"},
        {"skill_has_all_required",  0.90, 0.50,
         "All critical skills present",
         "Missing critical required skills"},
        {"skill_category_diversity",0.60, 0.20,
         "Broad technology expertise across multiple domains",
         "Limited skill diversity — consider broadening expertise"},
        {"avg_skill_confidence",    0.85, 0.50,
         "High-confidence skill matches across the profile",
         "Skill matches have low confidence — verify skill claims"},

        // Experience
        {"relevant_experience_months", 0.50, 0.15,
         "Strong relevant experience — {value}",
         "Limited relevant experience — {value}"},
        {"max_seniority_score",    0.70, 0.30,
         "Seniority level aligns well with position requirements",
         "Seniority level may not meet position requirements"},
        {"has_leadership_keywords",0.80, 0.10,
         "Demonstrated leadership and team management experience",
         "No leadership indicators found — consider highlighting management experience"},
        {"quantified_achievement_count", 0.60, 0.15,
         "Well-quantified achievements demonstrate measurable impact",
         "Few quantified achievements — add metrics to strengthen impact"},
        {"action_verb_density",    0.70, 0.30,
         "Strong action-oriented language throughout experience",
         "Weak action verb usage — use stronger action verbs in bullet points"},
        {"career_progression_score",0.65, 0.35,
         "Clear career progression demonstrated",
         "Career progression unclear or inconsistent"},
        {"employment_gap_months",  0.70, 0.30,
         "Continuous employment history with minimal gaps",
         "Notable employment gaps in work history"},

        // Education
        {"highest_degree_level",   0.80, 0.40,
         "Education level meets or exceeds requirements",
         "Education level may be below requirements"},
        {"degree_field_match",     0.70, 0.20,
         "Field of study directly relevant to the role",
         "Field of study not closely aligned with role requirements"},

        // Projects
        {"project_tech_overlap",   0.70, 0.20,
         "Project portfolio demonstrates relevant technology experience",
         "Project work does not show relevant technology alignment"},
        {"project_domain_relevance",0.65, 0.20,
         "Projects directly applicable to the job domain",
         "Project domain diverges from job requirements"},

        // Resume quality
        {"section_completeness_score",0.80, 0.40,
         "Resume is comprehensive and well-structured",
         "Resume is missing key sections — consider adding summary/projects"},
        {"quantified_achievements_total",0.50, 0.10,
         "Resume demonstrates strong impact with measurable results",
         "Resume lacks quantified results — add numbers to demonstrate impact"},

        // Retrieval / Semantic
        {"cosine_similarity",      0.75, 0.40,
         "Strong semantic alignment with job description",
         "Resume language does not closely match job description vocabulary"},
        {"bm25_score",             0.65, 0.25,
         "High keyword alignment with job requirements",
         "Resume does not contain enough job-relevant keywords"},
        {"keyword_overlap_ratio",  0.60, 0.20,
         "Good keyword coverage of job description terms",
         "Low keyword overlap with job description — consider tailoring resume"},
    };
}

// ---------------------------------------------------------------------------
// explain
// ---------------------------------------------------------------------------
ExplanationEngine::ExplanationResult ExplanationEngine::explain(
    const FeatureVector& fv,
    const CategoryScores& scores,
    const SkillGapResult& gap) const
{
    auto rules = build_rules();

    // Collect (factor_text, score) for positive and negative
    struct Candidate {
        std::string text;
        double      weight;
    };
    std::vector<Candidate> positives, negatives;

    for (const auto& rule : rules) {
        double val = fv.get(rule.feature_name);
        std::string val_str = fmt_pct(val);

        // Special formatting for experience months
        if (rule.feature_name == "relevant_experience_months" ||
            rule.feature_name == "total_experience_months") {
            val_str = fmt_months(val);
        }

        // Replace {value} placeholder
        auto fill = [&](std::string tpl) -> std::string {
            size_t pos = tpl.find("{value}");
            if (pos != std::string::npos) tpl.replace(pos, 7, val_str);
            return tpl;
        };

        if (val >= rule.positive_threshold) {
            positives.push_back({fill(rule.positive_template), val});
        } else if (val < rule.negative_threshold) {
            negatives.push_back({fill(rule.negative_template), 1.0 - val});
        }
    }

    // Add skill gap specifics
    if (!gap.missing_skills.empty()) {
        std::string missing_list;
        int show = std::min(3, static_cast<int>(gap.missing_skills.size()));
        for (int i = 0; i < show; ++i) {
            if (i > 0) missing_list += ", ";
            missing_list += gap.missing_skills[i];
        }
        if (static_cast<int>(gap.missing_skills.size()) > show) {
            missing_list += " +" + std::to_string(gap.missing_skills.size() - show) + " more";
        }
        negatives.push_back({"Missing required skills: " + missing_list, 0.9});
    }

    if (!gap.matched_skills.empty()) {
        std::string match_list;
        int show = std::min(3, static_cast<int>(gap.matched_skills.size()));
        for (int i = 0; i < show; ++i) {
            if (i > 0) match_list += ", ";
            match_list += gap.matched_skills[i];
        }
        positives.push_back({"Strong alignment: " + match_list, 0.85});
    }

    // Sort by weight descending and take top 5
    auto cmp = [](const Candidate& a, const Candidate& b){ return a.weight > b.weight; };
    std::sort(positives.begin(), positives.end(), cmp);
    std::sort(negatives.begin(), negatives.end(), cmp);

    ExplanationResult result;
    for (int i = 0; i < std::min(5, static_cast<int>(positives.size())); ++i)
        result.positive_factors.push_back(positives[i].text);
    for (int i = 0; i < std::min(5, static_cast<int>(negatives.size())); ++i)
        result.negative_factors.push_back(negatives[i].text);

    // Fallback: ensure at least one of each
    if (result.positive_factors.empty()) {
        double overall = (scores.skills + scores.experience + scores.semantic) / 3.0;
        if (overall >= 40.0)
            result.positive_factors.push_back("Candidate shows baseline alignment with the role");
    }
    if (result.negative_factors.empty()) {
        result.negative_factors.push_back("No significant gaps identified — review manually for culture fit");
    }

    return result;
}

} // namespace talentmatch
