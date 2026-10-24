// Файлы и пути. Все обращения к файловой системе идут через эти функции:
// путь — строка UTF-8 на любой системе, ошибка не бросает исключение.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tjtm {

/// Весь файл целиком; nullopt, если прочитать не удалось.
std::optional<std::string> readFile(const std::string& path);
/// Пишет файл целиком, заменяя прежний. false — не удалось.
bool writeFile(const std::string& path, std::string_view data);

bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
bool makeDirs(const std::string& path);

/// Path.IsPathRooted.
bool isPathRooted(const std::string& p);
/// Path.Combine: второй путь, если он абсолютный, иначе склейка.
std::string pathCombine(const std::string& a, const std::string& b);
/// Path.GetDirectoryName: всё до последнего разделителя.
std::string pathDirName(const std::string& p);
/// Path.GetFileName.
std::string pathFileName(const std::string& p);
/// Path.GetFileNameWithoutExtension.
std::string pathStem(const std::string& p);
/// Расширение с точкой, в нижнем регистре; пусто, если его нет.
std::string pathExtLower(const std::string& p);

/// Файлы каталога (только имена), отсортированные по возрастанию.
std::vector<std::string> listFiles(const std::string& dir);

}  // namespace tjtm
