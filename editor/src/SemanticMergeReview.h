#pragma once

#include "seed/scene/SceneMergeResolution.h"

#include <cstddef>
#include <vector>

namespace seed::studio {

enum class SemanticMergeReviewDecision {
    None,
    Apply,
    Cancel,
};

struct SemanticMergeReviewResult {
    SemanticMergeReviewDecision decision{SemanticMergeReviewDecision::None};
    Scene merged_scene;
};

class SemanticMergeReview {
public:
    void open(const Scene& base_scene, const Scene& ours_scene, const Scene& theirs_scene);
    void reset();
    bool active() const noexcept { return m_open; }
    SemanticMergeReviewResult draw();

private:
    bool m_open{false};
    bool m_request_open_popup{false};
    Scene m_base_scene;
    Scene m_ours_scene;
    Scene m_theirs_scene;
    SceneMergeResult m_merge_result;
    SceneMergeResolutionPlan m_plan;
};

} // namespace seed::studio
