// Лента слоёв и кадров под холстом (этап 4 RoadToProLevel).
//
// Устроена как у Aseprite: слева слои (верхний — сверху), правее кадры,
// в клетке — есть ли у слоя рисунок в этом кадре. Над номерами кадров —
// полоса меток анимации. Лента — часть главного окна, а не экран: рисуют,
// листают кадры и проигрывают, не закрывая её. Пока лента видна, холст
// ниже на её высоту: m.H — высота холста, а не окна.

#include <algorithm>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int TL_H = 150;
constexpr int TL_BAR_H = 30;
constexpr int TL_TAG_H = 16;
constexpr int TL_HEAD_H = 18;
constexpr int TL_ROW_H = 20;
/// Столбец слоёв: глаз, замок, имя.
constexpr int TL_NAME_W = 150;
constexpr int TL_EYE_W = 20;
constexpr int TL_LOCK_W = 20;
constexpr int TL_CELL_W = 26;
/// Место под знак вида слоя перед именем.
constexpr int TL_MARK_W = 12;
constexpr int TL_Y0 = CANVAS_H - TL_H;
constexpr int TL_TAG_Y = TL_Y0 + TL_BAR_H;
constexpr int TL_HEAD_Y = TL_TAG_Y + TL_TAG_H;
constexpr int TL_ROWS_Y = TL_HEAD_Y + TL_HEAD_H;
constexpr int TL_VIS_ROWS = (CANVAS_H - TL_ROWS_Y) / TL_ROW_H;
constexpr int TL_VIS_FRAMES = (CANVAS_W - TL_NAME_W) / TL_CELL_W;

enum TlButton {
    TB_PLAY,
    TB_ADD_FRAME,
    TB_COPY_FRAME,
    TB_DEL_FRAME,
    TB_ADD_LAYER,
    TB_DEL_LAYER,
    TB_ONION,
    TB_TAG,
    TB_MORE,
    TB_COUNT
};
constexpr int kTbW[TB_COUNT] = {30, 62, 56, 62, 62, 62, 42, 56, 30};
constexpr int TB_Y = TL_Y0 + 3;
constexpr int TB_H = 24;

int tbX(int b) {
    int x = 6;
    for (int k = 0; k < b; ++k)
        x += kTbW[k] + 4;
    return x;
}

bool blankGrid(const Grid<Color>& g) {
    return std::all_of(g.d.begin(), g.d.end(), [](Color c) { return c.a == 0; });
}

}  // namespace

void Editor::applyLayout() {
    const int h = timelineShown() ? TL_Y0 : CANVAS_H;
    const int r = m.rulersOn ? RULER_SIZE : 0;
    if (m.H == h && m.X0 == r && m.Y0 == r) return;
    // Тайл, что влезал в холст целиком, влезает и в укороченный; иначе
    // масштаб остаётся прежним, а вид лишь поджимается к краям.
    const bool fitted =
        m.docW() * m.pixelSize <= m.W - m.X0 && m.docH() * m.pixelSize <= m.H - m.Y0;
    m.H = h;
    m.X0 = r;
    m.Y0 = r;
    if (fitted && (m.docW() * m.pixelSize > m.W - m.X0 || m.docH() * m.pixelSize > m.H - m.Y0)) {
        m.fitScale();
    } else {
        m.clampView();
    }
    m.overDirty = true;
    m.needRepaint = true;
}

