#include "seed/render/RenderSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/render/IRenderer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

namespace seed {

bool RenderSystem::render(Scene& scene, IRenderer& renderer) {
    TransformComponent* camera_transform = nullptr;
    CameraComponent* camera = nullptr;

    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId, TransformComponent& transform, CameraComponent& candidate) {
            if (camera != nullptr || !candidate.enabled || !candidate.primary) {
                return;
            }
            camera_transform = &transform;
            camera = &candidate;
        }
    );

    if (camera == nullptr || camera_transform == nullptr) {
        return false;
    }

    const float aspect = static_cast<float>(renderer.width()) /
        static_cast<float>(renderer.height() > 0 ? renderer.height() : 1);

    const Vec3 forward = forward_from_euler(camera_transform->rotation_degrees);
    const Mat4 view = look_at_matrix(
        camera_transform->position,
        camera_transform->position + forward,
        {0.0f, 1.0f, 0.0f}
    );
    const Mat4 projection = perspective_matrix(
        camera->field_of_view_degrees,
        aspect,
        camera->near_plane,
        camera->far_plane
    );
    const Mat4 view_projection = projection * view;

    scene.for_each<TransformComponent, MeshComponent, MaterialComponent>(
        [&](EntityId,
            TransformComponent& transform,
            MeshComponent& mesh,
            MaterialComponent& material) {
            if (!mesh.visible || !mesh.mesh || !material.shader) {
                return;
            }

            const Mat4 model = transform_matrix(
                transform.position,
                transform.rotation_degrees,
                transform.scale
            );
            renderer.draw_mesh(mesh.mesh, material.shader, model, view_projection);
        }
    );

    return true;
}

} // namespace seed
