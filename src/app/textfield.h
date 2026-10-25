// Однострочное поле ввода: общее у askText и форм (объект карты, этап 10).
#pragma once

#include <algorithm>
#include <string>

#include "platform/platform.h"

namespace tjtm {

/// Однострочное поле: каретка, выделение мышью и Shift, слова по Ctrl,
/// буфер обмена, раскладки и IME. Оригинал ради этого клал поверх окна
/// настоящий TextBox: строки объектов карты длинные, и их переносят между
/// локациями целиком.
struct TextField {
    std::u32string text;
    std::size_t caret = 0;
    std::size_t anchor = 0;  // начало выделения; == caret — выделения нет
    int scroll = 0;

    [[nodiscard]] bool hasSel() const { return anchor != caret; }
    [[nodiscard]] std::size_t selLo() const { return std::min(anchor, caret); }
    [[nodiscard]] std::size_t selHi() const { return std::max(anchor, caret); }

    void eraseSel() {
        if (!hasSel()) return;
        text.erase(selLo(), selHi() - selLo());
        caret = anchor = selLo();
    }
    void insert(const std::u32string& s) {
        eraseSel();
        std::u32string clean;
        for (char32_t c : s) {
            if (c != U'\n' && c != U'\r') clean.push_back(c == U'\t' ? U' ' : c);
        }
        text.insert(caret, clean);
        caret += clean.size();
        anchor = caret;
    }
    static bool isWord(char32_t c) { return c != U' '; }
    [[nodiscard]] std::size_t wordLeft(std::size_t p) const {
        while (p > 0 && !isWord(text[p - 1]))
            --p;
        while (p > 0 && isWord(text[p - 1]))
            --p;
        return p;
    }
    [[nodiscard]] std::size_t wordRight(std::size_t p) const {
        while (p < text.size() && isWord(text[p]))
            ++p;
        while (p < text.size() && !isWord(text[p]))
            ++p;
        return p;
    }
};

bool editTextField(TextField& tf, const KeyPress& kp, Platform& pf);

}  // namespace tjtm