void Editor::drawTimeline() {
    if (!timelineShown()) return;
    const SpriteDoc& d = m.doc;
    const int nf = d.frameCount();
    const int nl = d.layerCount();
    if (nf == 0 || nl == 0) return;
    const int af = clampI(m.activeFrame, 0, nf - 1);
    const int al = clampI(m.activeLayer, 0, nl - 1);
    // Активная клетка всегда на виду, но только когда она сменилась: иначе
    // ролик не мог бы отлистать ленту от неё.
    if (af != tlSeenFrame_) {
        if (af < tlFirstFrame_) tlFirstFrame_ = af;
        if (af >= tlFirstFrame_ + TL_VIS_FRAMES) tlFirstFrame_ = af - TL_VIS_FRAMES + 1;
        tlSeenFrame_ = af;
    }
    // Строки ленты — слои сверху вниз без детей свёрнутых групп.
    const std::vector<int> rows = timelineRows();
    const int nrows = static_cast<int>(rows.size());
    const auto found = std::find(rows.begin(), rows.end(), al);
    const int activeRow = found == rows.end() ? -1 : static_cast<int>(found - rows.begin());
    if (al != tlSeenLayer_ && activeRow >= 0) {
        if (activeRow < tlFirstRow_) tlFirstRow_ = activeRow;
        if (activeRow >= tlFirstRow_ + TL_VIS_ROWS) tlFirstRow_ = activeRow - TL_VIS_ROWS + 1;
        tlSeenLayer_ = al;
    }
    tlFirstFrame_ = clampI(tlFirstFrame_, 0, std::max(0, nf - TL_VIS_FRAMES));
    tlFirstRow_ = clampI(tlFirstRow_, 0, std::max(0, nrows - TL_VIS_ROWS));

    fb_.resetClip();
    fb_.fillRect(0, TL_Y0, CANVAS_W, CANVAS_H, cPanel);
    fb_.fillRect(0, TL_Y0, CANVAS_W, TL_Y0 + 1, cBorder);

    // --- кнопки ---
    for (int b = 0; b < TB_COUNT; ++b) {
        const int x0 = tbX(b);
        const int x1 = x0 + kTbW[b];
        const bool on = (b == TB_PLAY && playing_) || (b == TB_ONION && m.onionSkin);
        const bool enabled = (b != TB_DEL_FRAME || nf > 1) && (b != TB_DEL_LAYER || nl > 1) &&
                             (b != TB_PLAY || nf > 1);
        std::string label;
        switch (b) {
            case TB_PLAY: label = playing_ ? "■" : "►"; break;
            case TB_ADD_FRAME: label = "+ Кадр"; break;
            case TB_COPY_FRAME: label = "Копия"; break;
            case TB_DEL_FRAME: label = "− Кадр"; break;
            case TB_ADD_LAYER: label = "+ Слой"; break;
            case TB_DEL_LAYER: label = "− Слой"; break;
            case TB_ONION: label = "Лук"; break;
            case TB_TAG: label = "Метка"; break;
            default: label = "…"; break;
        }
        if (active.modern) {
            const bool over = enabled && mouseX() >= x0 && mouseX() < x1 && mouseY() >= TB_Y &&
                              mouseY() < TB_Y + TB_H;
            drawButtonModern(x0, TB_Y, kTbW[b], TB_H, label, enabled, on, over, false,
                             Btn::Outlined);
            continue;
        }
        fb_.fillRoundRect(x0, TB_Y, x1, TB_Y + TB_H, 5, 5, on ? cAccent : cBorder);
        fb_.fillRoundRect(x0 + 1, TB_Y + 1, x1 - 1, TB_Y + TB_H - 1, 5, 5,
                          on ? cHover : (enabled ? cBack : cFace));
        textCentered(x0, TB_Y, x1, TB_Y + TB_H, label, enabled ? cText : cTextDim);
    }
    const std::string info = std::to_string(af + 1) + " / " + std::to_string(nf) + "   " +
                             std::to_string(d.frames[static_cast<std::size_t>(af)].durationMs) +
                             " мс";
    textCentered(tbX(TB_COUNT), TL_Y0, CANVAS_W - 4, TL_Y0 + TL_BAR_H, info, cTextDim);

    // --- метки ---
    fb_.fillRect(0, TL_TAG_Y, CANVAS_W, TL_HEAD_Y, cWindow);
    const bool tagOk = playTag_ >= 0 && playTag_ < static_cast<int>(d.tags.size());
    fb_.setClip(gfx::Rect{0, TL_TAG_Y, TL_NAME_W - 2, TL_HEAD_Y});
    textOut(6, TL_TAG_Y + (TL_TAG_H - textHeight()) / 2,
            tagOk ? "► " + d.tags[static_cast<std::size_t>(playTag_)].name : "► все кадры",
            cTextDim);
    fb_.resetClip();
    for (std::size_t i = 0; i < d.tags.size(); ++i) {
        const AnimTag& t = d.tags[i];
        const int a = std::max(t.from, tlFirstFrame_);
        const int b = std::min(t.to, tlFirstFrame_ + TL_VIS_FRAMES - 1);
        if (a > b) continue;
        const int x0 = TL_NAME_W + (a - tlFirstFrame_) * TL_CELL_W + 1;
        const int x1 = TL_NAME_W + (b - tlFirstFrame_ + 1) * TL_CELL_W - 1;
        fb_.fillRect(x0, TL_TAG_Y + 1, x1, TL_HEAD_Y - 1, active.tag[i % active.tag.size()]);
        if (static_cast<int>(i) == playTag_)
            fb_.frameRect(x0, TL_TAG_Y + 1, x1, TL_HEAD_Y - 1, cAccent);
        fb_.setClip(gfx::Rect{x0 + 1, TL_TAG_Y, x1 - 1, TL_HEAD_Y});
        textOut(x0 + 3, TL_TAG_Y + (TL_TAG_H - textHeight()) / 2, t.name, cText);
        fb_.resetClip();
    }

    // --- номера кадров ---
    for (int k = 0; k < TL_VIS_FRAMES; ++k) {
        const int f = tlFirstFrame_ + k;
        if (f >= nf) break;
        const int x = TL_NAME_W + k * TL_CELL_W;
        if (f == af) fb_.fillRect(x, TL_HEAD_Y, x + TL_CELL_W, TL_ROWS_Y, cHover);
        textCentered(x, TL_HEAD_Y, x + TL_CELL_W, TL_ROWS_Y, std::to_string(f + 1),
                     f == af ? cAccent : cTextDim);
    }

    // --- слои и ячейки ---
    // Активная ячейка живёт в m.tile, и в таблице её картинка может
    // отставать: пустоту проверяем по самой картинке.
    const bool activeHas = !blankGrid(m.tile);
    for (int r = 0; r < TL_VIS_ROWS; ++r) {
        const int row = tlFirstRow_ + r;
        if (row >= nrows) break;
        const int l = rows[static_cast<std::size_t>(row)];
        const Layer& layer = d.layers[static_cast<std::size_t>(l)];
        const bool activeL = l == al;
        const int y = TL_ROWS_Y + r * TL_ROW_H;
        const int cy = y + TL_ROW_H / 2;
        fb_.fillRect(0, y, TL_NAME_W, y + TL_ROW_H, activeL ? cHover : cBack);

        // Глаз: открытый — слой виден, черта — скрыт.
        const int ex = TL_EYE_W / 2;
        if (layer.visible) {
            fb_.fillRoundRect(ex - 7, cy - 4, ex + 8, cy + 5, 10, 8, cText);
            fb_.fillRoundRect(ex - 6, cy - 3, ex + 7, cy + 4, 9, 6, cBack);
            fb_.fillRoundRect(ex - 2, cy - 2, ex + 3, cy + 3, 5, 5, cText);
        } else {
            fb_.fillRect(ex - 6, cy, ex + 7, cy + 1, cTextDim);
        }
        // Замок: запертый — тёмный, открытый — бледный контур.
        const int lx = TL_EYE_W + TL_LOCK_W / 2;
        const Color lc = layer.locked ? cText : cBorder;
        fb_.frameRect(lx - 3, cy - 6, lx + 4, cy, lc);
        fb_.fillRect(lx - 5, cy - 1, lx + 6, cy + 6, lc);

        std::string name = layer.name;
        if (layer.opacity < 255)
            name += "  " + std::to_string(pround(layer.opacity * 100.0 / 255)) + "%";
        // Дерево — сдвигом по глубине; перед именем — знак вида слоя.
        const int nx = TL_EYE_W + TL_LOCK_W + 2 + std::min(layer.depth, 6) * 8;
        fb_.setClip(gfx::Rect{TL_EYE_W + TL_LOCK_W + 2, y, TL_NAME_W - 4, y + TL_ROW_H});
        drawLayerKindMark(nx, cy, layer);
        textOut(nx + TL_MARK_W, y + (TL_ROW_H - textHeight()) / 2, name,
                layer.visible ? cText : cTextDim);
        fb_.resetClip();

        for (int k = 0; k < TL_VIS_FRAMES; ++k) {
            const int f = tlFirstFrame_ + k;
            const int x = TL_NAME_W + k * TL_CELL_W;
            if (f >= nf) {
                fb_.fillRect(x, y, CANVAS_W, y + TL_ROW_H, cPanel);
                break;
            }
            fb_.fillRect(x, y, x + TL_CELL_W, y + TL_ROW_H, (f == af || activeL) ? cHover : cBack);
            fb_.fillRect(x + TL_CELL_W - 1, y, x + TL_CELL_W, y + TL_ROW_H, cLine);
            const bool isActive = activeL && f == af;
            const bool has = isActive ? activeHas : d.cel(l, f) != nullptr;
            const int cx = x + TL_CELL_W / 2;
            if (layer.kind == LK_GROUP) {
                // У группы своих ячеек нет: полоса через кадры.
                fb_.fillRect(x, cy - 1, x + TL_CELL_W, cy + 1, cBorder);
            } else if (has) {
                fb_.fillRoundRect(cx - 4, cy - 4, cx + 5, cy + 5, 9, 9,
                                  layer.visible ? cText : cTextDim);
            } else {
                fb_.fillRoundRect(cx - 4, cy - 4, cx + 5, cy + 5, 9, 9, cBorder);
                fb_.fillRoundRect(cx - 3, cy - 3, cx + 4, cy + 4, 7, 7,
                                  (f == af || activeL) ? cHover : cBack);
            }
            // Общая с соседним кадром картинка (копия кадра) — перемычкой,
            // как связанные ячейки Aseprite: правка любой из них разведёт их.
            // У активной ячейки указатель в таблице может отставать от
            // картинки — её связь не рисуется.
            const bool prevActive = activeL && f - 1 == af;
            if (k > 0 && has && !isActive && !prevActive && d.cel(l, f) == d.cel(l, f - 1))
                fb_.fillRect(cx - TL_CELL_W + 5, cy - 1, cx - 4, cy + 1,
                             layer.visible ? cText : cTextDim);
            if (isActive) fb_.frameRect(x + 1, y + 1, x + TL_CELL_W - 1, y + TL_ROW_H - 1, cAccent);
        }
        fb_.fillRect(0, y + TL_ROW_H - 1, CANVAS_W, y + TL_ROW_H, cLine);
    }
    fb_.fillRect(TL_NAME_W - 1, TL_TAG_Y, TL_NAME_W, CANVAS_H, cBorder);

    // Полоски прокрутки: сколько кадров и слоёв за краем ленты.
    if (nf > TL_VIS_FRAMES) {
        const int span = TL_VIS_FRAMES * TL_CELL_W;
        const int bx = TL_NAME_W + tlFirstFrame_ * span / nf;
        fb_.fillRect(bx, CANVAS_H - 3, bx + std::max(8, TL_VIS_FRAMES * span / nf), CANVAS_H - 1,
                     cTextDim);
    }
    if (nrows > TL_VIS_ROWS) {
        const int span = TL_VIS_ROWS * TL_ROW_H;
        const int by = TL_ROWS_Y + tlFirstRow_ * span / nrows;
        fb_.fillRect(TL_NAME_W - 4, by, TL_NAME_W - 2, by + std::max(6, TL_VIS_ROWS * span / nrows),
                     cTextDim);
    }
}

