"""
scripts/train_model.py  —  XGBoost model training script for TalentMatch v2.

This script:
1. Generates synthetic resume-JD feature pairs with realistic distributions
2. Trains an XGBoost binary classifier (relevant / not relevant)
3. Saves the model to models/xgboost_model.json

The synthetic data is parameterized to approximate real-world distributions:
  - High-match resumes: high skill coverage, relevant experience, good education
  - Low-match resumes:  low skill coverage, irrelevant experience
  - Medium resumes: mixed signals

Usage:
    pip install xgboost scikit-learn numpy
    python scripts/train_model.py

    # With custom output path:
    python scripts/train_model.py --output models/my_model.json --samples 5000

IMPORTANT: Replace synthetic data with real labeled data for production.
"""

import argparse
import json
import os
import sys
from pathlib import Path

import numpy as np

# Feature names must match FEATURE_NAMES in cpp_core/include/features.hpp
FEATURE_NAMES = [
    # Experience (15)
    "total_experience_months", "relevant_experience_months", "num_positions",
    "avg_tenure_months", "max_tenure_months", "employment_gap_months", "num_gaps",
    "is_currently_employed", "max_seniority_score", "avg_seniority_score",
    "has_leadership_keywords", "num_promotions", "quantified_achievement_count",
    "action_verb_density", "career_progression_score",
    # Education (10)
    "num_degrees", "highest_degree_level", "degree_field_match", "gpa_normalized",
    "has_gpa", "is_currently_studying", "num_institutions", "education_recency_score",
    "prestigious_institution_flag", "degree_requirement_met",
    # Projects (8)
    "num_projects", "avg_technologies_per_project", "project_link_ratio",
    "project_tech_overlap", "project_domain_relevance", "has_open_source",
    "avg_project_desc_length", "project_quantification_score",
    # Certifications (7)
    "num_certifications", "num_unique_issuers", "has_certifications",
    "jd_cert_match", "cert_recency_score", "has_cloud_cert", "has_security_cert",
    # Resume Quality (10)
    "section_completeness_score", "action_verb_density_global",
    "quantified_achievements_total", "avg_bullet_length", "total_word_count",
    "resume_length_score", "formatting_consistency", "has_summary",
    "has_links", "has_contact_info",
    # Skills (20)
    "skill_coverage_ratio", "critical_skill_coverage", "optional_skill_coverage",
    "num_matched_skills", "num_missing_skills", "num_partial_skills",
    "rare_skill_bonus", "skill_category_diversity", "avg_skill_confidence",
    "high_confidence_skill_ratio", "total_canonical_skills", "unmatched_skill_ratio",
    "primary_domain_confidence", "skills_text_length", "jd_skill_count",
    "jd_critical_skill_count", "skill_exact_match_count", "skill_fuzzy_match_count",
    "skill_missing_critical_count", "skill_has_all_required",
    # Retrieval (15)
    "bm25_score", "tfidf_cosine_similarity", "keyword_overlap_count",
    "keyword_overlap_ratio", "cosine_similarity", "jd_term_coverage",
    "resume_jd_length_ratio", "bigram_overlap_ratio", "title_match_score",
    "summary_jd_similarity", "jd_unique_term_count", "resume_unique_term_count",
    "jd_tech_term_count", "resume_tech_term_count", "tf_weighted_keyword_score",
]

N_FEATURES = len(FEATURE_NAMES)


