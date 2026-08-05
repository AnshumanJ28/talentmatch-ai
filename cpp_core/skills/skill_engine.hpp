#pragma once
#include "../include/taxonomy.hpp"
#include "trie.hpp"
#include "synonym_matcher.hpp"
#include "../include/resume.hpp"
#include <string>
#include <vector>
#include <memory>

namespace talentmatch {

struct SkillExtractionResult {
    std::vector<SkillMatch> matched_skills;  // from candidate resume
    std::string             primary_domain;
    float                   primary_domain_confidence{0.0f};
};

// ---------------------------------------------------------------------------
// SkillEngine
//
// Encapsulates: Taxonomy loading, Aho-Corasick trie, fuzzy fallback.
// Provides:
//   - extract_from_resume(resume)   → candidate's skills
//   - extract_from_jd(jd_text)     → JD's required/mentioned skills
//   - gap_analysis(candidate, jd)  → matched/missing/partial
// ---------------------------------------------------------------------------
class SkillEngine {
public:
    explicit SkillEngine(const SkillTaxonomy& taxonomy);

    SkillExtractionResult extract(const std::string& text,
                                  const std::vector<std::string>& raw_skills = {}) const;

    SkillGapResult gap_analysis(const std::vector<SkillMatch>& candidate_skills,
                                const std::vector<SkillMatch>& jd_skills) const;

    const SkillTaxonomy& taxonomy() const { return taxonomy_; }

private:
    SkillTaxonomy              taxonomy_;
    AhoCorasickTrie            trie_;
    std::unique_ptr<SynonymMatcher> synonym_matcher_;

    std::string detect_primary_domain(const std::vector<SkillMatch>& skills) const;
};

} // namespace talentmatch