std::vector<int> Editor::timelineRows() const {
    std::vector<int> rows;
    for (int l = m.doc.layerCount() - 1; l >= 0; --l) {
        if (m.layerRowVisible(l)) rows.push_back(l);
    }
    return rows;
}

void Editor::drawLayerKindMark(int x, int cy, const Layer& layer) {
    const Color c = layer.visible ? cText : cTextDim;
    switch (layer.kind) {
        case LK_GROUP:
            // Треугольник: вершиной вправо — свёрнута, вниз — открыта.
            for (int k = 0; k < 4; ++k) {
                if (layer.collapsed) {
                    fb_.fillRect(x + 2 + k, cy - 3 + k, x + 3 + k, cy + 4 - k, c);
                } else {
                    fb_.fillRect(x + 1 + k, cy - 2 + k, x + 8 - k, cy - 1 + k, c);
                }
            }
            break;
        case LK_VECTOR:
            // Ромб — опорная точка кривой.
            for (int k = 0; k < 4; ++k) {
                fb_.fillRect(x + 4 - k, cy - 3 + k, x + 5 - k, cy - 2 + k, c);
                fb_.fillRect(x + 4 + k, cy - 3 + k, x + 5 + k, cy - 2 + k, c);
                fb_.fillRect(x + 4 - k, cy + 3 - k, x + 5 - k, cy + 4 - k, c);
                fb_.fillRect(x + 4 + k, cy + 3 - k, x + 5 + k, cy + 4 - k, c);
            }
            break;
        case LK_TILEMAP:
            // Решётка 2x2 — клетки тайлов.
            fb_.frameRect(x + 1, cy - 4, x + 10, cy + 5, c);
            fb_.fillRect(x + 5, cy - 4, x + 6, cy + 5, c);
            fb_.fillRect(x + 1, cy, x + 10, cy + 1, c);
            break;
        case LK_REFERENCE:
            // Рамка с горкой — картинка, которую срисовывают.
            fb_.frameRect(x + 1, cy - 4, x + 10, cy + 5, c);
            for (int k = 0; k < 3; ++k)
                fb_.fillRect(x + 3 + k, cy + 2 - k, x + 8 - k, cy + 3 - k, c);
            break;
        default: break;
    }
}

