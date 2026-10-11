#pragma once

#include "seed/math/Math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace seed {

struct Quaternion {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{1.0f};
};

struct JointTransform {
    Vec3 translation{};
    Quaternion rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct SkeletonJoint {
    std::string name;
    std::int32_t parent{-1};
    JointTransform bind_local{};
    Mat4 inverse_bind_matrix{Mat4::identity()};
};

struct Skeleton {
    std::string name;
    std::vector<SkeletonJoint> joints;
};

struct Vec3Keyframe {
    float time_seconds{0.0f};
    Vec3 value{};
};

struct QuaternionKeyframe {
    float time_seconds{0.0f};
    Quaternion value{};
};

struct JointAnimationTrack {
    std::uint32_t joint_index{0};
    std::vector<Vec3Keyframe> translation_keys;
    std::vector<QuaternionKeyframe> rotation_keys;
    std::vector<Vec3Keyframe> scale_keys;
};

struct AnimationClip {
    std::string name;
    float duration_seconds{0.0f};
    bool looping{true};
    std::vector<JointAnimationTrack> tracks;
};

struct SkeletonPose {
    std::vector<JointTransform> local_transforms;
    std::vector<Mat4> global_matrices;
    std::vector<Mat4> skin_matrices;
};

} // namespace seed
