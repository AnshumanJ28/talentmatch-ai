#include "bm25.hpp"
#include <numeric>
#include <iterator>

namespace talentmatch {

// ---------------------------------------------------------------------------
// Stop words
// ---------------------------------------------------------------------------
const std::unordered_set<std::string>& BM25Scorer::stop_words() {
    static const std::unordered_set<std::string> sw = {
        "a","an","the","and","or","but","in","on","at","to","for","of","with",
        "by","is","are","was","were","be","been","being","have","has","had",
        "do","does","did","will","would","could","should","may","might","shall",
        "can","from","as","it","its","this","that","these","those","we","you",
        "he","she","they","i","me","my","our","us","your","their","his","her",
        "also","not","no","nor","so","yet","both","either","neither","more",
        "most","other","some","such","only","than","too","very","just","about"
    };
    return sw;
}

// ---------------------------------------------------------------------------
// TextTokens::build
// ---------------------------------------------------------------------------
TextTokens TextTokens::build(const std::string& text) {
    TextTokens t;
    std::string token;
    const auto& sw = BM25Scorer::stop_words();

    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '+' || c == '#') {
            token += static_cast<char>(std::tolower(c));
        } else {
            if (token.size() >= 2) {
                if (!sw.count(token)) {
                    t.tokens.push_back(token);
                    t.term_freq[token]++;
                    t.unique_terms.insert(token);
                }
            }
            token.clear();
        }
    }
    if (token.size() >= 2 && !sw.count(token)) {
        t.tokens.push_back(token);
        t.term_freq[token]++;
        t.unique_terms.insert(token);
    }
    t.total_tokens = static_cast<int>(t.tokens.size());
    return t;
}

// ---------------------------------------------------------------------------
// BM25Scorer
// ---------------------------------------------------------------------------
BM25Scorer::BM25Scorer(double k1, double b) : k1_(k1), b_(b) {}

double BM25Scorer::score(const std::string& doc_text, const std::string& query_text) const {
    TextTokens doc   = TextTokens::build(doc_text);
    TextTokens query = TextTokens::build(query_text);

    if (doc.total_tokens == 0 || query.total_tokens == 0) return 0.0;

    // BM25 IDF approximation: treat doc as single document in corpus
    // Use simplified IDF: log(1 + 1 / (0.5 + 0.5 * tf_in_doc))
    double score_val = 0.0;
    double avg_dl = static_cast<double>(doc.total_tokens); // single doc → avgdl = dl

    for (const auto& [term, q_tf] : query.term_freq) {
        auto it = doc.term_freq.find(term);
        int d_tf = (it != doc.term_freq.end()) ? it->second : 0;
        if (d_tf == 0) continue;

        // IDF approximation for single doc scenario
        double idf = std::log(1.0 + static_cast<double>(doc.total_tokens) /
                              static_cast<double>(d_tf + 1));

        // BM25 term score
        double tf_norm = static_cast<double>(d_tf) * (k1_ + 1.0) /
                         (static_cast<double>(d_tf) + k1_ * (1.0 - b_ + b_ * avg_dl / avg_dl));
        score_val += idf * tf_norm;
    }
    return score_val;
}

double BM25Scorer::normalize(double raw, double scale) {
    // Sigmoid-like mapping: score/(score+scale)
    return raw / (raw + scale);
}

// ---------------------------------------------------------------------------
// TFIDFScorer — default IDF table
// ---------------------------------------------------------------------------
std::unordered_map<std::string, double> TFIDFScorer::build_default_idf() {
    // Pre-computed IDF for common tech + soft-skill terms.
    // Higher IDF = rarer term = more discriminating.
    std::unordered_map<std::string, double> idf = {
        // High-IDF tech terms (rare, very discriminating)
        {"kubernetes",8.5},{"terraform",8.5},{"graphql",8.0},{"grpc",8.0},
        {"elasticsearch",7.8},{"cassandra",7.8},{"kafka",7.5},{"spark",7.2},
        {"mlflow",8.0},{"airflow",7.5},{"dvc",8.5},{"fastapi",7.5},{"flask",6.5},
        {"django",6.5},{"pytorch",7.0},{"tensorflow",6.8},{"xgboost",7.5},
        {"langchain",8.5},{"faiss",8.5},{"llm",7.5},{"rag",8.0},
        {"golang",7.0},{"rust",7.5},{"scala",7.5},{"typescript",6.5},
        {"snowflake",7.8},{"databricks",8.0},{"dbt",8.5},{"redshift",7.5},
        // Medium-IDF terms
        {"python",5.0},{"java",5.0},{"javascript",5.0},{"sql",4.5},
        {"docker",5.5},{"aws",5.5},{"gcp",5.5},{"azure",5.5},{"git",4.5},
        {"postgresql",6.0},{"mysql",5.5},{"mongodb",6.0},{"redis",6.0},
        {"react",5.5},{"nodejs",5.5},{"linux",5.0},{"bash",5.5},
        {"numpy",5.5},{"pandas",5.5},{"scikit",6.0},{"opencv",6.5},
        {"microservices",6.5},{"restful",5.5},{"api",4.0},{"cicd",6.0},
        // Soft skills / roles (lower IDF - common)
        {"engineer",3.5},{"developer",3.5},{"manager",3.5},{"analyst",3.5},
        {"senior",3.0},{"junior",3.0},{"lead",3.5},{"architect",4.0},
        {"experience",2.5},{"skills",2.0},{"knowledge",2.5},{"team",2.5},
        {"strong",2.0},{"excellent",2.0},{"good",1.5},{"work",2.0},
        {"software",3.0},{"system",3.0},{"data",3.5},{"machine",4.0},
        {"learning",4.0},{"deep",4.0},{"backend",5.0},{"frontend",5.0},
        {"fullstack",5.5},{"distributed",5.5},{"scalable",5.5},
    };
    return idf;
}

