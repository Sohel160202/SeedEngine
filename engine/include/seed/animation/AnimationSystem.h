#pragma once

#include "seed/animation/AnimationData.h"

namespace seed {

class AnimationSystem {
public:
    static Quaternion normalize_quaternion(const Quaternion& value) noexcept;
    static Quaternion nlerp_quaternion(
        const Quaternion& a,
        const Quaternion& b,
        float alpha
    ) noexcept;

    static Mat4 joint_transform_matrix(const JointTransform& transform) noexcept;

    static void initialize_bind_pose(
        const Skeleton& skeleton,
        SkeletonPose& pose
    );

    static void evaluate_clip(
        const Skeleton& skeleton,
        const AnimationClip& clip,
        float time_seconds,
        SkeletonPose& pose
    );

    static void rebuild_matrices(
        const Skeleton& skeleton,
        SkeletonPose& pose
    );
};

} // namespace seed
