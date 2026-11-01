// Значок программы (D5): окно, стартовый экран, справка.
#pragma once

#include "core/color.h"
#include "core/grid.h"

namespace tjtm {

/// Значок 32×32: четыре тайла с общим стыком посередине.
Grid<Color> appIcon();

}  // namespace tjtm
