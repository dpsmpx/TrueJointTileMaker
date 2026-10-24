#include "gfx/surface.h"

#include <algorithm>
#include <cmath>

namespace tjtm::gfx {

namespace {

std::uint32_t pack(Color c) {
    return c.argb();
}

/// x / 255 с округлением — так делит pixman, на котором стоит GDI+ в Mono.
int div255(int x) {
    return (x + 128 + ((x + 128) >> 8)) >> 8;
}

/// Наложение поверх непрозрачного или прозрачного приёмника. Для
/// непрозрачного — целочисленно, как pixman: цвет источника умножается на
/// его альфу с округлением отдельно от доли приёмника. Формула подобрана по
/// снимкам эталона: подкраска выделения и метки полос цвета совпали до
/// единицы, а «(s·a + d·(255 − a)) / 255 с округлением» расходилась на одну.
std::uint32_t over(std::uint32_t dst, Color s) {
    const auto da = static_cast<int>(dst >> 24);
    if (da == 255) {
        const int a = s.a;
        const int ia = 255 - a;
        const int r = div255(s.r * a) + div255(static_cast<int>((dst >> 16) & 0xFF) * ia);
        const int g = div255(s.g * a) + div255(static_cast<int>((dst >> 8) & 0xFF) * ia);
        const int b = div255(s.b * a) + div255(static_cast<int>(dst & 0xFF) * ia);
        return 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) |
               (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
    }
    return blendOver(s, Color::fromArgb(dst)).argb();
}

}  // namespace

Surface::Surface(int w, int h, Color fill) {
    resize(w, h, fill);
}

void Surface::resize(int w, int h, Color fill) {
    w_ = std::max(0, w);
    h_ = std::max(0, h);
    px_.assign(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_), pack(fill));
    clipOn_ = false;
}

Color Surface::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return {};
    return Color::fromArgb(px_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) +
                               static_cast<std::size_t>(x)]);
}

void Surface::setPixel(int x, int y, Color c) {
    const Rect b = bounds();
    if (x < b.x0 || y < b.y0 || x >= b.x1 || y >= b.y1) return;
    px_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)] =
        pack(c);
}

void Surface::setClip(Rect r) {
    clip_ = r;
    clipOn_ = !r.empty();
}

void Surface::resetClip() {
    clipOn_ = false;
    clip_ = Rect{};
}

Rect Surface::bounds() const {
    Rect b{0, 0, w_, h_};
    if (clipOn_) {
        b.x0 = std::max(b.x0, clip_.x0);
        b.y0 = std::max(b.y0, clip_.y0);
        b.x1 = std::min(b.x1, clip_.x1);
        b.y1 = std::min(b.y1, clip_.y1);
    }
    return b;
}

void Surface::clear(Color c) {
    std::fill(px_.begin(), px_.end(), pack(c));
}

void Surface::blendPixel(int x, int y, Color c) {
    const Rect b = bounds();
    if (x < b.x0 || y < b.y0 || x >= b.x1 || y >= b.y1 || c.a == 0) return;
    std::uint32_t& d = px_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) +
                           static_cast<std::size_t>(x)];
    d = c.a == 255 ? pack(c) : over(d, c);
}

void Surface::blendPixelCoverage(int x, int y, Color c, int coverage) {
    if (coverage <= 0) return;
    if (coverage >= 255) {
        blendPixel(x, y, c);
        return;
    }
    Color cc = c;
    cc.a = static_cast<std::uint8_t>((c.a * coverage + 127) / 255);
    blendPixel(x, y, cc);
}

void Surface::fillRect(int x1, int y1, int x2, int y2, Color c) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    const Rect b = bounds();
    x1 = std::max(x1, b.x0);
    y1 = std::max(y1, b.y0);
    x2 = std::min(x2, b.x1);
    y2 = std::min(y2, b.y1);
    if (x2 <= x1 || y2 <= y1 || c.a == 0) return;
    for (int y = y1; y < y2; ++y) {
        std::uint32_t* row =
            px_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w_);
        if (c.a == 255) {
            std::fill(row + x1, row + x2, pack(c));
        } else {
            for (int x = x1; x < x2; ++x)
                row[x] = over(row[x], c);
        }
    }
}

void Surface::frameRect(int x0, int y0, int x1, int y1, Color c) {
    // Четыре полоски, как FrameRect оригинала: полупрозрачный цвет в углах
    // ложится дважды — так было и там.
    fillRect(x0, y0, x1, y0 + 1, c);
    fillRect(x0, y1 - 1, x1, y1, c);
    fillRect(x0, y0, x0 + 1, y1, c);
    fillRect(x1 - 1, y0, x1, y1, c);
}

