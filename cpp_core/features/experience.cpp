#include "experience.hpp"
#include <regex>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cmath>
#include <unordered_set>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace talentmatch {

static std::string tl_lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

// ---------------------------------------------------------------------------
// Date parsing helpers
// ---------------------------------------------------------------------------
static int month_from_str(const std::string& s) {
    std::string l = s;
    for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::unordered_map<std::string, int> months = {
        {"jan",1},{"feb",2},{"mar",3},{"apr",4},{"may",5},{"jun",6},
        {"jul",7},{"aug",8},{"sep",9},{"oct",10},{"nov",11},{"dec",12},
        {"january",1},{"february",2},{"march",3},{"april",4},{"june",6},
        {"july",7},{"august",8},{"september",9},{"october",10},{"november",11},{"december",12}
    };
    auto it = months.find(l.substr(0, std::min<size_t>(l.size(),9)));
    return (it != months.end()) ? it->second : 1;
}

static std::pair<int,int> parse_ym(const std::string& s) {
    // Returns (year, month) or (0,0) on failure
    if (s.empty()) return {0,0};
    std::string l = s;
    for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l == "present" || l == "current" || l == "now") {
        // Approximate as 2026
        return {2026, 8};
    }
    static const std::regex yr_re(R"(\b(19|20)(\d{2})\b)");
    static const std::regex mo_yr_re(R"((jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec\w*)\s+(20\d{2}|19\d{2}))",
        std::regex_constants::icase);
    std::smatch m;
    if (std::regex_search(s, m, mo_yr_re)) {
        return {std::stoi(m[2].str()), month_from_str(m[1].str())};
    }
    if (std::regex_search(s, m, yr_re)) {
        return {std::stoi(m[0].str()), 6}; // default to mid-year
    }
    return {0, 0};
}

static double months_between(const std::string& start, const std::string& end) {
    auto [sy, sm] = parse_ym(start);
    auto [ey, em] = parse_ym(end.empty() ? "present" : end);
    if (sy == 0) return 0.0;
    double months = static_cast<double>((ey - sy) * 12 + (em - sm));
    return std::max(0.0, months);
}

// ---------------------------------------------------------------------------
// compute_seniority_score
// ---------------------------------------------------------------------------
double ExperienceFeatureExtractor::compute_seniority_score(const std::string& title) {
    std::string l = title;
    for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (l.find("principal") != std::string::npos || l.find("fellow") != std::string::npos) return 1.0;
    if (l.find("staff") != std::string::npos)    return 0.90;
    if (l.find("lead") != std::string::npos)     return 0.80;
    if (l.find("senior") != std::string::npos || l.find("sr.") != std::string::npos) return 0.70;
    if (l.find("mid") != std::string::npos)      return 0.50;
    if (l.find("junior") != std::string::npos || l.find("jr.") != std::string::npos) return 0.30;
    if (l.find("intern") != std::string::npos || l.find("trainee") != std::string::npos) return 0.15;
    return 0.45; // default: assume mid-level
}

// ---------------------------------------------------------------------------
// has_leadership
// ---------------------------------------------------------------------------
bool ExperienceFeatureExtractor::has_leadership(const std::string& text) {
    std::string l = text;
    for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::vector<std::string> kws = {
        "led", "managed", "directed", "supervised", "mentored", "coached",
        "oversaw", "spearheaded", "headed", "coordinated team", "built team",
        "hired", "grew team", "team of"
    };
    for (const auto& kw : kws)
        if (l.find(kw) != std::string::npos) return true;
    return false;
}

