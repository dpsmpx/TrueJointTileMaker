// Значок программы и логотип (план «Интерфейс», D5, UX-36). У 1.6 у окна
// не было значка, а название в меню было набрано обычным текстом. Цвета —
// фирменные, а не роли темы: значок одинаков в любой теме, поэтому он в
// ядре, а не среди экранов.
// Значок — пиксельный и рисуется кодом: четыре тайла с общим стыком
// посередине — то, ради чего редактор назван True Joint.

#include "core/logo.h"

namespace tjtm {

Grid<Color> appIcon() {
    constexpr int kN = 32;
    Grid<Color> g(kN, kN, argb(0, 0, 0, 0));
    const Color frame = argb(255, 0x1B, 0x2A, 0x41);
    const Color accent = argb(255, 0x24, 0x68, 0xD8);
    const Color grassA = argb(255, 0x7A, 0xC0, 0x4C);
    const Color grassB = argb(255, 0x4E, 0x8F, 0x2E);
    const Color sand = argb(255, 0xE8, 0xD3, 0x9A);
    const Color joint = argb(255, 0xF4, 0xF7, 0xFC);
    for (int y = 0; y < kN; ++y) {
        for (int x = 0; x < kN; ++x) {
            // Скруглённый квадрат: углы прозрачные.
            const bool corner = (x < 2 || x > kN - 3) && (y < 2 || y > kN - 3);
            if (corner && (x == 0 || x == kN - 1 || y == 0 || y == kN - 1)) continue;
            const bool edge = x < 2 || y < 2 || x > kN - 3 || y > kN - 3;
            if (edge) {
                g.at(x, y) = frame;
                continue;
            }
            // Четыре тайла: трава и песок шахматкой, на траве — узор.
            const bool right = x >= kN / 2;
            const bool low = y >= kN / 2;
            Color c = (right != low) ? sand : grassA;
            if (c == grassA && ((x + y) % 5 == 0)) c = grassB;
            g.at(x, y) = c;
        }
    }
    // Стык — крестом посередине, акцентом с белой серединой.
    for (int k = 2; k < kN - 2; ++k) {
        g.at(kN / 2 - 1, k) = accent;
        g.at(kN / 2, k) = joint;
        g.at(k, kN / 2 - 1) = accent;
        g.at(k, kN / 2) = joint;
    }
    return g;
}

}  // namespace tjtm