void Surface::fillRoundRect(int x1, int y1, int x2, int y2, int rw, int rh, Color c) {
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    // Контур GraphABC: от (x1, y1) до (x2 - 1, y2 - 1), дуги w x h по углам.
    // В режиме смещения Half точка (i, j) занимает квадрат [i, i+1].
    const double L = x1;
    const double T = y1;
    const double R = x2 - 1;
    const double B = y2 - 1;
    if (R <= L || B <= T) return;
    const double a = std::min(rw / 2.0, (R - L) / 2.0);
    const double bb = std::min(rh / 2.0, (B - T) / 2.0);
    if (a <= 0 || bb <= 0) {
        fillRect(x1, y1, x2 - 1, y2 - 1, c);
        return;
    }
    auto inside = [&](double px, double py) {
        if (px < L || px > R || py < T || py > B) return false;
        double cx = 0;
        double cy = 0;
        if (px < L + a) {
            cx = L + a;
        } else if (px > R - a) {
            cx = R - a;
        } else {
            return true;
        }
        if (py < T + bb) {
            cy = T + bb;
        } else if (py > B - bb) {
            cy = B - bb;
        } else {
            return true;
        }
        const double dx = (px - cx) / a;
        const double dy = (py - cy) / bb;
        return dx * dx + dy * dy <= 1.0;
    };
    const Rect bnd = bounds();
    const int ix0 = std::max(x1, bnd.x0);
    const int iy0 = std::max(y1, bnd.y0);
    const int ix1 = std::min(x2 - 1, bnd.x1);
    const int iy1 = std::min(y2 - 1, bnd.y1);
    constexpr int kSub = 8;
    for (int y = iy0; y < iy1; ++y) {
        const bool rowInCorner = y < T + bb || y + 1 > B - bb;
        for (int x = ix0; x < ix1; ++x) {
            const bool colInCorner = x < L + a || x + 1 > R - a;
            if (!rowInCorner || !colInCorner) {
                blendPixel(x, y, c);
                continue;
            }
            // Угол: покрытие точки по сетке 8x8 подотсчётов — сглаживание
            // GDI+ с AntiAlias дало бы ту же долю площади.
            int hit = 0;
            for (int sy = 0; sy < kSub; ++sy) {
                for (int sx = 0; sx < kSub; ++sx) {
                    if (inside(x + (sx + 0.5) / kSub, y + (sy + 0.5) / kSub)) ++hit;
                }
            }
            blendPixelCoverage(x, y, c, hit * 255 / (kSub * kSub));
        }
    }
}

void Surface::line(int x1, int y1, int x2, int y2, Color c) {
    if (x1 == x2) {
        fillRect(x1, std::min(y1, y2), x1 + 1, std::max(y1, y2) + 1, c);
        return;
    }
    if (y1 == y2) {
        fillRect(std::min(x1, x2), y1, std::max(x1, x2) + 1, y1 + 1, c);
        return;
    }
    const int dx = std::abs(x2 - x1);
    const int dy = -std::abs(y2 - y1);
    const int sx = x1 < x2 ? 1 : -1;
    const int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        blendPixel(x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void Surface::dashedRect(int x0, int y0, int x1, int y1, Color on, int dash, int phase) {
    if (x1 <= x0 || y1 <= y0 || dash <= 0) return;
    const int w = x1 - x0;
    const int h = y1 - y0;
    const int period = dash * 2;
    // Положение точки на обходе по часовой стрелке от левого верхнего угла.
    // Так раскладывает пунктир DrawRectangle: сверено с эталоном.
    auto dark = [&](int t) { return ((t + phase) % period + period) % period < dash; };
    for (int x = x0; x <= x1; ++x) {
        if (dark(x - x0)) blendPixel(x, y0, on);
    }
    for (int y = y0 + 1; y <= y1; ++y) {
        if (dark(w + (y - y0))) blendPixel(x1, y, on);
    }
    for (int x = x0; x < x1; ++x) {
        if (dark(w + h + (x1 - x) - 1)) blendPixel(x, y1, on);
    }
    for (int y = y0 + 1; y < y1; ++y) {
        if (dark(2 * w + h + (y1 - y) - 1)) blendPixel(x0, y, on);
    }
}

void Surface::drawImage(const Grid<Color>& img, int x, int y, int w, int h) {
    if (img.empty() || w <= 0 || h <= 0) return;
    const Rect b = bounds();
    const int dx0 = std::max(x, b.x0);
    const int dy0 = std::max(y, b.y0);
    const int dx1 = std::min(x + w, b.x1);
    const int dy1 = std::min(y + h, b.y1);
    for (int dy = dy0; dy < dy1; ++dy) {
        // Ближайший сосед по центру точки приёмника: при целом масштабе
        // каждая исходная точка — ровный квадрат.
        const auto sy =
            static_cast<int>((static_cast<long long>(dy - y) * img.h * 2 + img.h) / (2LL * h));
        for (int dx = dx0; dx < dx1; ++dx) {
            const auto sx =
                static_cast<int>((static_cast<long long>(dx - x) * img.w * 2 + img.w) / (2LL * w));
            blendPixel(dx, dy, img.at(sx, sy));
        }
    }
}

void Surface::drawBgra(const std::vector<std::uint8_t>& raw, int srcW, int srcH, int x, int y,
                       int w, int h) {
    if (srcW <= 0 || srcH <= 0 ||
        raw.size() < static_cast<std::size_t>(srcW) * static_cast<std::size_t>(srcH) * 4)
        return;
    Grid<Color> g(srcW, srcH);
    for (std::size_t i = 0; i < g.d.size(); ++i)
        g.d[i] = Color(raw[i * 4 + 3], raw[i * 4 + 2], raw[i * 4 + 1], raw[i * 4]);
    drawImage(g, x, y, w, h);
}

Grid<Color> Surface::toGrid() const {
    Grid<Color> g(w_, h_);
    for (std::size_t i = 0; i < px_.size(); ++i)
        g.d[i] = Color::fromArgb(px_[i]);
    return g;
}

}  // namespace tjtm::gfx
