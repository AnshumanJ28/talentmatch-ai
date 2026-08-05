#include "synonym_matcher.hpp"
#include <algorithm>
#include <cctype>
#include <vector>
#include <limits>

namespace talentmatch {

// ---------------------------------------------------------------------------
// normalize
// ---------------------------------------------------------------------------
std::string SynonymMatcher::normalize(const std::string& s) {
    std::string r;
    for (unsigned char c : s) {
        if (std::isalpha(c)) r += static_cast<char>(std::tolower(c));
        else if (std::isdigit(c) || c == '+' || c == '#') r += static_cast<char>(c);
        else r += ' ';
    }
    // Trim + collapse spaces
    std::string out;
    bool sp = true; // skip leading spaces
    for (char c : r) {
        if (c == ' ') { if (!sp) out += ' '; sp = true; }
        else { out += c; sp = false; }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// ---------------------------------------------------------------------------
// edit_distance  — standard Wagner-Fischer DP
// ---------------------------------------------------------------------------
int SynonymMatcher::edit_distance(const std::string& a, const std::string& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (a[i-1] == b[j-1]) dp[i][j] = dp[i-1][j-1];
            else dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
        }
    }
    return dp[n][m];
}

// ---------------------------------------------------------------------------
// similarity  → [0.0, 1.0]
// ---------------------------------------------------------------------------
float SynonymMatcher::similarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0f;
    int max_len = static_cast<int>(std::max(a.size(), b.size()));
    if (max_len == 0) return 1.0f;
    int dist = edit_distance(a, b);
    return 1.0f - static_cast<float>(dist) / static_cast<float>(max_len);
}

// ---------------------------------------------------------------------------
// SynonymMatcher constructor
// ---------------------------------------------------------------------------
SynonymMatcher::SynonymMatcher(std::vector<Entry> entries, float threshold)
    : entries_(std::move(entries)), threshold_(threshold)
{}

// ---------------------------------------------------------------------------
// match
// ---------------------------------------------------------------------------
std::optional<FuzzyMatch> SynonymMatcher::match(const std::string& query) const {
    std::string norm_query = normalize(query);
    if (norm_query.empty()) return std::nullopt;

    float best_sim = 0.0f;
    const Entry* best_entry = nullptr;

    for (const auto& e : entries_) {
        float sim = similarity(norm_query, e.normalized);
        if (sim > best_sim) {
            best_sim = sim;
            best_entry = &e;
        }
    }

    if (best_entry && best_sim >= threshold_) {
        return FuzzyMatch{best_entry->canonical_name, best_entry->category, best_sim};
    }
    return std::nullopt;
}

} // namespace talentmatch
