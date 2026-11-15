#pragma once

namespace lynx::touchgestures
{
inline bool& verticalPinchFlag() noexcept
{
    static bool active = false;
    return active;
}

struct ScopedVerticalPinch
{
    explicit ScopedVerticalPinch (bool vertical) noexcept : prev (verticalPinchFlag())
    {
        verticalPinchFlag() = vertical;
    }
    ~ScopedVerticalPinch() noexcept
    {
        verticalPinchFlag() = prev;
    }
    bool prev;
};

inline bool isVerticalPinch() noexcept
{
    return verticalPinchFlag();
}
} // namespace lynx::touchgestures
