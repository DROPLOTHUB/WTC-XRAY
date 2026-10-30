#pragma once
#include <algorithm>
#include <cmath>

namespace overlay_profile_position {
struct Point { float x{}, y{}; };
inline Point top_right(float viewport_x, float viewport_y, float viewport_width,
                       float text_width, float margin = 15.0f) {
    if (!std::isfinite(viewport_x) || !std::isfinite(viewport_y) ||
        !std::isfinite(viewport_width) || !std::isfinite(text_width) ||
        viewport_width <= 0.0f || text_width < 0.0f ||
        !std::isfinite(margin) || margin < 0.0f)
        return {viewport_x, viewport_y};
    return {std::max(viewport_x + margin,
                     viewport_x + viewport_width - margin - text_width),
            viewport_y + margin};
}
}
