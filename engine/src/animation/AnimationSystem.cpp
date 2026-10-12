#include "seed/animation/AnimationSystem.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace seed {
namespace {

float quaternion_dot(const Quaternion& a, const Quaternion& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec3 sample_vec3(
    const std::vector<Vec3Keyframe>& keys,
    float time,
    const Vec3& fallback
) {
    if (keys.empty()) return fallback;
    if (keys.size() == 1 || time <= keys.front().time_seconds) return keys.front().value;
    if (time >= keys.back().time_seconds) return keys.back().value;

    const auto upper = std::upper_bound(
        keys.begin(), keys.end(), time,
        [](float value, const Vec3Keyframe& key) { return value < key.time_seconds; }
    );
    const auto lower = upper - 1;
    const float span = upper->time_seconds - lower->time_seconds;
    const float alpha = span > 0.000001f
        ? std::clamp((time - lower->time_seconds) / span, 0.0f, 1.0f)
        : 0.0f;
    return lower->value * (1.0f - alpha) + upper->value * alpha;
}

Quaternion sample_quaternion(
    const std::vector<QuaternionKeyframe>& keys,
    float time,
    const Quaternion& fallback
) {
    if (keys.empty()) return fallback;
    if (keys.size() == 1 || time <= keys.front().time_seconds) {
        return AnimationSystem::normalize_quaternion(keys.front().value);
    }
    if (time >= keys.back().time_seconds) {
        return AnimationSystem::normalize_quaternion(keys.back().value);
    }

    const auto upper = std::upper_bound(
        keys.begin(), keys.end(), time,
        [](float value, const QuaternionKeyframe& key) { return value < key.time_seconds; }
    );
    const auto lower = upper - 1;
    const float span = upper->time_seconds - lower->time_seconds;
    const float alpha = span > 0.000001f
        ? std::clamp((time - lower->time_seconds) / span, 0.0f, 1.0f)
        : 0.0f;
    return AnimationSystem::nlerp_quaternion(lower->value, upper->value, alpha);
}

float clip_time(const AnimationClip& clip, float time_seconds) {
    const float duration = std::max(0.0f, clip.duration_seconds);
    if (duration <= 0.000001f) return 0.0f;

    if (!clip.looping) return std::clamp(time_seconds, 0.0f, duration);

    float wrapped = std::fmod(time_seconds, duration);
    if (wrapped < 0.0f) wrapped += duration;
    return wrapped;
}

const AnimationClip* find_clip(
    const AnimatedModelAssetResource& asset,
    const std::string& requested_name
) {
    if (!requested_name.empty()) {
        const auto found = std::find_if(
            asset.animations.begin(),
            asset.animations.end(),
            [&](const AnimationClip& clip) { return clip.name == requested_name; }
        );
        if (found != asset.animations.end()) return &*found;
    }
    return asset.animations.empty() ? nullptr : &asset.animations.front();
}

const AnimationClip* find_named_clip(
    const AnimatedModelAssetResource& asset,
    const char* name
) {
    const auto found = std::find_if(
        asset.animations.begin(),
        asset.animations.end(),
        [name](const AnimationClip& clip) { return clip.name == name; }
    );
    return found == asset.animations.end() ? nullptr : &*found;
}

void collect_animation_events(
    const AnimationClip& clip,
    const std::vector<AnimationEvent>& events,
    float previous_time,
    float current_time,
    std::vector<std::string>& triggered
) {
    if (events.empty() || clip.duration_seconds <= 0.000001f || current_time <= previous_time) return;

    const float duration = clip.duration_seconds;
    const float previous_phase = clip_time(clip, previous_time);
    const float current_phase = clip_time(clip, current_time);
    const bool crossed_full_loop = clip.looping && (current_time - previous_time >= duration);

    for (const auto& event : events) {
        if (!event.clip_name.empty() && event.clip_name != clip.name) continue;
        const float event_time = std::clamp(event.time_seconds, 0.0f, duration);
        bool crossed = false;
        if (!clip.looping) {
            crossed = event_time > previous_phase && event_time <= current_phase;
        } else if (crossed_full_loop) {
            crossed = true;
        } else if (current_phase >= previous_phase) {
            crossed = event_time > previous_phase && event_time <= current_phase;
        } else {
            crossed = event_time > previous_phase || event_time <= current_phase;
        }
        if (crossed) triggered.push_back(event.name);
    }
}

void blend_pose(
    const Skeleton& skeleton,
    const SkeletonPose& from,
    float alpha,
    SkeletonPose& target
) {
    if (from.local_transforms.size() != skeleton.joints.size() ||
        target.local_transforms.size() != skeleton.joints.size()) {
        return;
    }

    for (std::size_t i = 0; i < skeleton.joints.size(); ++i) {
        const auto& source = from.local_transforms[i];
        auto& destination = target.local_transforms[i];
        destination.translation =
            source.translation * (1.0f - alpha) + destination.translation * alpha;
        destination.rotation = AnimationSystem::nlerp_quaternion(
            source.rotation,
            destination.rotation,
            alpha
        );
        destination.scale =
            source.scale * (1.0f - alpha) + destination.scale * alpha;
    }
    AnimationSystem::rebuild_matrices(skeleton, target);
}

} // namespace

Quaternion AnimationSystem::normalize_quaternion(const Quaternion& value) noexcept {
    const float magnitude = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w
    );
    if (magnitude <= 0.000001f) return {};
    const float inverse = 1.0f / magnitude;
    return {
        value.x * inverse,
        value.y * inverse,
        value.z * inverse,
        value.w * inverse,
    };
}