def generate_synthetic_data(n_samples: int = 3000, seed: int = 42) -> tuple:
    """
    Generates synthetic (feature_vector, label) pairs.

    Label = 1 if the resume is relevant to the JD, 0 otherwise.
    Positive samples: 40% of total (mirrors typical ATS pass rate).
    """
    rng = np.random.default_rng(seed)
    X = np.zeros((n_samples, N_FEATURES), dtype=np.float32)
    y = np.zeros(n_samples, dtype=np.int32)

    n_positive = int(n_samples * 0.40)
    fi = {name: i for i, name in enumerate(FEATURE_NAMES)}

    def set_feat(i, name, val):
        X[i, fi[name]] = np.clip(float(val), 0.0, 1.0)

    for i in range(n_samples):
        is_positive = (i < n_positive)
        y[i] = int(is_positive)

        if is_positive:
            # High-match resume: good skills, experience, semantics
            sc = rng.beta(5, 1.5)        # ~0.75 mean
            exp = rng.beta(4, 2)          # ~0.67 mean
            sem = rng.beta(4.5, 1.5)      # ~0.75 mean
        else:
            # Low-match resume: poor alignment
            sc = rng.beta(1.5, 4)         # ~0.27 mean
            exp = rng.beta(2, 4)          # ~0.33 mean
            sem = rng.beta(1.5, 4)        # ~0.27 mean

        noise = lambda s=0.05: rng.normal(0, s)

        # Skills
        set_feat(i, "skill_coverage_ratio",     sc + noise())
        set_feat(i, "critical_skill_coverage",  sc * 0.9 + noise())
        set_feat(i, "optional_skill_coverage",  sc * 1.1 + noise())
        set_feat(i, "num_matched_skills",       sc + noise())
        set_feat(i, "num_missing_skills",       1 - sc + noise())
        set_feat(i, "num_partial_skills",       rng.beta(2, 3) if is_positive else rng.beta(1, 4))
        set_feat(i, "rare_skill_bonus",         rng.beta(2, 5))
        set_feat(i, "skill_category_diversity", sc * 0.8 + noise())
        set_feat(i, "avg_skill_confidence",     sc * 0.95 + noise(0.03))
        set_feat(i, "high_confidence_skill_ratio", sc * 0.9 + noise())
        set_feat(i, "total_canonical_skills",   rng.beta(3, 2))
        set_feat(i, "unmatched_skill_ratio",    sc + noise(0.03))
        set_feat(i, "primary_domain_confidence",sc * 0.85 + noise())
        set_feat(i, "skills_text_length",       rng.beta(3, 2))
        set_feat(i, "jd_skill_count",           rng.beta(2, 2))
        set_feat(i, "jd_critical_skill_count",  rng.beta(2, 3))
        set_feat(i, "skill_exact_match_count",  sc + noise())
        set_feat(i, "skill_fuzzy_match_count",  rng.beta(2, 5))
        set_feat(i, "skill_missing_critical_count", 1 - sc * 0.8 + noise())
        set_feat(i, "skill_has_all_required",   sc + noise(0.03))

        # Experience
        set_feat(i, "total_experience_months",      exp + noise())
        set_feat(i, "relevant_experience_months",   exp * 0.9 + noise())
        set_feat(i, "num_positions",                rng.beta(3, 3))
        set_feat(i, "avg_tenure_months",            rng.beta(2, 2))
        set_feat(i, "max_tenure_months",            rng.beta(3, 2))
        set_feat(i, "employment_gap_months",        rng.beta(3, 2) if is_positive else rng.beta(2, 3))
        set_feat(i, "num_gaps",                     rng.beta(3, 2) if is_positive else rng.beta(2, 4))
        set_feat(i, "is_currently_employed",        float(rng.random() > (0.2 if is_positive else 0.5)))
        set_feat(i, "max_seniority_score",          exp * 0.85 + noise())
        set_feat(i, "avg_seniority_score",          exp * 0.8 + noise())
        set_feat(i, "has_leadership_keywords",      float(rng.random() > (0.3 if is_positive else 0.7)))
        set_feat(i, "num_promotions",               rng.beta(2, 5))
        set_feat(i, "quantified_achievement_count", exp * 0.9 + noise())
        set_feat(i, "action_verb_density",          rng.beta(3, 2))
        set_feat(i, "career_progression_score",     exp * 0.85 + noise())

        # Education
        edu = rng.beta(3, 2) if is_positive else rng.beta(2, 3)
        set_feat(i, "num_degrees",              rng.beta(2, 2))
        set_feat(i, "highest_degree_level",     edu + noise())
        set_feat(i, "degree_field_match",       edu * 0.9 + noise())
        set_feat(i, "gpa_normalized",           rng.beta(3, 2))
        set_feat(i, "has_gpa",                  float(rng.random() > 0.5))
        set_feat(i, "is_currently_studying",    float(rng.random() > 0.8))
        set_feat(i, "num_institutions",         rng.beta(2, 4))
        set_feat(i, "education_recency_score",  rng.beta(3, 2))
        set_feat(i, "prestigious_institution_flag", float(rng.random() > 0.8))
        set_feat(i, "degree_requirement_met",   edu + noise())

        # Projects
        proj = rng.beta(3, 2) if is_positive else rng.beta(2, 4)
        set_feat(i, "num_projects",               rng.beta(2, 3))
        set_feat(i, "avg_technologies_per_project",rng.beta(2, 2))
        set_feat(i, "project_link_ratio",         rng.beta(3, 3))
        set_feat(i, "project_tech_overlap",       proj + noise())
        set_feat(i, "project_domain_relevance",   proj * 0.9 + noise())
        set_feat(i, "has_open_source",            float(rng.random() > (0.4 if is_positive else 0.8)))
        set_feat(i, "avg_project_desc_length",    rng.beta(2, 2))
        set_feat(i, "project_quantification_score",rng.beta(2, 3))

        # Certifications
        set_feat(i, "num_certifications",  rng.beta(2, 4))
        set_feat(i, "num_unique_issuers",  rng.beta(2, 4))
        set_feat(i, "has_certifications",  float(rng.random() > 0.5))
        set_feat(i, "jd_cert_match",       float(rng.random() > (0.5 if is_positive else 0.9)))
        set_feat(i, "cert_recency_score",  rng.beta(3, 2))
        set_feat(i, "has_cloud_cert",      float(rng.random() > 0.7))
        set_feat(i, "has_security_cert",   float(rng.random() > 0.8))

        # Resume quality
        set_feat(i, "section_completeness_score",   rng.beta(3, 2))
        set_feat(i, "action_verb_density_global",   rng.beta(3, 2))
        set_feat(i, "quantified_achievements_total",rng.beta(2, 3))
        set_feat(i, "avg_bullet_length",            rng.beta(3, 2))
        set_feat(i, "total_word_count",             rng.beta(3, 2))
        set_feat(i, "resume_length_score",          rng.beta(4, 2))
        set_feat(i, "formatting_consistency",       rng.beta(4, 1.5))
        set_feat(i, "has_summary",                  float(rng.random() > (0.3 if is_positive else 0.6)))
        set_feat(i, "has_links",                    float(rng.random() > 0.5))
        set_feat(i, "has_contact_info",             float(rng.random() > 0.1))

        # Retrieval
        set_feat(i, "bm25_score",              sem + noise())
        set_feat(i, "tfidf_cosine_similarity", sem * 0.95 + noise())
        set_feat(i, "keyword_overlap_count",   sem * 0.9 + noise())
        set_feat(i, "keyword_overlap_ratio",   sem + noise())
        set_feat(i, "cosine_similarity",       sem + noise(0.03))
        set_feat(i, "jd_term_coverage",        sem * 0.85 + noise())
        set_feat(i, "resume_jd_length_ratio",  rng.beta(3, 2))
        set_feat(i, "bigram_overlap_ratio",    sem * 0.7 + noise())
        set_feat(i, "title_match_score",       sem * 0.8 + noise())
        set_feat(i, "summary_jd_similarity",   sem * 0.85 + noise())
        set_feat(i, "jd_unique_term_count",    rng.beta(2, 2))
        set_feat(i, "resume_unique_term_count",rng.beta(2, 2))
        set_feat(i, "jd_tech_term_count",      rng.beta(2, 3))
        set_feat(i, "resume_tech_term_count",  rng.beta(2, 2))
        set_feat(i, "tf_weighted_keyword_score",sem * 0.9 + noise())

    # Shuffle
    idx = rng.permutation(n_samples)
    return X[idx], y[idx]


