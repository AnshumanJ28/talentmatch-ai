#pragma once
#include <string>
#include <vector>
#include <optional>

namespace talentmatch {

// ---------------------------------------------------------------------------
// SynonymMatcher
//
// Fuzzy fallback for skill terms that miss the Aho-Corasick trie.
// Uses normalized Levenshtein edit distance to find the closest taxonomy
// entry above a similarity threshold.
//
// This is intentionally slow (O(n × m) per query) but is only called for
// skills that failed exact + trie matching — typically a small fraction.
// ---------------------------------------------------------------------------

struct FuzzyMatch {
    std::string canonical_name;
    std::string category;
    float       similarity{0.0f}; // 0.0 – 1.0
};

class SynonymMatcher {
public:
    struct Entry {
        std::string normalized; // pre-normalized canonical name
        std::string canonical_name;
        std::string category;
    };

    explicit SynonymMatcher(std::vector<Entry> entries,
                            float threshold = 0.82f);

    std::optional<FuzzyMatch> match(const std::string& query) const;

private:
    std::vector<Entry> entries_;
    float              threshold_;

    static std::string normalize(const std::string& s);

    // Normalized edit distance ∈ [0, 1]  (1 = identical)
    static float similarity(const std::string& a, const std::string& b);
    static int   edit_distance(const std::string& a, const std::string& b);
};

} // namespace talentmatch
