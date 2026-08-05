#pragma once
#include "../include/features.hpp"
#include <string>
#include <vector>

namespace talentmatch {

class CategoryScoreComputer {
public:
    static CategoryScores compute(const FeatureVector& fv);
};

// ---------------------------------------------------------------------------
// XGBoostRanker
//
// Loads models/xgboost_model.json (if it exists) and uses the XGBoost C API
// to predict a relevance probability from the feature vector.
//
// Fallback: if the model file is absent or the XGBoost library is not
// available, uses a deterministic weighted linear combination of category
// scores instead. The fallback is transparent and labelled in the response.
// ---------------------------------------------------------------------------
class XGBoostRanker {
public:
    explicit XGBoostRanker(const std::string& model_path = "models/xgboost_model.json");
    ~XGBoostRanker();

    // Returns probability ∈ [0.0, 1.0]
    double predict(const FeatureVector& fv) const;

    bool is_model_loaded() const { return model_loaded_; }

private:
    bool        model_loaded_{false};
    std::string model_path_;
    void*       booster_{nullptr}; // XGBoosterHandle — opaque to avoid xgboost.h dependency here

    // Fallback linear scorer
    static double linear_fallback(const FeatureVector& fv);

    // Try to load XGBoost dynamically
    bool try_load_xgboost();
    double xgb_predict(const FeatureVector& fv) const;
};

} // namespace talentmatch
