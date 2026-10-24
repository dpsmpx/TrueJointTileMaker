// Строки: UTF-8 внутри переноса, правила .NET там, где их видно снаружи.
//
// Оригинал работал со строками .NET — UTF-16, Trim по char.IsWhiteSpace,
// ToLower с кириллицей. Перенос хранит строки в UTF-8, а эти правила
// повторяет здесь: от них зависит, какой ключ Config.txt узнаётся и какой
// длины считается строка сетки карты.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tjtm {

/// Декодирует UTF-8 в кодовые точки. Испорченная последовательность даёт
/// U+FFFD, как StreamReader в .NET.
std::u32string utf8ToU32(std::string_view s);
std::string u32ToUtf8(std::u32string_view s);
void appendUtf8(std::string& out, char32_t cp);

/// Длина в единицах UTF-16 — Length строки .NET.
std::size_t utf16Length(std::string_view s);

/// Декодирует CP1251 в UTF-8: запасной путь чтения старого Config.txt.
std::string cp1251ToUtf8(std::string_view s);

/// char.IsWhiteSpace из .NET для кодовой точки.
bool isNetWhiteSpace(char32_t c);
/// String.Trim из .NET.
std::string netTrim(std::string_view s);
/// String.ToLower: латиница и кириллица, включая Ё.
std::string netToLower(std::string_view s);

/// Деление текста на строки так, как это делает File.ReadAllLines:
/// разделители CR, LF и CRLF, метка BOM в начале снимается, последний
/// перевод строки не даёт пустой строки в конце.
std::vector<std::string> splitNetLines(std::string_view text);

/// Удаляет ведущие U+FEFF (StripBOM оригинала).
std::string stripBom(std::string_view s);

/// Разбор целого по правилам StrToInt из PascalABC.NET: пробелы по краям
/// допускаются, знак тоже, всё прочее и переполнение — неудача.
bool parseNetInt(std::string_view s, int& value);

}  // namespace tjtm