Quaternion AnimationSystem::nlerp_quaternion(
    const Quaternion& a,
    const Quaternion& b,
    float alpha
) noexcept {
    Quaternion start = normalize_quaternion(a);
    Quaternion end = normalize_quaternion(b);
    if (quaternion_dot(start, end) < 0.0f) {
        end.x = -end.x;
        end.y = -end.y;
        end.z = -end.z;
        end.w = -end.w;
    }

    const float t = std::clamp(alpha, 0.0f, 1.0f);
    return normalize_quaternion({
        start.x * (1.0f - t) + end.x * t,
        start.y * (1.0f - t) + end.y * t,
        start.z * (1.0f - t) + end.z * t,
        start.w * (1.0f - t) + end.w * t,
    });
}

Mat4 AnimationSystem::joint_transform_matrix(const JointTransform& transform) noexcept {
    const Quaternion q = normalize_quaternion(transform.rotation);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float xw = q.x * q.w;
    const float yw = q.y * q.w;
    const float zw = q.z * q.w;

    Mat4 rotation = Mat4::identity();
    rotation.values[0] = 1.0f - 2.0f * (yy + zz);
    rotation.values[1] = 2.0f * (xy + zw);
    rotation.values[2] = 2.0f * (xz - yw);

    rotation.values[4] = 2.0f * (xy - zw);
    rotation.values[5] = 1.0f - 2.0f * (xx + zz);
    rotation.values[6] = 2.0f * (yz + xw);

    rotation.values[8] = 2.0f * (xz + yw);
    rotation.values[9] = 2.0f * (yz - xw);
    rotation.values[10] = 1.0f - 2.0f * (xx + yy);

    return translation_matrix(transform.translation) *
        rotation * scale_matrix(transform.scale);
}

void AnimationSystem::initialize_bind_pose(
    const Skeleton& skeleton,
    SkeletonPose& pose
) {
    pose.local_transforms.resize(skeleton.joints.size());
    for (std::size_t i = 0; i < skeleton.joints.size(); ++i) {
        pose.local_transforms[i] = skeleton.joints[i].bind_local;
    }
    rebuild_matrices(skeleton, pose);
}

void AnimationSystem::evaluate_clip(
    const Skeleton& skeleton,
    const AnimationClip& clip,
    float time_seconds,
    SkeletonPose& pose
) {
    initialize_bind_pose(skeleton, pose);
    if (skeleton.joints.empty() || clip.tracks.empty()) return;

    const float time = clip_time(clip, time_seconds);
    for (const auto& track : clip.tracks) {
        if (track.joint_index >= pose.local_transforms.size()) continue;

        auto& transform = pose.local_transforms[track.joint_index];
        transform.translation = sample_vec3(track.translation_keys, time, transform.translation);
        transform.rotation = sample_quaternion(track.rotation_keys, time, transform.rotation);
        transform.scale = sample_vec3(track.scale_keys, time, transform.scale);
    }

    rebuild_matrices(skeleton, pose);
}

