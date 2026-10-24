// GIF: запись и чтение анимации без сторонних библиотек (этап 4).
//
// GIF хранит до 256 цветов и прозрачность «да или нет». Запись берёт
// цвета всех кадров в одну общую таблицу: если их не больше 255 (один
// номер — под прозрачность), они ложатся точно, иначе сводятся медианным
// разрезом. Точка с альфой меньше половины — прозрачная. Цвета палитры
// документа идут в таблицу первыми и в её порядке: индексная картинка
// выходит из редактора с теми же номерами.
//
// Кадр пишется во весь холст. Если в анимации есть прозрачность, после
// показа кадр стирается (способ очистки 2), иначе прозрачные места нового
// кадра показали бы старый.
//
// Чтение сводит кадры на холст по их способам очистки, как браузер.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/png.h"

namespace tjtm {

/// Пишет GIF89a. plays — сколько раз проиграть, 0 — бесконечно, как у
/// APNG. preferred — цвета, которые встают в таблицу первыми и в своём
/// порядке (палитра).
std::vector<std::uint8_t> gifEncode(const std::vector<AnimFrame>& frames, int plays = 0,
                                    const std::vector<Color>& preferred = {});

/// Читает GIF кадрами во весь холст. plays — сколько раз проиграть.
bool gifDecode(const std::vector<std::uint8_t>& file, std::vector<AnimFrame>& frames,
               std::string& error, int* plays = nullptr);

/// Начинается ли файл с подписи GIF.
bool isGif(const std::vector<std::uint8_t>& file);

/// Сжатие LZW по правилам GIF: номера точек → поток кодов без разбивки
/// на блоки. Отдельно — для тестов.
std::vector<std::uint8_t> gifLzwEncode(const std::vector<std::uint8_t>& indices, int minCodeSize);
bool gifLzwDecode(const std::vector<std::uint8_t>& data, int minCodeSize, std::size_t expected,
                  std::vector<std::uint8_t>& out);

}  // namespace tjtm
