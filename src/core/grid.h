// Двумерный массив с обращением [x, y], как array[,] оригинала.
//
// Оригинал индексировал Tile[x, y]: первый индекс — столбец. Здесь тот
// же порядок обращения, а хранение построчное: строка — непрерывный кусок
// памяти, и выдача картинки построчно не требует перестановок.
#pragma once

#include <cstddef>
#include <vector>

namespace tjtm {

template <class T>
struct Grid {
    int w = 0;
    int h = 0;
    std::vector<T> d;

    Grid() = default;
    Grid(int ww, int hh, const T& fill = T{}) { resize(ww, hh, fill); }

    void resize(int ww, int hh, const T& fill = T{}) {
        w = ww < 0 ? 0 : ww;
        h = hh < 0 ? 0 : hh;
        d.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill);
    }
    void clear() {
        w = 0;
        h = 0;
        d.clear();
    }
    bool empty() const { return w <= 0 || h <= 0; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    bool sameSize(int ww, int hh) const { return w == ww && h == hh; }

    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
               static_cast<std::size_t>(x);
    }
    T& at(int x, int y) { return d[index(x, y)]; }
    const T& at(int x, int y) const { return d[index(x, y)]; }

    friend bool operator==(const Grid& a, const Grid& b) {
        return a.w == b.w && a.h == b.h && a.d == b.d;
    }
};

}  // namespace tjtm