TFIDFScorer::TFIDFScorer() : idf_(build_default_idf()) {}

double TFIDFScorer::idf_for(const std::string& term) const {
    auto it = idf_.find(term);
    return (it != idf_.end()) ? it->second : 2.0; // default IDF for unknown terms
}

double TFIDFScorer::cosine_similarity(const std::string& doc_text,
                                       const std::string& query_text) const
{
    TextTokens doc   = TextTokens::build(doc_text);
    TextTokens query = TextTokens::build(query_text);

    if (doc.total_tokens == 0 || query.total_tokens == 0) return 0.0;

    // Build TF-IDF vectors for terms in query
    double dot = 0.0, norm_d = 0.0, norm_q = 0.0;

    for (const auto& term : query.unique_terms) {
        double idf = idf_for(term);
        double q_tfidf = static_cast<double>(query.term_freq.at(term)) /
                         static_cast<double>(query.total_tokens) * idf;
        auto d_it = doc.term_freq.find(term);
        double d_tfidf = 0.0;
        if (d_it != doc.term_freq.end()) {
            d_tfidf = static_cast<double>(d_it->second) /
                      static_cast<double>(doc.total_tokens) * idf;
        }
        dot    += q_tfidf * d_tfidf;
        norm_q += q_tfidf * q_tfidf;
        norm_d += d_tfidf * d_tfidf;
    }
    // Also add doc-only terms to norm_d
    for (const auto& term : doc.unique_terms) {
        if (!query.unique_terms.count(term)) {
            double idf = idf_for(term);
            double d_tfidf = static_cast<double>(doc.term_freq.at(term)) /
                             static_cast<double>(doc.total_tokens) * idf;
            norm_d += d_tfidf * d_tfidf;
        }
    }

    double denom = std::sqrt(norm_q) * std::sqrt(norm_d);
    return (denom > 0) ? std::min(1.0, dot / denom) : 0.0;
}

// ---------------------------------------------------------------------------
// RetrievalFeatureExtractor
// ---------------------------------------------------------------------------
RetrievalFeatureExtractor::RetrievalFeatureExtractor() {}

double RetrievalFeatureExtractor::cosine_embedding(const std::vector<float>& a,
                                                    const std::vector<float>& b)
{
    if (a.empty() || b.empty() || a.size() != b.size()) return 0.0;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    double denom = std::sqrt(na) * std::sqrt(nb);
    return (denom > 0) ? std::max(0.0, std::min(1.0, dot / denom)) : 0.0;
}

double RetrievalFeatureExtractor::keyword_overlap(const TextTokens& a, const TextTokens& b) {
    if (b.unique_terms.empty()) return 0.0;
    int overlap = 0;
    for (const auto& t : b.unique_terms) {
        if (a.unique_terms.count(t)) ++overlap;
    }
    return static_cast<double>(overlap) / static_cast<double>(b.unique_terms.size());
}

double RetrievalFeatureExtractor::bigram_overlap(const std::vector<std::string>& a,
                                                  const std::vector<std::string>& b)
{
    if (a.size() < 2 || b.size() < 2) return 0.0;
    std::unordered_set<std::string> a_bigrams, b_bigrams;
    for (size_t i = 0; i + 1 < a.size(); ++i) a_bigrams.insert(a[i] + " " + a[i+1]);
    for (size_t i = 0; i + 1 < b.size(); ++i) b_bigrams.insert(b[i] + " " + b[i+1]);
    int overlap = 0;
    for (const auto& bg : b_bigrams) {
        if (a_bigrams.count(bg)) ++overlap;
    }
    return static_cast<double>(overlap) / static_cast<double>(b_bigrams.size());
}

double RetrievalFeatureExtractor::term_coverage(const TextTokens& query,
                                                  const TextTokens& doc)
{
    if (query.unique_terms.empty()) return 1.0;
    int covered = 0;
    for (const auto& t : query.unique_terms) {
        if (doc.unique_terms.count(t)) ++covered;
    }
    return static_cast<double>(covered) / static_cast<double>(query.unique_terms.size());
}

