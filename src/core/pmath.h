// Арифметика PascalABC.NET, от которой зависит точность переноса.
//
// Оригинал считает в .NET, и несколько его правил отличаются от привычных
// в C++. Здесь они собраны в одном месте, чтобы перенос не мог разойтись
// с эталоном незаметно: цвет, полосы выбора и масштаб опираются на них.
#pragma once

#include <cmath>
#include <cstdint>

namespace tjtm {

/// Round из PascalABC.NET — это Convert.ToInt32(double): банковское
/// округление, половина к чётному. 2.5 даёт 2, 3.5 даёт 4. std::lround
/// округлял бы половину от нуля и расходился бы с оригиналом ровно на
/// тех значениях, где HSV попадает на границу канала.
inline int pround(double x) {
    return static_cast<int>(std::nearbyint(x));
}

/// Floor из PascalABC.NET: Math.Floor и приведение к целому.
inline int pfloor(double x) {
    return static_cast<int>(std::floor(x));
}

/// ClampI из TJTMColor: значение в пределах lo..hi.
inline int clampI(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/// Деление с округлением вниз. Встроенное деление C++, как и div в
/// Паскале, отсекает к нулю; для отрицательных номеров копий тайла это
/// пропустило бы одну из них.
inline int floorDiv(int a, int b) {
    if (b == 0) return 0;
    if ((a < 0) != (b < 0)) {
        return (a % b == 0) ? a / b : a / b - 1;
    }
    return a / b;
}

/// mod в Паскале, как и % в C++, берёт знак делимого. Обёртка лишь
/// напоминает, что это одно и то же, и называет операцию так же.
inline int pmod(int a, int b) {
    return a % b;
}

}  // namespace tjtm