int Editor::timelineButtonAt(int x, int y) const {
    if (y < TB_Y || y >= TB_Y + TB_H) return -1;
    for (int b = 0; b < TB_COUNT; ++b) {
        if (x >= tbX(b) && x < tbX(b) + kTbW[b]) return b;
    }
    return -1;
}

void Editor::handleTimelineClick() {
    const int x = frameClickX_;
    const int y = frameClickY_;
    const bool right = frameClickButton_ == MB_RIGHT;
    const int nf = m.doc.frameCount();
    const int frameAt = x >= TL_NAME_W ? tlFirstFrame_ + (x - TL_NAME_W) / TL_CELL_W : -1;
    const bool frameOk = frameAt >= 0 && frameAt < nf;
    m.needRepaint = true;

    if (y < TL_TAG_Y) {
        for (int b = 0; b < TB_COUNT; ++b) {
            if (x < tbX(b) || x >= tbX(b) + kTbW[b] || y < TB_Y || y >= TB_Y + TB_H) continue;
            if (b != TB_PLAY) stopPlayback();
            switch (b) {
                case TB_PLAY: togglePlayback(); break;
                case TB_ADD_FRAME: m.addFrame(false); break;
                case TB_COPY_FRAME: m.addFrame(true); break;
                case TB_DEL_FRAME: m.deleteFrame(); break;
                case TB_ADD_LAYER: m.addLayer(); break;
                case TB_DEL_LAYER: m.deleteLayer(); break;
                case TB_ONION:
                    m.onionSkin = !m.onionSkin;
                    m.tileBmpDirty = true;
                    break;
                case TB_TAG: editTag(m.tagOfFrame(m.activeFrame)); break;
                default: showAnimationScreen(); break;
            }
            return;
        }
        return;
    }

    if (y < TL_HEAD_Y) {
        // Метка под курсором: левая кнопка выбирает её для проигрывания
        // (повторный щелчок — снова все кадры), правая правит. Самая узкая
        // из накрывающих — она и видна поверх.
        int hit = -1;
        if (frameOk) {
            for (std::size_t i = 0; i < m.doc.tags.size(); ++i) {
                const AnimTag& t = m.doc.tags[i];
                if (frameAt >= t.from && frameAt <= t.to &&
                    (hit < 0 ||
                     t.to - t.from <= m.doc.tags[static_cast<std::size_t>(hit)].to -
                                          m.doc.tags[static_cast<std::size_t>(hit)].from))
                    hit = static_cast<int>(i);
            }
        }
        if (right) {
            editTag(hit);
        } else {
            playTag_ = (hit < 0 || hit == playTag_) ? -1 : hit;
            playPing_ = 0;
        }
        return;
    }

    if (y < TL_ROWS_Y) {
        if (!frameOk) return;
        stopPlayback();
        m.setActiveCel(m.activeLayer, frameAt);
        if (right) editFrameDuration(frameAt);
        return;
    }

    const std::vector<int> rows = timelineRows();
    const int row = tlFirstRow_ + (y - TL_ROWS_Y) / TL_ROW_H;
    if (row >= static_cast<int>(rows.size())) return;
    const int l = rows[static_cast<std::size_t>(row)];
    const Layer& layer = m.doc.layers[static_cast<std::size_t>(l)];
    // Треугольник группы сворачивает и разворачивает её.
    const int nx = TL_EYE_W + TL_LOCK_W + 2 + std::min(layer.depth, 6) * 8;
    if (layer.kind == LK_GROUP && x >= nx && x < nx + TL_MARK_W) {
        m.setLayerCollapsed(l, !layer.collapsed);
        return;
    }
    if (x < TL_EYE_W) {
        m.setLayerVisible(l, !layer.visible);
        return;
    }
    if (x < TL_EYE_W + TL_LOCK_W) {
        m.setLayerLocked(l, !layer.locked);
        return;
    }
    stopPlayback();
    if (x < TL_NAME_W) {
        const int now = pf_.milliseconds();
        const bool dbl = l == tlLastClickLayer_ && now - tlLastClickMs_ <= DOUBLE_CLICK_MS;
        tlLastClickMs_ = dbl ? -100000 : now;
        tlLastClickLayer_ = l;
        m.setActiveCel(l, m.activeFrame);
        if (right || dbl) renameLayerAsk(l);
        return;
    }
    if (frameOk) {
        m.setActiveCel(l, frameAt);
        if (right) editFrameDuration(frameAt);
    }
}

