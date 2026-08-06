#pragma once

/* ============================================================================
 * engine.h  —  TalentMatch Core C-linkage API
 *
 * This is the ONLY interface exposed to Python (via ctypes).
 * All other C++ types are internal to the shared library.
 *
 * Platform export macros:
 *   Windows  → __declspec(dllexport / dllimport)
 *   Linux    → __attribute__((visibility("default")))
 * ============================================================================ */

#ifdef _WIN32
#  if defined(TALENTMATCH_EXPORTS) || defined(talentmatch_EXPORTS)
#    define TM_API __declspec(dllexport)
#  else
#    define TM_API __declspec(dllimport)
#  endif
#else
#  define TM_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * engine_score
 *
 * Main entry point. Accepts a UTF-8 JSON string, performs all ML computation,
 * and returns a UTF-8 JSON string.
 *
 * Input JSON schema:
 * {
 *   "resume_text":       string,          // plain text extracted from PDF
 *   "jd_text":           string,          // job description text
 *   "resume_embedding":  [float, ...],    // 384-dim unit vector from Python
 *   "jd_embedding":      [float, ...],    // 384-dim unit vector from Python
 *   "taxonomy_path":     string           // optional path to skill_taxonomy.json
 * }
 *
 * Output JSON schema:
 * {
 *   "overall_score":        float,        // 0–100
 *   "scores": {
 *     "skills":             float,        // 0–100
 *     "experience":         float,        // 0–100
 *     "education":          float,        // 0–100
 *     "projects":           float,        // 0–100
 *     "semantic":           float,        // 0–100
 *     "resume_quality":     float         // 0–100
 *   },
 *   "matched_skills":       [string],
 *   "missing_skills":       [string],
 *   "partial_skills":       [{"jd_skill": string, "matched_as": string, "confidence": float}],
 *   "top_positive_factors": [string],
 *   "top_negative_factors": [string],
 *   "feature_vector":       {string: float},  // all ~120 features (debug)
 *   "error":                string | null
 * }
 *
 * Returns: heap-allocated C string owned by the caller. Call engine_free() when done.
 * Never returns NULL — errors appear as {"error": "..."} JSON.
 */
TM_API const char* engine_score(const char* request_json);

/**
 * engine_free
 *
 * Releases memory allocated by engine_score(). Must be called exactly once
 * per returned pointer. Passing NULL is a no-op.
 */
TM_API void engine_free(const char* ptr);

/**
 * engine_version
 *
 * Returns a static string (do NOT free) with the engine semantic version.
 * Format: "major.minor.patch"
 */
TM_API const char* engine_version();

#ifdef __cplusplus
}
#endif