void AnimationSystem::rebuild_matrices(
    const Skeleton& skeleton,
    SkeletonPose& pose
) {
    const std::size_t count = skeleton.joints.size();
    if (pose.local_transforms.size() != count) {
        pose.local_transforms.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            pose.local_transforms[i] = skeleton.joints[i].bind_local;
        }
    }

    pose.global_matrices.assign(count, Mat4::identity());
    pose.skin_matrices.assign(count, Mat4::identity());
    std::vector<std::uint8_t> state(count, 0);

    std::function<void(std::size_t)> solve = [&](std::size_t index) {
        if (state[index] == 2) return;
        if (state[index] == 1) {
            // Defensive cycle handling. Invalid skeletons fall back to local space
            // instead of recursing forever.
            pose.global_matrices[index] = joint_transform_matrix(pose.local_transforms[index]);
            state[index] = 2;
            return;
        }

        state[index] = 1;
        const Mat4 local = joint_transform_matrix(pose.local_transforms[index]);
        const std::int32_t parent = skeleton.joints[index].parent;
        if (parent >= 0 && static_cast<std::size_t>(parent) < count &&
            static_cast<std::size_t>(parent) != index) {
            solve(static_cast<std::size_t>(parent));
            pose.global_matrices[index] = pose.global_matrices[static_cast<std::size_t>(parent)] * local;
        } else {
            pose.global_matrices[index] = local;
        }
        state[index] = 2;
    };

    for (std::size_t i = 0; i < count; ++i) solve(i);
    for (std::size_t i = 0; i < count; ++i) {
        pose.skin_matrices[i] = pose.global_matrices[i] * skeleton.joints[i].inverse_bind_matrix;
    }
}

void AnimationSystem::update_scene(Scene& scene, float delta_seconds) {
    scene.for_each<SkinnedMeshComponent, AnimationPlayerComponent>(
        [&](EntityId, SkinnedMeshComponent& mesh, AnimationPlayerComponent& player) {
            player.triggered_events.clear();
            const AnimatedModelAssetResource* asset = mesh.runtime_asset;
            if (asset == nullptr || asset->skeleton.joints.empty()) {
                player.pose = {};
                return;
            }

            const AnimationClip* clip = find_clip(*asset, player.clip_name);
            if (player.locomotion_enabled) {
                const char* desired_name = "Idle";
                if (!player.locomotion_grounded) {
                    desired_name = player.locomotion_vertical_velocity > 0.1f ? "Jump" : "Fall";
                } else if (player.locomotion_speed > 0.01f) {
                    desired_name = player.locomotion_sprinting ? "Run" : "Walk";
                }

                if (const AnimationClip* locomotion_clip = find_named_clip(*asset, desired_name);
                    locomotion_clip != nullptr) {
                    if (clip != locomotion_clip) {
                        player.time_seconds = 0.0f;
                    }
                    clip = locomotion_clip;
                    player.clip_name = clip->name;
                }
            }

            if (clip == nullptr) {
                initialize_bind_pose(asset->skeleton, player.pose);
                player.active_clip_name.clear();
                player.blend_from_pose = {};
                return;
            }

            const bool clip_changed = player.active_clip_name != clip->name;
            if (clip_changed) {
                if (player.pose.local_transforms.size() == asset->skeleton.joints.size()) {
                    player.blend_from_pose = player.pose;
                } else {
                    player.blend_from_pose = {};
                }
                player.active_clip_name = clip->name;
                player.blend_elapsed_seconds = 0.0f;
            }

            const float previous_time = clip_changed ? 0.0f : player.time_seconds;
            if (player.playing) {
                player.time_seconds += delta_seconds * player.playback_speed;
                if (delta_seconds > 0.0f && player.playback_speed > 0.0f) {
                    collect_animation_events(
                        *clip,
                        player.events,
                        previous_time,
                        player.time_seconds,
                        player.triggered_events
                    );
                }
            }

            SkeletonPose target_pose;
            evaluate_clip(asset->skeleton, *clip, player.time_seconds, target_pose);

            const float blend_duration = std::max(0.0f, player.blend_duration_seconds);
            if (blend_duration > 0.000001f &&
                !player.blend_from_pose.local_transforms.empty() &&
                player.blend_elapsed_seconds < blend_duration) {
                player.blend_elapsed_seconds += std::max(0.0f, delta_seconds);
                const float alpha = std::clamp(
                    player.blend_elapsed_seconds / blend_duration,
                    0.0f,
                    1.0f
                );
                blend_pose(asset->skeleton, player.blend_from_pose, alpha, target_pose);
                player.pose = std::move(target_pose);
                if (alpha >= 1.0f) player.blend_from_pose = {};
            } else {
                player.blend_from_pose = {};
                player.pose = std::move(target_pose);
            }
        }
    );
}

} // namespace seed