// ---------------------------------------------------------------------------
// count_quantified  — lines with numbers / percentages / dollar amounts
// ---------------------------------------------------------------------------
int ExperienceFeatureExtractor::count_quantified(const std::vector<std::string>& bullets) {
    static const std::regex quant_re(R"(\d+\s*(%|x|×|k|\$|million|billion|users|requests|ms|seconds|hours|days|months|years|points|percent))");
    int count = 0;
    for (const auto& b : bullets) {
        if (std::regex_search(b, quant_re)) ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------
// action_verb_density
// ---------------------------------------------------------------------------
double ExperienceFeatureExtractor::action_verb_density(const std::vector<std::string>& bullets) {
    if (bullets.empty()) return 0.0;
    static const std::unordered_set<std::string> action_verbs = {
        "built","developed","designed","implemented","created","led","managed",
        "improved","optimized","reduced","increased","launched","deployed","authored",
        "architected","engineered","refactored","migrated","integrated","automated",
        "delivered","shipped","released","established","collaborated","drove","owned",
        "transformed","scaled","revamped","streamlined","mentored","trained","wrote",
        "analyzed","researched","investigated","resolved","fixed","debugged",
        "maintained","monitored","configured","administered","supported","reviewed"
    };
    int total = 0, with_verb = 0;
    for (const auto& b : bullets) {
        if (b.empty()) continue;
        ++total;
        // First word of the bullet
        std::istringstream ss(b);
        std::string first;
        ss >> first;
        std::string fl = first;
        for (auto& c : fl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (action_verbs.count(fl)) ++with_verb;
    }
    return (total > 0) ? static_cast<double>(with_verb) / total : 0.0;
}

// ---------------------------------------------------------------------------
// jd_seniority_level
// ---------------------------------------------------------------------------
double ExperienceFeatureExtractor::jd_seniority_level(const std::string& jd_text) {
    std::string l = jd_text;
    for (auto& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l.find("principal") != std::string::npos || l.find("staff") != std::string::npos) return 1.0;
    if (l.find("lead") != std::string::npos)   return 0.80;
    if (l.find("senior") != std::string::npos) return 0.70;
    if (l.find("junior") != std::string::npos || l.find("entry level") != std::string::npos) return 0.25;
    return 0.50; // mid-level default
}

// ---------------------------------------------------------------------------
// career_progression  — score based on seniority increase over time
// ---------------------------------------------------------------------------
double ExperienceFeatureExtractor::career_progression(const std::vector<ExperienceEntry>& exp) {
    if (exp.size() < 2) return 0.5;
    // Score first and last jobs by seniority
    double first = compute_seniority_score(exp.front().job_title);
    double last  = compute_seniority_score(exp.back().job_title);
    // If last > first → progression happened
    double delta = last - first;
    return std::max(0.0, std::min(1.0, 0.5 + delta));
}

// ---------------------------------------------------------------------------
// extract  — main entry point, fills 15 features into fv
// ---------------------------------------------------------------------------
void ExperienceFeatureExtractor::extract(const Resume& resume,
                                          const std::string& jd_text,
                                          FeatureVector& fv)
{
    const auto& exp = resume.experience;
    if (exp.empty()) {
        // Zero out all experience features
        for (const auto& nm : {"total_experience_months","relevant_experience_months",
            "num_positions","avg_tenure_months","max_tenure_months","employment_gap_months",
            "num_gaps","is_currently_employed","max_seniority_score","avg_seniority_score",
            "has_leadership_keywords","num_promotions","quantified_achievement_count",
            "action_verb_density","career_progression_score"}) {
            fv.set(nm, 0.0);
        }
        return;
    }

    // Compute per-entry durations
    std::vector<double> durations;
    double total_months = 0.0;
    double max_months   = 0.0;
    bool   is_current   = false;

    for (const auto& e : exp) {
        double dur = months_between(e.start_date, e.end_date);
        durations.push_back(dur);
        total_months += dur;
        max_months = std::max(max_months, dur);
        if (e.is_current) is_current = true;
    }
    double avg_tenure = total_months / static_cast<double>(exp.size());

    // Employment gaps: sort entries by start date and find gaps
    double gap_months = 0.0;
    int    num_gaps   = 0;
    // (simplified: if entries are sorted, check adjacent end/start)
    // For simplicity, we estimate gaps from total calendar span
    double total_span = months_between(exp.back().start_date, "present");
    if (total_span > total_months) {
        gap_months = total_span - total_months;
        num_gaps   = std::max(1, static_cast<int>(gap_months / 12.0));
    }

    // Seniority
    double max_seniority = 0.0, sum_seniority = 0.0;
    for (const auto& e : exp) {
        double s = compute_seniority_score(e.job_title);
        max_seniority = std::max(max_seniority, s);
        sum_seniority += s;
    }
    double avg_seniority = sum_seniority / static_cast<double>(exp.size());

    // Leadership
    bool leadership = false;
    std::vector<std::string> all_bullets;
    for (const auto& e : exp) {
        for (const auto& r : e.responsibilities) all_bullets.push_back(r);
        if (!leadership) {
            leadership = has_leadership(e.job_title) || has_leadership(e.company);
            for (const auto& r : e.responsibilities) {
                if (has_leadership(r)) { leadership = true; break; }
            }
        }
    }

    // Quantification
    int quant_count = count_quantified(all_bullets);

    // Action verb density
    double av_density = action_verb_density(all_bullets);

    // Promotions: same company with increasing seniority
    int promotions = 0;
    for (size_t i = 1; i < exp.size(); ++i) {
        if (tl_lower(exp[i].company) == tl_lower(exp[i-1].company)) {
            if (compute_seniority_score(exp[i].job_title) >
                compute_seniority_score(exp[i-1].job_title) + 0.1) {
                ++promotions;
            }
        }
    }

    // JD seniority alignment
    double jd_seniority = jd_seniority_level(jd_text);
    double seniority_match = 1.0 - std::abs(max_seniority - jd_seniority);

    // Relevant experience: approximate as total × seniority match
    double relevant_months = total_months * seniority_match;

    // Normalize features to [0, 1]
    static const double MONTHS_CAP = 240.0; // 20 years
    fv.set("total_experience_months",   std::min(1.0, total_months / MONTHS_CAP));
    fv.set("relevant_experience_months",std::min(1.0, relevant_months / MONTHS_CAP));
    fv.set("num_positions",             std::min(1.0, static_cast<double>(exp.size()) / 10.0));
    fv.set("avg_tenure_months",         std::min(1.0, avg_tenure / 60.0));
    fv.set("max_tenure_months",         std::min(1.0, max_months / 120.0));
    fv.set("employment_gap_months",     1.0 - std::min(1.0, gap_months / 24.0)); // lower gap = better
    fv.set("num_gaps",                  1.0 - std::min(1.0, static_cast<double>(num_gaps) / 5.0));
    fv.set("is_currently_employed",     is_current ? 1.0 : 0.0);
    fv.set("max_seniority_score",       max_seniority);
    fv.set("avg_seniority_score",       avg_seniority);
    fv.set("has_leadership_keywords",   leadership ? 1.0 : 0.0);
    fv.set("num_promotions",            std::min(1.0, static_cast<double>(promotions) / 3.0));
    fv.set("quantified_achievement_count", std::min(1.0, static_cast<double>(quant_count) / 10.0));
    fv.set("action_verb_density",       av_density);
    fv.set("career_progression_score",  career_progression(exp));
}

} // namespace talentmatch
