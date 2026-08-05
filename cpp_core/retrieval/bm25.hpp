#pragma once
#include "../include/features.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace talentmatch {

// ---------------------------------------------------------------------------
// Text preprocessing shared across retrieval modules
// ---------------------------------------------------------------------------
struct TextTokens {
    std::vector<std::string>              tokens;         // ordered token list
    std::unordered_map<std::string, int>  term_freq;      // TF counts
    std::unordered_set<std::string>       unique_terms;
    int                                   total_tokens{0};

    static TextTokens build(const std::string& text);
};

// ---------------------------------------------------------------------------
// BM25Scorer  — standard BM25 retrieval score
// Parameters: k1=1.5, b=0.75 (standard Elasticsearch defaults)
// ---------------------------------------------------------------------------
class BM25Scorer {
public:
    BM25Scorer(double k1 = 1.5, double b = 0.75);

    // Score a single document against a query (both provided as raw text)
    // Returns unnormalized BM25 score (non-negative)
    double score(const std::string& doc_text, const std::string& query_text) const;

    // Normalize to [0, 1] via sigmoid-like mapping
    static double normalize(double raw_score, double scale = 10.0);
    static const std::unordered_set<std::string>& stop_words();

private:
    double k1_, b_;
};

// ---------------------------------------------------------------------------
// TFIDFScorer  — cosine similarity between TF-IDF vectors
// IDF is estimated from a pre-built reference document frequency table
// (common English + tech vocabulary, embedded in the binary).
// ---------------------------------------------------------------------------
class TFIDFScorer {
public:
    TFIDFScorer();

    // Returns cosine similarity ∈ [0, 1] between TF-IDF vectors of doc and query
    double cosine_similarity(const std::string& doc_text,
                              const std::string& query_text) const;

private:
    // Pre-computed IDF for ~2000 tech vocabulary terms
    // Lower IDF = more common (less discriminating)
    std::unordered_map<std::string, double> idf_;

    double idf_for(const std::string& term) const;
    static std::unordered_map<std::string, double> build_default_idf();
};

// ---------------------------------------------------------------------------
// RetrievalFeatureExtractor  — computes all 15 retrieval features
// ---------------------------------------------------------------------------
class RetrievalFeatureExtractor {
public:
    RetrievalFeatureExtractor();

    void extract(const std::string& resume_text,
                 const std::string& jd_text,
                 const std::vector<float>& resume_embedding,
                 const std::vector<float>& jd_embedding,
                 FeatureVector& fv) const;

private:
    BM25Scorer   bm25_;
    TFIDFScorer  tfidf_;

    static double cosine_embedding(const std::vector<float>& a,
                                   const std::vector<float>& b);
    static double keyword_overlap(const TextTokens& a, const TextTokens& b);
    static double bigram_overlap(const std::vector<std::string>& a_tokens,
                                  const std::vector<std::string>& b_tokens);
    static double term_coverage(const TextTokens& query, const TextTokens& doc);
    static int    count_tech_terms(const TextTokens& t);
    static double tf_weighted_keyword_score(const TextTokens& resume,
                                             const TextTokens& jd);
};

} // namespace talentmatch
