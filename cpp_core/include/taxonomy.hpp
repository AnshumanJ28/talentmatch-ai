#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace talentmatch {

// ---------------------------------------------------------------------------
// A single entry in the skill taxonomy
// ---------------------------------------------------------------------------
struct TaxonomyEntry {
    std::string              canonical_name;
    std::string              category;
    std::vector<std::string> aliases;  // all normalized lowercase aliases
};

// ---------------------------------------------------------------------------
// The full skill taxonomy, keyed by normalized alias → TaxonomyEntry
// ---------------------------------------------------------------------------
struct SkillTaxonomy {
    std::string              version;
    std::vector<std::string> categories;
    std::vector<TaxonomyEntry> skills;

    // Fast alias → entry lookup (built on load)
    std::unordered_map<std::string, const TaxonomyEntry*> alias_index;

    void build_index() {
        alias_index.clear();
        for (const auto& entry : skills) {
            // Index canonical name (normalized lowercase)
            alias_index[normalize(entry.canonical_name)] = &entry;
            // Index all aliases
            for (const auto& alias : entry.aliases) {
                alias_index[normalize(alias)] = &entry;
            }
        }
    }

    // Normalize: lowercase, strip punctuation except ++ # .
    static std::string normalize(const std::string& s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            if (std::isalpha(static_cast<unsigned char>(c))) {
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            } else if (c == '+' || c == '#' || c == '.' || c == ' ') {
                result += c;
            } else if (c == '_' || c == '-') {
                result += ' ';
            }
        }
        // Collapse multiple spaces
        std::string collapsed;
        bool last_space = false;
        for (char c : result) {
            if (c == ' ') {
                if (!last_space) collapsed += ' ';
                last_space = true;
            } else {
                collapsed += c;
                last_space = false;
            }
        }
        // Trim
        if (!collapsed.empty() && collapsed.front() == ' ') collapsed = collapsed.substr(1);
        if (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
        return collapsed;
    }

    const TaxonomyEntry* find(const std::string& raw_skill) const {
        auto key = normalize(raw_skill);
        auto it = alias_index.find(key);
        return (it != alias_index.end()) ? it->second : nullptr;
    }
};

} // namespace talentmatch
