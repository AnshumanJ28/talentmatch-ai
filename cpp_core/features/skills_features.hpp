#pragma once
#include "../include/features.hpp"
#include "../include/resume.hpp"
#include "../skills/skill_engine.hpp"
#include <string>
#include <vector>

namespace talentmatch {

// ---------------------------------------------------------------------------
// SkillsFeatureExtractor  — 20 skill-gap features
// ---------------------------------------------------------------------------
class SkillsFeatureExtractor {
public:
    static void extract(const SkillExtractionResult& candidate,
                        const SkillExtractionResult& jd_skills,
                        const SkillGapResult& gap,
                        FeatureVector& fv);
};

} // namespace talentmatch
