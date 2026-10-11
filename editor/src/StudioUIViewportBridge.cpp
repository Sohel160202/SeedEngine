#include "StudioUI.h"
#include "SeedDoctorPanel.h"

#include "seed/render/RenderComponents.h"
#include "seed/gameplay/Components.h"

namespace seed::studio {

void StudioUI::draw(Scene& scene, const StudioDocumentInfo& document) {
    EntityId viewport_camera = InvalidEntity;

    if (!document.playing) {
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
                if (viewport_camera == InvalidEntity && camera.enabled && camera.editor_only) {
                    viewport_camera = entity;
                }
            }
        );
    } else {
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
                if (viewport_camera == InvalidEntity && camera.enabled && camera.primary && !camera.editor_only) {
                    viewport_camera = entity;
                }
            }
        );
    }

    if (viewport_camera == InvalidEntity) {
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
                if (viewport_camera == InvalidEntity && camera.enabled) viewport_camera = entity;
            }
        );
    }

    draw(scene, document, viewport_camera);

    // Seed Doctor is an authoring tool, not a runtime HUD. Keep its presentation
    // entirely in Studio while the validation rules remain engine-owned.
    static SeedDoctorPanel doctor_panel;
    const SeedDoctorPanelResult doctor_result = doctor_panel.draw(scene, *this, !document.playing);
    if (doctor_result.scene_changed) {
        m_scene_edited = true;
    }
}

} // namespace seed::studio
