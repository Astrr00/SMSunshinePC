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
constexpr std::uint32_t kMarioJumping = 0x800;
inline bool brakingJump(std::uint32_t status, float heightAboveFloor) {
    return (status & kMarioJumping) != 0 && heightAboveFloor >= 10.f;
}
} // namespace devscript
