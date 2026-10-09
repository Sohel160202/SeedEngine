#include "seed/render/RenderSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/render/IRenderer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>

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
                if (camera != nullptr || !candidate.enabled || !candidate.primary) return;
                camera_transform = &transform;
                camera = &candidate;
            }
        );
    }

    if (camera == nullptr || camera_transform == nullptr) return false;

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
    lighting.camera_position = camera_transform->position;

    DirectionalLightComponent* directional = nullptr;
    TransformComponent* directional_transform = nullptr;
    scene.for_each<TransformComponent, DirectionalLightComponent>(
        [&](EntityId, TransformComponent& transform, DirectionalLightComponent& light) {
            if (directional != nullptr || !light.enabled) return;
            directional = &light;
            directional_transform = &transform;
            lighting.directional_direction = forward_from_euler(transform.rotation_degrees);
            lighting.directional_color = light.color;
            lighting.directional_intensity = light.intensity;
        }
    );

    bool found_ambient = false;
    scene.for_each<AmbientLightComponent>(
        [&](EntityId, AmbientLightComponent& light) {
            if (found_ambient || !light.enabled) return;
            lighting.ambient_color = light.color;
            lighting.ambient_intensity = light.intensity;
            found_ambient = true;
        }
    );

    SkySettings sky;
    scene.for_each<SkyComponent>([&](EntityId, SkyComponent& component) {
        if (sky.enabled || !component.enabled) return;
        sky.zenith_color = component.zenith_color;
        sky.horizon_color = component.horizon_color;
        sky.intensity = component.intensity;
        sky.enabled = true;

        // Environment Lighting v0 intentionally uses the exact same authored
        // Sky as its source. There is no separate probe or duplicated setup for
        // beginners: change the Sky and PBR materials react to that world.
        lighting.environment_zenith_color = component.zenith_color;
        lighting.environment_horizon_color = component.horizon_color;
        lighting.environment_intensity = std::max(component.intensity, 0.0f);
        lighting.environment_enabled = true;
    });
    renderer.draw_sky(sky);

    if (directional != nullptr && directional_transform != nullptr &&
        directional->casts_shadows && directional->intensity > 0.0f) {
        const Vec3 light_direction = normalize(lighting.directional_direction);
        const float distance = std::max(directional->shadow_distance, 5.0f);
        const float extent = std::max(8.0f, distance * 0.60f);
        const Vec3 center = camera_transform->position + forward * std::min(distance * 0.25f, 8.0f);
        const Vec3 light_position = center - light_direction * distance;
        const Vec3 up = std::fabs(dot(light_direction, Vec3{0.0f, 1.0f, 0.0f})) > 0.96f
            ? Vec3{0.0f, 0.0f, 1.0f}
            : Vec3{0.0f, 1.0f, 0.0f};
        const Mat4 light_view = look_at_matrix(light_position, center, up);
        const Mat4 light_projection = orthographic_matrix(
            -extent, extent, -extent, extent, 0.5f, distance * 2.5f
        );
        lighting.light_view_projection = light_projection * light_view;

        if (renderer.begin_shadow_pass(lighting.light_view_projection)) {
            scene.for_each<TransformComponent, MeshComponent>(
                [&](EntityId, TransformComponent& transform, MeshComponent& mesh) {
                    if (!mesh.visible || !mesh.cast_shadows || !mesh.mesh) return;
                    renderer.draw_shadow_mesh(
                        mesh.mesh,
                        transform_matrix(transform.position, transform.rotation_degrees, transform.scale)
                    );
                }
            );
            renderer.end_shadow_pass();
            lighting.shadows_enabled = true;
        }
    }

    scene.for_each<TransformComponent, MeshComponent, MaterialComponent>(
        [&](EntityId, TransformComponent& transform, MeshComponent& mesh, MaterialComponent& material) {
            if (!mesh.visible || !mesh.mesh || !material.shader) return;
            const Mat4 model = transform_matrix(
                transform.position,
                transform.rotation_degrees,
                transform.scale
            );
            renderer.draw_mesh(
                mesh.mesh,
                material.shader,
                MaterialTextures{
                    .base_color = material.use_base_color_texture ? material.base_color_texture : TextureHandle{},
                    .metallic_roughness = material.use_metallic_roughness_texture ? material.metallic_roughness_texture : TextureHandle{},
                    .normal = material.use_normal_texture ? material.normal_texture : TextureHandle{},
                },
                material.base_color,
                MaterialSurface{
                    .metallic = std::clamp(material.metallic, 0.0f, 1.0f),
                    .roughness = std::clamp(material.roughness, 0.04f, 1.0f),
                    .normal_scale = std::clamp(material.normal_scale, 0.0f, 4.0f),
                },
                mesh.receive_shadows,
                model,
                view_projection,
                lighting
            );
        }
    );

    return true;
}

} // namespace seed
