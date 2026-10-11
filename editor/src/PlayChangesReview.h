#pragma once

#include "seed/scene/SceneDiff.h"

#include <cstddef>
#include <vector>

namespace seed::studio {

enum class PlayChangesDecision {
    None,
    ApplySelected,
    Discard,
};

struct PlayChangesReviewResult {
    PlayChangesDecision decision{PlayChangesDecision::None};
    std::vector<std::size_t> selected_change_indices;
};

// Beginner-facing review surface for semantic Play Mode changes. Selection is
// intentionally conservative: Safe changes start selected, Review changes are
// opt-in, and Runtime State cannot be selected from the normal review workflow.
class PlayChangesReview {
public:
    void open(const SceneDiff& diff);
    void reset();

    bool is_open() const noexcept { return m_open; }
    PlayChangesReviewResult draw(const SceneDiff& diff);

private:
    void ensure_selection_matches(const SceneDiff& diff);

    bool m_open{false};
    bool m_request_open_popup{false};
    std::vector<bool> m_selected;
};

} // namespace seed::studio
