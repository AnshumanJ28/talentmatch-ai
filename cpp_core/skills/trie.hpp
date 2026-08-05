#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>

namespace talentmatch {

// ---------------------------------------------------------------------------
// AhoCorasickTrie
//
// Multi-pattern string matcher. Build it once with all canonical skill names
// and their aliases, then call scan() on any text block to find all matches
// in O(n + m) time (n = text length, m = total match length).
//
// Case-insensitive matching: all patterns and input are normalized to
// lowercase before insertion/scanning.
// ---------------------------------------------------------------------------

struct TrieMatch {
    size_t      start;          // byte offset in search text
    size_t      end;            // exclusive byte offset
    std::string canonical_name; // the taxonomy canonical name
    std::string category;
    std::string matched_text;   // the actual text that matched
};

struct TrieNode {
    std::unordered_map<char, int> children;
    int                           fail{0};
    std::string                   canonical_name; // non-empty if this is a terminal
    std::string                   category;
    int                           pattern_len{0}; // length of the pattern ending here
};

class AhoCorasickTrie {
public:
    AhoCorasickTrie();

    // Add a pattern (alias/canonical name) that maps to a canonical skill
    void add_pattern(const std::string& pattern,
                     const std::string& canonical_name,
                     const std::string& category);

    // Build failure links — MUST be called after all add_pattern() calls
    void build();

    // Scan text and return all non-overlapping skill matches
    // Longer matches are preferred over shorter ones at the same position.
    std::vector<TrieMatch> scan(const std::string& text) const;

private:
    std::vector<TrieNode> nodes_;
    bool                  built_{false};

    static std::string normalize(const std::string& s);
    int new_node();
};

} // namespace talentmatch
