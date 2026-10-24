// Проект .tjt — собственный формат документа (этап 3 RoadToProLevel).
//
// Файл — обычный PNG: его картинка — сведённый первый кадр, и любой
// просмотрщик покажет проект как картинку. Сам проект лежит в служебном
// куске tjTp (служебный, частный, безопасный для копирования), сжатый
// zlib. Внутри — разделы с метками из четырёх букв и длиной:
//
//   HEAD  размеры, индексный ли режим, активные слой и кадр
//   PALT  палитра и прозрачный номер
//   LAYR  слои: имя, видимость, замок, непрозрачность
//   FRAM  кадры: длительность
//   TAGS  метки анимации
//   CELS  непустые ячейки: RGBA и номера палитры
//   SELM  выделение
//   VIEW  вид и инструменты: масштаб, сетка, шахматка, оси симметрии…
//   META  пары «ключ = значение»
//
// Незнакомый раздел пропускается: файл новой версии откроется старой с
// потерей лишь того, чего она не знает. Числа — little-endian.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tjtm {

class Model;

inline constexpr const char* kProjectChunk = "tjTp";
inline constexpr std::uint32_t kProjectVersion = 1;

/// Проект целиком: PNG с куском tjTp. Активная ячейка записывается в
/// таблицу перед сохранением.
std::vector<std::uint8_t> projectEncode(Model& m);
/// Разбирает проект в модель. Модель меняется, только если разбор удался
/// целиком.
bool projectDecode(const std::vector<std::uint8_t>& file, Model& m, std::string& error);
/// Есть ли в файле кусок проекта.
bool isProjectFile(const std::vector<std::uint8_t>& file);

bool projectSave(const std::string& path, Model& m, std::string& error);
bool projectLoad(const std::string& path, Model& m, std::string& error);

}  // namespace tjtm