static const std::unordered_set<std::string> TECH_TERMS = {
    "python","java","javascript","typescript","golang","rust","scala","cpp","csharp",
    "react","angular","vue","nodejs","flask","django","fastapi","spring",
    "aws","gcp","azure","docker","kubernetes","terraform","ansible",
    "sql","mysql","postgresql","mongodb","redis","elasticsearch","cassandra","kafka",
    "pytorch","tensorflow","sklearn","numpy","pandas","spark","hadoop",
    "git","linux","bash","rest","api","graphql","grpc","microservices","cicd"
};

int RetrievalFeatureExtractor::count_tech_terms(const TextTokens& t) {
    int count = 0;
    for (const auto& term : t.unique_terms) {
        if (TECH_TERMS.count(term)) ++count;
    }
    return count;
}

double RetrievalFeatureExtractor::tf_weighted_keyword_score(const TextTokens& resume,
                                                              const TextTokens& jd)
{
    if (jd.total_tokens == 0 || resume.total_tokens == 0) return 0.0;
    double score = 0.0, max_score = 0.0;
    for (const auto& [term, q_tf] : jd.term_freq) {
        double weight = static_cast<double>(q_tf) / static_cast<double>(jd.total_tokens);
        max_score += weight;
        auto it = resume.term_freq.find(term);
        if (it != resume.term_freq.end()) {
            double r_tf = static_cast<double>(it->second) / static_cast<double>(resume.total_tokens);
            score += weight * std::min(1.0, r_tf / (weight + 0.001));
        }
    }
    return (max_score > 0) ? std::min(1.0, score / max_score) : 0.0;
}

void RetrievalFeatureExtractor::extract(
    const std::string& resume_text,
    const std::string& jd_text,
    const std::vector<float>& resume_emb,
    const std::vector<float>& jd_emb,
    FeatureVector& fv) const
{
    TextTokens resume_tok = TextTokens::build(resume_text);
    TextTokens jd_tok     = TextTokens::build(jd_text);

    // BM25
    double bm25_raw = bm25_.score(resume_text, jd_text);
    double bm25_norm = BM25Scorer::normalize(bm25_raw);

    // TF-IDF cosine
    double tfidf_cos = tfidf_.cosine_similarity(resume_text, jd_text);

    // Embedding cosine
    double emb_cos = cosine_embedding(resume_emb, jd_emb);

    // Keyword overlap (JD terms found in resume)
    double kw_overlap = keyword_overlap(resume_tok, jd_tok);
    int kw_count = 0;
    for (const auto& t : jd_tok.unique_terms)
        if (resume_tok.unique_terms.count(t)) ++kw_count;

    // Term coverage
    double jd_coverage = term_coverage(jd_tok, resume_tok);

    // Length ratio
    double len_ratio = (jd_tok.total_tokens > 0)
        ? std::min(1.0, static_cast<double>(resume_tok.total_tokens) /
                        static_cast<double>(jd_tok.total_tokens) / 3.0)
        : 0.5;

    // Bigram overlap
    double bigram = bigram_overlap(resume_tok.tokens, jd_tok.tokens);

    // Tech terms
    int resume_tech = count_tech_terms(resume_tok);
    int jd_tech     = count_tech_terms(jd_tok);

    // TF-weighted keyword score
    double tf_kw = tf_weighted_keyword_score(resume_tok, jd_tok);

    fv.set("bm25_score",              bm25_norm);
    fv.set("tfidf_cosine_similarity", tfidf_cos);
    fv.set("keyword_overlap_count",   std::min(1.0, static_cast<double>(kw_count) / 30.0));
    fv.set("keyword_overlap_ratio",   kw_overlap);
    fv.set("cosine_similarity",       emb_cos);
    fv.set("jd_term_coverage",        jd_coverage);
    fv.set("resume_jd_length_ratio",  len_ratio);
    fv.set("bigram_overlap_ratio",    bigram);
    fv.set("title_match_score",       0.5); // placeholder; engine.cpp fills from JD title parse
    fv.set("summary_jd_similarity",   0.5); // placeholder; engine.cpp can refine
    fv.set("jd_unique_term_count",    std::min(1.0, static_cast<double>(jd_tok.unique_terms.size()) / 200.0));
    fv.set("resume_unique_term_count",std::min(1.0, static_cast<double>(resume_tok.unique_terms.size()) / 400.0));
    fv.set("jd_tech_term_count",      std::min(1.0, static_cast<double>(jd_tech) / 20.0));
    fv.set("resume_tech_term_count",  std::min(1.0, static_cast<double>(resume_tech) / 30.0));
    fv.set("tf_weighted_keyword_score", tf_kw);
}

} // namespace talentmatch
