// Поля объекта карты (этап 10): хвост строки объекта, разобранный по
// грамматике src/world.cpp игры, — чтобы править его формой, а не строкой.
//
//   npc    <житель>
//   item   <предмет> [количество]
//   exit   <локация> <x цели> <y цели> [key= quest= counter= deny=…]
//   spawn  <враг> <сколько> <радиус>
//   chest  <золото> <ключ|-> [предмет:количество …]
//   note   <записка>
//   bed
//   sign   <текст до конца строки>
//
// Слова сверх грамматики игра у части объектов пропускает молча; здесь они
// не теряются, а лежат в последнем поле «Прочее». Если формой ничего не
// поменяли, строка остаётся прежней байт в байт: её пересобирают только
// тогда, когда поле правда изменилось.
#pragma once

#include <string>
#include <vector>

namespace tjtm::mapobj {

struct Field {
    std::string label;
    std::string value;
    /// Целое число.
    bool number = false;
    /// Может быть пустым.
    bool optional = false;
    /// Остаток строки: пробелы внутри — часть значения.
    bool tail = false;
    /// Подсказка под полем.
    std::string hint;
};

/// Поля объекта kind из хвоста rest.
std::vector<Field> fieldsOf(const std::string& kind, const std::string& rest);
/// Хвост строки из полей: через пробел, пустые необязательные пропускаются.
std::string restOf(const std::vector<Field>& fields);
/// Что не так в полях по правилам игры; пусто — всё верно.
std::string checkFields(const std::string& kind, const std::vector<Field>& fields);

}  // namespace tjtm::mapobj
