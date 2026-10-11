#include "seed/animation/AnimationSystem.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    seed::Skeleton skeleton;
    skeleton.name = "Two Joint Test";

    seed::SkeletonJoint root;
    root.name = "Root";
    root.parent = -1;
    root.bind_local.translation = {0.0f, 0.0f, 0.0f};
    skeleton.joints.push_back(root);

    seed::SkeletonJoint child;
    child.name = "Child";
    child.parent = 0;
    child.bind_local.translation = {0.0f, 1.0f, 0.0f};
    skeleton.joints.push_back(child);

    seed::AnimationClip clip;
    clip.name = "MoveAndTurn";
    clip.duration_seconds = 1.0f;
    clip.looping = false;

    seed::JointAnimationTrack root_track;
    root_track.joint_index = 0;
    root_track.translation_keys = {
        {0.0f, {0.0f, 0.0f, 0.0f}},
        {1.0f, {2.0f, 0.0f, 0.0f}},
    };
    clip.tracks.push_back(root_track);

    seed::JointAnimationTrack child_track;
    child_track.joint_index = 1;
    child_track.rotation_keys = {
        {0.0f, {0.0f, 0.0f, 0.0f, 1.0f}},
        {1.0f, {0.0f, 1.0f, 0.0f, 0.0f}},
    };
    clip.tracks.push_back(child_track);

    seed::SkeletonPose pose;
    seed::AnimationSystem::evaluate_clip(skeleton, clip, 0.5f, pose);

    assert(pose.local_transforms.size() == 2);
    assert(pose.global_matrices.size() == 2);
    assert(pose.skin_matrices.size() == 2);

    assert(near(pose.local_transforms[0].translation.x, 1.0f));
    assert(near(pose.global_matrices[0].values[12], 1.0f));
    assert(near(pose.global_matrices[1].values[12], 1.0f));
    assert(near(pose.global_matrices[1].values[13], 1.0f));

    const auto& half_rotation = pose.local_transforms[1].rotation;
    assert(near(std::fabs(half_rotation.y), 0.7071067f, 0.002f));
    assert(near(std::fabs(half_rotation.w), 0.7071067f, 0.002f));

    // Non-looping clips clamp at the final key.
    seed::AnimationSystem::evaluate_clip(skeleton, clip, 2.0f, pose);
    assert(near(pose.local_transforms[0].translation.x, 2.0f));

    // Looping clips wrap into their duration.
    clip.looping = true;
    seed::AnimationSystem::evaluate_clip(skeleton, clip, 1.25f, pose);
    assert(near(pose.local_transforms[0].translation.x, 0.5f));

    // Empty clips reproduce the bind pose.
    seed::AnimationClip empty;
    empty.duration_seconds = 1.0f;
    seed::AnimationSystem::evaluate_clip(skeleton, empty, 0.5f, pose);
    assert(near(pose.local_transforms[1].translation.y, 1.0f));

    std::cout << "Seed animation core tests passed.\n";
    return 0;
}
