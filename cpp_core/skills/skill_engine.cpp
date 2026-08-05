#include "skill_engine.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace talentmatch {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// SkillEngine constructor  — loads taxonomy, builds trie
// ---------------------------------------------------------------------------
SkillEngine::SkillEngine(const SkillTaxonomy& taxonomy)
    : taxonomy_(taxonomy)
{
    // Build Aho-Corasick trie
    for (const auto& entry : taxonomy_.skills) {
        trie_.add_pattern(entry.canonical_name, entry.canonical_name, entry.category);
        for (const auto& alias : entry.aliases) {
            trie_.add_pattern(alias, entry.canonical_name, entry.category);
        }
    }
    trie_.build();

    // Build synonym matcher entries
    std::vector<SynonymMatcher::Entry> sm_entries;
    for (const auto& entry : taxonomy_.skills) {
        SynonymMatcher::Entry e;
        e.normalized    = SkillTaxonomy::normalize(entry.canonical_name);
        e.canonical_name = entry.canonical_name;
        e.category       = entry.category;
        sm_entries.push_back(std::move(e));
    }
    synonym_matcher_ = std::make_unique<SynonymMatcher>(std::move(sm_entries), 0.82f);
}

// ---------------------------------------------------------------------------
// detect_primary_domain
// ---------------------------------------------------------------------------
std::string SkillEngine::detect_primary_domain(const std::vector<SkillMatch>& skills) const {
    std::unordered_map<std::string, float> weights;
    for (const auto& s : skills) {
        weights[s.category] += s.confidence;
    }
    std::string best;
    float best_w = -1;
    for (const auto& [cat, w] : weights) {
        if (w > best_w) { best_w = w; best = cat; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// extract  — from raw text + optional explicit skills list
// ---------------------------------------------------------------------------
SkillExtractionResult SkillEngine::extract(const std::string& text,
                                            const std::vector<std::string>& raw_skills) const
{
    std::unordered_map<std::string, SkillMatch> seen; // canonical → best match

    auto add_match = [&](const std::string& canonical, const std::string& category,
                          float confidence, const std::string& method) {
        auto it = seen.find(canonical);
        if (it == seen.end()) {
            seen[canonical] = {canonical, category, confidence, method};
        } else if (confidence > it->second.confidence) {
            it->second.confidence  = confidence;
            it->second.match_method = method;
        }
    };

    // 1. Trie scan over full text
    auto trie_matches = trie_.scan(text);
    for (const auto& m : trie_matches) {
        add_match(m.canonical_name, m.category, 0.95f, "trie");
    }

    // 2. Explicit raw_skills list — exact lookup, then fuzzy fallback
    for (const auto& raw : raw_skills) {
        const TaxonomyEntry* e = taxonomy_.find(raw);
        if (e) {
            add_match(e->canonical_name, e->category, 1.0f, "exact");
        } else {
            // Fuzzy fallback
            auto fm = synonym_matcher_->match(raw);
            if (fm) {
                add_match(fm->canonical_name, fm->category, fm->similarity * 0.85f, "fuzzy");
            }
        }
    }

    // Convert map to vector
    SkillExtractionResult result;
    for (const auto& [canon, sm] : seen) {
        result.matched_skills.push_back(sm);
    }
    std::sort(result.matched_skills.begin(), result.matched_skills.end(),
        [](const SkillMatch& a, const SkillMatch& b){ return a.confidence > b.confidence; });

    result.primary_domain = detect_primary_domain(result.matched_skills);

    // Primary domain confidence
    if (!result.matched_skills.empty()) {
        float total_conf = 0.0f;
        float domain_conf = 0.0f;
        for (const auto& s : result.matched_skills) {
            total_conf += s.confidence;
            if (s.category == result.primary_domain) domain_conf += s.confidence;
        }
        result.primary_domain_confidence = (total_conf > 0) ? domain_conf / total_conf : 0.0f;
    }

    return result;
}

// ---------------------------------------------------------------------------
// gap_analysis
// ---------------------------------------------------------------------------
SkillGapResult SkillEngine::gap_analysis(
    const std::vector<SkillMatch>& candidate_skills,
    const std::vector<SkillMatch>& jd_skills) const
{
    SkillGapResult result;

    std::unordered_set<std::string> candidate_set;
    for (const auto& s : candidate_skills) candidate_set.insert(s.canonical_name);

    for (const auto& jd_skill : jd_skills) {
        if (candidate_set.count(jd_skill.canonical_name)) {
            result.matched_skills.push_back(jd_skill.canonical_name);
        } else {
            // Check for fuzzy partial match among candidate skills
            float best_sim = 0.0f;
            std::string best_match;
            for (const auto& cs : candidate_skills) {
                // Simple category-based partial: same category = partial
                float sim = 0.0f;
                if (cs.category == jd_skill.category) sim = 0.45f;
                // Fuzzy name similarity
                auto fm = synonym_matcher_->match(jd_skill.canonical_name);
                if (fm && fm->canonical_name == cs.canonical_name) {
                    sim = std::max(sim, fm->similarity);
                }
                if (sim > best_sim) {
                    best_sim = sim;
                    best_match = cs.canonical_name;
                }
            }
            if (best_sim >= 0.50f) {
                result.partial_skills.push_back({jd_skill.canonical_name, best_match, best_sim});
            } else {
                result.missing_skills.push_back(jd_skill.canonical_name);
            }
        }
    }

    int total = static_cast<int>(jd_skills.size());
    result.match_ratio = (total > 0)
        ? static_cast<float>(result.matched_skills.size()) / static_cast<float>(total)
        : 1.0f;

    return result;
}

} // namespace talentmatch
