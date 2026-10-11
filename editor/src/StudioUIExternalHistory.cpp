#include "StudioUI.h"

namespace seed::studio {
namespace {

StudioUI* g_active_studio_ui = nullptr;
constexpr std::size_t ExternalHistoryLimit = 64;

} // namespace

StudioUI::StudioUI() {
    g_active_studio_ui = this;
}

StudioUI* StudioUI::active_instance() noexcept {
    return g_active_studio_ui;
}

void StudioUI::begin_external_history_transaction() {
    if (!m_has_history_baseline || m_history_transaction_active) return;

    m_undo_stack.push_back(m_history_baseline);
    if (m_undo_stack.size() > ExternalHistoryLimit) {
        m_undo_stack.erase(m_undo_stack.begin());
    }

    m_redo_stack.clear();

    // The external edit happens after StudioUI::draw() in the same frame. Keep
    // the history transaction open until the next non-active frame so the normal
    // finish_history_frame() path adopts the post-apply scene as the new
    // baseline without creating a second Undo entry.
    m_history_transaction_active = true;
}

} // namespace seed::studio