def train(n_samples: int, output_path: Path) -> None:
    try:
        import xgboost as xgb
        from sklearn.model_selection import train_test_split
        from sklearn.metrics import roc_auc_score, classification_report
    except ImportError:
        print("ERROR: Missing dependencies. Install with:")
        print("  pip install xgboost scikit-learn")
        sys.exit(1)

    print(f"Generating {n_samples} synthetic training samples...")
    X, y = generate_synthetic_data(n_samples)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

    print(f"Training XGBoost on {len(X_train)} samples ({y_train.sum()} positive)...")
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        use_label_encoder=False,
        eval_metric="auc",
        random_state=42,
        verbosity=1,
    )
    model.fit(
        X_train, y_train,
        eval_set=[(X_test, y_test)],
        verbose=25,
    )

    # Evaluate
    y_prob = model.predict_proba(X_test)[:, 1]
    auc = roc_auc_score(y_test, y_prob)
    print(f"\nTest AUC: {auc:.4f}")

    # Feature importance
    fi = model.feature_importances_
    top_n = 10
    top_idx = fi.argsort()[-top_n:][::-1]
    print(f"\nTop {top_n} features:")
    for idx in top_idx:
        print(f"  {FEATURE_NAMES[idx]:40s}  {fi[idx]:.4f}")

    # Save
    output_path.parent.mkdir(parents=True, exist_ok=True)
    model.save_model(str(output_path))
    print(f"\nModel saved to: {output_path}")

    # Save feature names alongside model for C++ to use
    meta_path = output_path.with_suffix(".features.json")
    with open(meta_path, "w") as f:
        json.dump({"feature_names": FEATURE_NAMES, "n_features": N_FEATURES}, f, indent=2)
    print(f"Feature metadata saved to: {meta_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train XGBoost model for TalentMatch v2")
    parser.add_argument("--output", default="models/xgboost_model.json",
                        help="Output model path (default: models/xgboost_model.json)")
    parser.add_argument("--samples", type=int, default=3000,
                        help="Number of synthetic training samples (default: 3000)")
    args = parser.parse_args()
    train(args.samples, Path(args.output))
