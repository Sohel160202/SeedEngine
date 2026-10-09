#include "seed/render/RenderSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/render/IRenderer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

namespace seed {

bool RenderSystem::render(Scene& scene, IRenderer& renderer, EntityId camera_entity) {
    TransformComponent* camera_transform = nullptr;
    CameraComponent* camera = nullptr;

    if (camera_entity != InvalidEntity && scene.is_alive(camera_entity)) {
        camera_transform = scene.get_component<TransformComponent>(camera_entity);
        camera = scene.get_component<CameraComponent>(camera_entity);
        if (camera != nullptr && !camera->enabled) {
            camera = nullptr;
            camera_transform = nullptr;
        }
    }

    if (camera == nullptr || camera_transform == nullptr) {
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId, TransformComponent& transform, CameraComponent& candidate) {
                if (camera != nullptr || !candidate.enabled || !candidate.primary) {
                    return;
                }
                camera_transform = &transform;
                camera = &candidate;
            }
        );
    }

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

    SceneLighting lighting;
    bool found_directional = false;
    scene.for_each<TransformComponent, DirectionalLightComponent>(
        [&](EntityId, TransformComponent& transform, DirectionalLightComponent& light) {
            if (found_directional || !light.enabled) {
                return;
            }
            lighting.directional_direction = forward_from_euler(transform.rotation_degrees);
            lighting.directional_color = light.color;
            lighting.directional_intensity = light.intensity;
            found_directional = true;
        }
    );

    bool found_ambient = false;
    scene.for_each<AmbientLightComponent>(
        [&](EntityId, AmbientLightComponent& light) {
            if (found_ambient || !light.enabled) {
                return;
            }
            lighting.ambient_color = light.color;
            lighting.ambient_intensity = light.intensity;
            found_ambient = true;
        }
    );

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
            renderer.draw_mesh(
                mesh.mesh,
                material.shader,
                material.base_color_texture,
                material.base_color,
                model,
                view_projection,
                lighting
            );
        }
    );

    return true;
}

} // namespace seed