void Editor::timelineWheel(int wheel) {
    // Над слоями ролик листает слои, над кадрами — кадры.
    if (mouseX() < TL_NAME_W) {
        tlFirstRow_ = clampI(tlFirstRow_ - wheel, 0, std::max(0, m.doc.layerCount() - TL_VIS_ROWS));
    } else {
        tlFirstFrame_ =
            clampI(tlFirstFrame_ - wheel, 0, std::max(0, m.doc.frameCount() - TL_VIS_FRAMES));
    }
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Проигрывание
// ---------------------------------------------------------------------------

void Editor::togglePlayback() {
    if (playing_) {
        stopPlayback();
        return;
    }
    if (m.docMode != DOC_TILE || m.doc.frameCount() < 2) return;
    if (playTag_ >= static_cast<int>(m.doc.tags.size())) playTag_ = -1;
    playing_ = true;
    playPing_ = 0;
    // Кадр вне проигрываемой метки — проигрывание с её начала.
    if (playTag_ >= 0) {
        const AnimTag& t = m.doc.tags[static_cast<std::size_t>(playTag_)];
        if (m.activeFrame < t.from || m.activeFrame > t.to)
            m.setActiveCel(m.activeLayer, t.direction == TAG_REVERSE ? t.to : t.from);
    }
    playNextMs_ =
        pf_.milliseconds() + m.doc.frames[static_cast<std::size_t>(m.activeFrame)].durationMs;
    m.needRepaint = true;
}

void Editor::stopPlayback() {
    if (!playing_) return;
    playing_ = false;
    m.needRepaint = true;
}

void Editor::stepPlayback() {
    if (!playing_) return;
    if (m.docMode != DOC_TILE || m.doc.frameCount() < 2) {
        stopPlayback();
        return;
    }
    const int now = pf_.milliseconds();
    if (now < playNextMs_) return;
    const int f = m.nextPlayFrame(m.activeFrame, playTag_, playPing_);
    m.setActiveCel(m.activeLayer, f);
    const int dur = m.doc.frames[static_cast<std::size_t>(f)].durationMs;
    // Отставший таймер не догоняет пропущенное пачкой кадров: следующий
    // кадр — через свою длительность от «сейчас».
    playNextMs_ += dur;
    if (playNextMs_ <= now) playNextMs_ = now + dur;
}

// ---------------------------------------------------------------------------
// Свойства слоя, кадра, метки
// ---------------------------------------------------------------------------

void Editor::renameLayerAsk(int l) {
    if (l < 0 || l >= m.doc.layerCount()) return;
    std::string name = m.doc.layers[static_cast<std::size_t>(l)].name;
    if (askText("Имя слоя",
                "Слой " + std::to_string(l + 1) + " из " + std::to_string(m.doc.layerCount()), name,
                name) &&
        !netTrim(name).empty())
        m.renameLayer(l, netTrim(name));
}

void Editor::editFrameDuration(int f) {
    if (f < 0 || f >= m.doc.frameCount()) return;
    int ms = m.doc.frames[static_cast<std::size_t>(f)].durationMs;
    if (askNumbers("Длительность кадра " + std::to_string(f + 1),
                   "Миллисекунды; 100 мс — десять кадров в секунду",
                   {{"Длительность, мс", &ms, 10, 60000, 10}}, "Применить", nullptr))
        m.setFrameDuration(f, ms);
}

void Editor::editTag(int i) {
    stopPlayback();
    const int nf = m.doc.frameCount();
    const bool fresh = i < 0 || i >= static_cast<int>(m.doc.tags.size());
    AnimTag t = fresh ? AnimTag{"Метка " + std::to_string(m.doc.tags.size() + 1), m.activeFrame,
                                m.activeFrame, TAG_FORWARD}
                      : m.doc.tags[static_cast<std::size_t>(i)];
    int from = t.from + 1;
    int to = t.to + 1;
    bool done = false;
    bool ok = false;
    bool erase = false;
    dropInput();
    pf_.input().keys.clear();
    auto stepper = [&](int y, const std::string& label, int& v) {
        uiLabel(40, y, 180, 34, label, cText);
        if (uiButton(282, y, 46, 34, "−1", v > 1)) --v;
        fb_.fillRoundRect(334, y, 434, y + 34, 6, 6, cFace);
        uiLabel(334, y, 100, 34, std::to_string(v), cText);
        if (uiButton(440, y, 46, 34, "+1", v < nf)) ++v;
    };
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 30, WIN_W, 66, fresh ? "Новая метка анимации" : "Метка анимации", cText);
        uiLabel(0, 70, WIN_W, 26, "Именованный отрезок кадров: бег, прыжок, удар", cTextDim);
        uiLabel(40, 120, 180, 34, "Имя", cText);
        fb_.fillRoundRect(230, 120, 540, 154, 6, 6, cFace);
        uiLabel(230, 120, 310, 34, fitText(t.name, 290), cText);
        if (uiButton(548, 120, 120, 34, "Изменить…", true)) {
            std::string name = t.name;
            if (askText("Имя метки", "Его увидят движок и описание листа спрайтов", name, name) &&
                !netTrim(name).empty())
                t.name = netTrim(name);
        }
        stepper(170, "С кадра", from);
        stepper(220, "По кадр", to);
        uiLabel(40, 270, 180, 34, "Проигрывать", cText);
        static const char* kDir[3] = {"Вперёд", "Назад", "Туда и обратно"};
        for (int k = 0; k < 3; ++k) {
            if (uiToggle(230 + k * 150, 270, 144, 34, kDir[k], t.direction == k)) t.direction = k;
        }
        const int by = 350;
        if (uiButton(WIN_W / 2 - 270, by, 170, 38, fresh ? "Создать" : "Применить", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 - 85, by, 170, 38, "Удалить метку", !fresh)) {
            erase = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 100, by, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER) {
            ok = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    if (erase) {
        m.deleteTag(i);
        if (playTag_ == i) playTag_ = -1;
        if (playTag_ > i) --playTag_;
        return;
    }
    if (!ok) return;
    t.from = from - 1;
    t.to = to - 1;
    if (fresh) {
        playTag_ = m.addTag(t.name, t.from, t.to, t.direction);
    } else {
        m.setTag(i, t);
    }
}

// ---------------------------------------------------------------------------
// Экран «Слои, кадры и экспорт»
// ---------------------------------------------------------------------------

void Editor::showAnimationScreen() {
    stopPlayback();
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    // Четыре колонки: слой, его вид и место в дереве, кадр с метками,
    // просмотр с выгрузкой.
    constexpr int CW = 178;
    constexpr int BH = 28;
    constexpr int ST = 31;
    const int c1 = 20;
    const int c2 = c1 + CW + 12;
    const int c3 = c2 + CW + 12;
    const int c4 = c3 + CW + 12;

    auto tileSizeAsk = [&](const std::string& title, int& tw, int& th) {
        tw = m.majorGridW > 0 ? m.majorGridW : std::min(16, m.tile.w);
        th = m.majorGridH > 0 ? m.majorGridH : std::min(16, m.tile.h);
        return askTwoNumbers(title, "Размер тайла в точках; клетки идут от левого верхнего угла",
                             "Ширина тайла", tw, 1, m.tile.w, "Высота тайла", th, 1, m.tile.h,
                             "Создать", nullptr);
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 8, WIN_W, 34, "Слои, кадры и экспорт", cText);
        const bool tile = m.docMode == DOC_TILE;
        const int nl = m.doc.layerCount();
        const int nf = m.doc.frameCount();
        const int al = m.activeLayer;
        const int af = m.activeFrame;
        // Копия: кнопки ниже меняют список слоёв посреди кадра.
        const Layer layer = m.doc.layers[static_cast<std::size_t>(al)];
        const int tag = m.tagOfFrame(af);
        const bool group = layer.kind == LK_GROUP;
        uiLabel(0, 32, WIN_W, 20,
                std::to_string(nl) + " сл. · " + std::to_string(nf) + " кадр. · " +
                    std::to_string(m.doc.tags.size()) + " меток   ·   " + std::to_string(m.tile.w) +
                    "×" + std::to_string(m.tile.h),
                cTextDim);
        // Как на экране изображения: кнопка делает своё и закрывает экран,
        // переключатели — нет.
        auto act = [&](int x, int y, const std::string& caption, bool enabled) {
            const bool hit = uiButton(x, y, CW, BH, caption, enabled);
            if (hit) done = true;
            return hit;
        };

        // --- слой ---
        int y = 78;
        uiLabelLeft(c1, y - 24, 20, fitText("Слой «" + layer.name + "»", CW), cTextDim);
        if (act(c1, y, "Переименовать…", tile)) renameLayerAsk(al);
        y += ST;
        if (act(c1, y, "Непрозрачность…", tile)) {
            int pct = pround(layer.opacity * 100.0 / 255);
            if (askNumbers("Непрозрачность слоя", "Сто — слой как есть, ноль — не виден",
                           {{"Непрозрачность, %", &pct, 0, 100}}, "Применить", nullptr))
                m.setLayerOpacity(al, pround(pct * 255.0 / 100));
        }
        y += ST;
        if (uiToggle(c1, y, CW, BH, "Виден", layer.visible)) m.setLayerVisible(al, !layer.visible);
        y += ST;
        if (uiToggle(c1, y, CW, BH, "Заперт", layer.locked)) m.setLayerLocked(al, !layer.locked);
        y += ST;
        if (act(c1, y, "Новый слой", tile)) m.addLayer();
        y += ST;
        if (act(c1, y, group ? "Копия группы" : "Копия слоя", tile)) m.duplicateLayer();
        y += ST;
        if (act(c1, y, group ? "Удалить группу" : "Удалить слой", tile && nl > 1)) m.deleteLayer();
        y += ST;
        if (act(c1, y, "Слить с нижним", tile && al > 0 && !group)) m.mergeLayerDown();
        y += ST;
        if (act(c1, y, "Выше   PgUp — выбрать", tile)) m.moveLayer(1);
        y += ST;
        if (act(c1, y, "Ниже   PgDn — выбрать", tile)) m.moveLayer(-1);

        // --- дерево и вид слоя (этап 6) ---
        y = 78;
        uiLabelLeft(c2, y - 24, 20, "Группы и вид слоя", cTextDim);
        if (act(c2, y, "Сгруппировать", tile)) m.groupLayer();
        y += ST;
        if (act(c2, y, "Распустить группу", tile && group)) m.ungroupLayer();
        y += ST;
        if (act(c2, y, "В группу над ним", tile)) m.indentLayer();
        y += ST;
        if (act(c2, y, "Из группы", tile && m.layerParent(al) >= 0)) m.outdentLayer();
        y += ST;
        if (act(c2, y, "Свести группу", tile && group)) m.flattenGroup();
        y += ST;
        if (act(c2, y, "Новый векторный слой", tile)) m.addVectorLayer();
        y += ST;
        if (act(c2, y, "Новая тайлкарта…", tile)) {
            int tw = 16;
            int th = 16;
            if (tileSizeAsk("Новая тайлкарта", tw, th)) m.addTilemapLayer(tw, th);
        }
        y += ST;
        if (act(c2, y, "Слой → тайлкарта…", tile && layer.kind == LK_RASTER)) {
            int tw = 16;
            int th = 16;
            if (tileSizeAsk("Слой в тайлкарту", tw, th)) m.convertLayerToTilemap(tw, th);
        }
        y += ST;
        if (act(c2, y, "Тайлы слоя…   Ctrl+T", tile && layer.kind == LK_TILEMAP))
            showTilePickerScreen();
        y += ST;
        if (act(c2, y, "Растрировать слой",
                tile && (layer.kind == LK_VECTOR || layer.kind == LK_TILEMAP)))
            m.rasterizeLayer();
        y += ST;
        if (act(c2, y, "Слой-образец…", tile)) {
            // Подложка для срисовки (этап 9): любая картинка, вписанная в
            // документ; не правится и в выгрузку не попадает.
            const std::string path = askOpenFile("Слой-образец");
            Grid<Color> img;
            if (!path.empty() && readImageGrid(path, img, false))
                m.addReferenceLayer(img, pathStem(path));
        }

        // --- кадр и метки ---
        y = 78;
        uiLabelLeft(c3, y - 24, 20, "Кадр " + std::to_string(af + 1) + " из " + std::to_string(nf),
                    cTextDim);
        if (act(c3, y, "Длительность…", tile)) editFrameDuration(af);
        y += ST;
        if (act(c3, y, "Длительность всех…", tile)) {
            int ms = m.doc.frames[static_cast<std::size_t>(af)].durationMs;
            if (askNumbers("Длительность всех кадров", "Миллисекунды на каждый кадр",
                           {{"Длительность, мс", &ms, 10, 60000, 10}}, "Применить", nullptr))
                m.setFramesDuration(0, nf - 1, ms);
        }
        y += ST;
        if (act(c3, y, "Новый кадр", tile)) m.addFrame(false);
        y += ST;
        if (act(c3, y, "Копия кадра", tile)) m.addFrame(true);
        y += ST;
        if (act(c3, y, "Удалить кадр", tile && nf > 1)) m.deleteFrame();
        y += ST;
        if (act(c3, y, "Левее", tile && af > 0)) m.moveFrame(-1);
        y += ST;
        if (act(c3, y, "Правее", tile && af < nf - 1)) m.moveFrame(1);
        y += ST + 8;
        uiLabelLeft(c3, y - 6, 20, "Метки анимации", cTextDim);
        y += 16;
        if (act(c3, y, "Новая метка…", tile)) editTag(-1);
        y += ST;
        if (act(c3, y, "Изменить метку кадра…", tile && tag >= 0)) editTag(tag);

        // --- просмотр и экспорт ---
        y = 78;
        uiLabelLeft(c4, y - 24, 20, "Просмотр", cTextDim);
        if (uiToggle(c4, y, CW, BH, "Лента кадров   J", timelineOn_)) timelineOn_ = !timelineOn_;
        y += ST;
        if (uiToggle(c4, y, CW, BH, "Луковая кожа   F3", m.onionSkin)) {
            m.onionSkin = !m.onionSkin;
            m.tileBmpDirty = true;
        }
        y += ST;
        if (act(c4, y, "Кадров до и после…", tile)) {
            int prev = m.onionPrev;
            int next = m.onionNext;
            askTwoNumbers("Луковая кожа", "Прошлые кадры — красноватые, будущие — синеватые",
                          "Кадров до", prev, 0, 8, "Кадров после", next, 0, 8, "Применить",
                          nullptr);
            m.onionPrev = prev;
            m.onionNext = next;
            m.tileBmpDirty = true;
        }
        y += ST + 8;
        uiLabelLeft(
            c4, y - 6, 20,
            playTag_ >= 0 && playTag_ < static_cast<int>(m.doc.tags.size())
                ? fitText("Экспорт: «" + m.doc.tags[static_cast<std::size_t>(playTag_)].name + "»",
                          CW)
                : "Экспорт: все кадры",
            cTextDim);
        y += 16;
        if (act(c4, y, "Лист спрайтов и JSON…", tile)) exportSheetAsk();
        y += ST;
        if (act(c4, y, "Анимация GIF…", tile)) exportAnimationAsk(true);
        y += ST;
        if (act(c4, y, "Анимация APNG…", tile)) exportAnimationAsk(false);
        y += ST;
        if (act(c4, y, "Каждый кадр в PNG…", tile)) exportFramesAsk();
        y += ST;
        if (act(c4, y, "Проект .tjt…", tile)) commandSaveAs();
        y += ST;
        // Документ, открытый из набора тайлов, помнит свой слот: первый
        // кадр сведённым PNG уходит туда, где его ищет игра.
        const std::string slot = m.metaValue("tileset.slot");
        if (act(c4, y,
                slot.empty() ? "В набор игры — слота нет" : fitText("В набор: " + slot, CW - 8),
                tile && !slot.empty()))
            exportToTilesetSlot();

        if (uiButton(c4, WIN_H - 50, CW, 34, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

}  // namespace tjtm
