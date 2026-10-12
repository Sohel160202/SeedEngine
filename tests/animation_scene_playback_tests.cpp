#include "seed/animation/AnimationComponents.h"
#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/gameplay/Components.h"
#include "seed/render/IRenderer.h"
#include "seed/render/RenderComponents.h"
#include "seed/render/RenderSystem.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

class CaptureRenderer final : public seed::IRenderer {
public:
    bool initialize(void*, const seed::RendererConfig&) override { return true; }
    void resize(std::uint32_t width, std::uint32_t height) override { m_width = width; m_height = height; }
    std::uint32_t width() const noexcept override { return m_width; }
    std::uint32_t height() const noexcept override { return m_height; }

    seed::ShaderHandle create_shader(const seed::ShaderDesc&) override { return {1}; }
    void destroy_shader(seed::ShaderHandle) override {}
    seed::MeshHandle create_mesh(const seed::MeshDesc&) override { return {1}; }
    seed::MeshHandle create_skinned_mesh(const seed::SkinnedMeshDesc&) override { return {2}; }
    void destroy_mesh(seed::MeshHandle) override {}
    seed::TextureHandle create_texture(const seed::TextureDesc&) override { return {1}; }
    void destroy_texture(seed::TextureHandle) override {}

    void begin_frame() override {}
    void draw_sky(const seed::SkySettings&) override {}
    bool begin_shadow_pass(const seed::Mat4&) override { return false; }
    void draw_shadow_mesh(seed::MeshHandle, const seed::Mat4&) override {}
    void end_shadow_pass() override {}
    void draw_mesh(
        seed::MeshHandle,
        seed::ShaderHandle,
        const seed::MaterialTextures&,
        const seed::Vec4&,
        const seed::MaterialSurface&,
        bool,
        const seed::Mat4&,
        const seed::Mat4&,
        const seed::SceneLighting&
    ) override {}

    void draw_skinned_mesh(
        seed::MeshHandle mesh,
        seed::ShaderHandle shader,
        const seed::MaterialTextures&,
        const seed::Vec4&,
        const seed::MaterialSurface&,
        bool,
        const seed::Mat4&,
        const seed::Mat4&,
        const seed::SceneLighting&,
        std::span<const seed::Mat4> joint_palette
    ) override {
        ++skinned_draw_calls;
        last_mesh = mesh;
        last_shader = shader;
        last_palette.assign(joint_palette.begin(), joint_palette.end());
    }

    void end_frame() override {}
    std::string_view backend_name() const override { return "Capture"; }
    void shutdown() override {}

    int skinned_draw_calls{0};
    seed::MeshHandle last_mesh{};
    seed::ShaderHandle last_shader{};
    std::vector<seed::Mat4> last_palette;

private:
    std::uint32_t m_width{1280};
    std::uint32_t m_height{720};
};

} // namespace

int main() {
    seed::AnimatedModelAssetResource character;
    character.mesh = {42};
    character.skeleton.name = "Test Skeleton";

    seed::SkeletonJoint root;
    root.name = "Root";
    root.parent = -1;
    root.bind_local.translation = {0.0f, 0.0f, 0.0f};
    root.inverse_bind_matrix = seed::Mat4::identity();
    character.skeleton.joints.push_back(root);

    seed::AnimationClip clip;
    clip.name = "Walk";
    clip.duration_seconds = 1.0f;
    clip.looping = true;
    seed::JointAnimationTrack track;
    track.joint_index = 0;
    track.translation_keys.push_back({.time_seconds = 0.0f, .value = {0.0f, 0.0f, 0.0f}});
    track.translation_keys.push_back({.time_seconds = 1.0f, .value = {1.0f, 0.0f, 0.0f}});
    clip.tracks.push_back(track);
    character.animations.push_back(clip);

    seed::Scene scene;

    const seed::EntityId camera = scene.create_entity("Camera", "camera");
    auto& camera_transform = scene.add_component<seed::TransformComponent>(camera);
    camera_transform.position = {0.0f, 1.5f, 5.0f};
    auto& camera_component = scene.add_component<seed::CameraComponent>(camera);
    camera_component.primary = true;

    const seed::EntityId actor = scene.create_entity("Character", "character");
    scene.add_component<seed::TransformComponent>(actor);

    seed::SkinnedMeshComponent skinned;
    skinned.mesh = character.mesh;
    skinned.asset_id = "character:Test.gltf";
    skinned.runtime_asset = &character;
    scene.add_component<seed::SkinnedMeshComponent>(actor, skinned);

    seed::AnimationPlayerComponent player;
    player.clip_name = "Walk";
    player.playback_speed = 1.0f;
    player.playing = true;
    player.events.push_back({.clip_name = "Walk", .name = "Footstep", .time_seconds = 0.75f});
    scene.add_component<seed::AnimationPlayerComponent>(actor, player);

    seed::MaterialComponent material;
    material.shader = {7};
    material.use_base_color_texture = false;
    material.use_metallic_roughness_texture = false;
    material.use_normal_texture = false;
    scene.add_component<seed::MaterialComponent>(actor, material);

    seed::AnimationSystem::update_scene(scene, 0.5f);

    auto* updated_player = scene.get_component<seed::AnimationPlayerComponent>(actor);
    assert(updated_player != nullptr);
    assert(near(updated_player->time_seconds, 0.5f));
    assert(updated_player->pose.skin_matrices.size() == 1);
    assert(near(updated_player->pose.global_matrices[0].values[12], 0.5f));
    assert(near(updated_player->pose.skin_matrices[0].values[12], 0.5f));
    assert(updated_player->triggered_events.empty());

    CaptureRenderer renderer;
    assert(seed::RenderSystem::render(scene, renderer, camera));
    assert(renderer.skinned_draw_calls == 1);
    assert(renderer.last_mesh.value == 42);
    assert(renderer.last_shader.value == 7);
    assert(renderer.last_palette.size() == 1);
    assert(near(renderer.last_palette[0].values[12], 0.5f));

    seed::AnimationSystem::update_scene(scene, 0.75f);
    updated_player = scene.get_component<seed::AnimationPlayerComponent>(actor);
    assert(updated_player != nullptr);
    assert(near(updated_player->time_seconds, 1.25f));
    // Looping clip wraps 1.25s to 0.25s.
    assert(near(updated_player->pose.skin_matrices[0].values[12], 0.25f));
    assert(updated_player->triggered_events.size() == 1);
    assert(updated_player->triggered_events.front() == "Footstep");

    std::cout << "Seed scene animation playback + render routing tests passed.\n";
    return 0;
}
