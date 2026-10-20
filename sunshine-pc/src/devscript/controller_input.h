#pragma once
#include <cmath>
#include <cstdint>

namespace devscript {
// Invert MarioMove.cpp's intended yaw: atan2(stickH, -stickV) + camera input yaw.
inline float cameraRelativeStick(float dx, float dz, std::int16_t cameraYaw, std::int8_t& sx, std::int8_t& sy) {
    const float dist = std::hypot(dx, dz);
    sx = sy = 0;
    if (dist < 1.f) return dist;
    const float angle = std::atan2(dx, dz) - cameraYaw * (6.2831853f / 65536.f);
    // PADClamp removes 15 raw units per axis; its remaining octagon scaling preserves heading.
    const int x = static_cast<int>(112.f * std::sin(angle));
    const int y = static_cast<int>(-112.f * std::cos(angle));
    sx = static_cast<std::int8_t>(x + (x > 0 ? 15 : x < 0 ? -15 : 0));
    sy = static_cast<std::int8_t>(y + (y > 0 ? 15 : y < 0 ? -15 : 0));
    return dist;
}
// Mario's Hover turns only between its forward and backward thrust cones.
inline float hoverSteeringDelta(float delta) {
    const float magnitude = std::fabs(delta);
    return magnitude > 0.05f && (magnitude < 0.5236f || magnitude > 2.6179f)
        ? std::copysign(0.7854f, delta) : delta;
}
// Small turns invert Mario's second deadzone after JUTGamePad scales the stick by 128/54.
inline std::int8_t smallTurnAxis(std::int8_t raw) {
    const int magnitude = raw < 0 ? -static_cast<int>(raw) : raw;
    if (magnitude <= 15) return 0;
    const int desired = static_cast<int>(std::lround(48.f * (magnitude - 15) / 112.f));
    if (!desired) return 0;
    const int clamped = static_cast<int>(std::ceil((desired + 6) * 54.f / 128.f));
    const int value = 15 + static_cast<int>(std::ceil(clamped * 112.f / 72.f));
    return static_cast<std::int8_t>(raw < 0 ? -value : value);
}

constexpr std::uint32_t kMarioJumping = 0x800;
inline bool brakingJump(std::uint32_t status, float heightAboveFloor) {
    return (status & kMarioJumping) != 0 && heightAboveFloor >= 10.f;
}
} // namespace devscript
