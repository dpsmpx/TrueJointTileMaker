#include "core/anim.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/imageops.h"
#include "core/model.h"
#include "core/pmath.h"

namespace tjtm {

void sheetGrid(int n, const SheetOptions& o, int& cols, int& rows) {
    n = std::max(1, n);
    if (o.layout == SHEET_COLUMN) {
        cols = 1;
    } else if (o.layout == SHEET_GRID) {
        cols = o.columns > 0 ? o.columns
                             : static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    } else {
        cols = n;
    }
    cols = clampI(cols, 1, n);
    rows = (n + cols - 1) / cols;
}

SpriteSheet buildSheet(const std::vector<AnimFrame>& frames, const SheetOptions& o) {
    SpriteSheet s;
    if (frames.empty()) return s;
    const int scale = clampI(o.scale, 1, 16);
    const int margin = std::max(0, o.margin);
    const int spacing = std::max(0, o.spacing);
    const int fw = frames.front().image.w * scale;
    const int fh = frames.front().image.h * scale;
    int cols = 1;
    int rows = 1;
    sheetGrid(static_cast<int>(frames.size()), o, cols, rows);
    s.image = Grid<Color>(margin * 2 + cols * fw + (cols - 1) * spacing,
                          margin * 2 + rows * fh + (rows - 1) * spacing, argb(0, 0, 0, 0));
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const int c = static_cast<int>(i) % cols;
        const int r = static_cast<int>(i) / cols;
        SheetCell cell{margin + c * (fw + spacing), margin + r * (fh + spacing), fw, fh,
                       frames[i].durationMs};
        const Grid<Color>& src = frames[i].image;
        const Grid<Color> big =
            scale == 1 ? src : img::scaleNearest(src, src.w * scale, src.h * scale);
        for (int y = 0; y < std::min(fh, big.h); ++y) {
            for (int x = 0; x < std::min(fw, big.w); ++x)
                s.image.at(cell.x + x, cell.y + y) = big.at(x, y);
        }
        s.cells.push_back(cell);
    }
    return s;
}

std::vector<AnimFrame> scaleFrames(const std::vector<AnimFrame>& frames, int scale) {
    if (scale <= 1) return frames;
    std::vector<AnimFrame> out;
    out.reserve(frames.size());
    for (const AnimFrame& f : frames)
        out.push_back(AnimFrame{img::scaleNearest(f.image, f.image.w * scale, f.image.h * scale),
                                f.durationMs});
    return out;
}

std::string jsonString(const std::string& s) {
    std::string o = "\"";
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else {
                    // UTF-8 идёт как есть: JSON его допускает.
                    o += ch;
                }
        }
    }
    return o + "\"";
}

std::string sheetJson(const SpriteSheet& s, const std::string& imageFile,
                      const std::string& frameName, const std::vector<AnimTag>& tags) {
    // Ключи идут через jsonString: в тексте нет экранированных кавычек, и
    // любое имя кадра или метки выходит правильной строкой JSON.
    auto kv = [](const char* key, const std::string& value) {
        return jsonString(key) + ": " + value;
    };
    auto num = [](int v) { return std::to_string(v); };
    auto size = [&](int w, int h) {
        return "{ " + kv("w", num(w)) + ", " + kv("h", num(h)) + " }";
    };
    auto rect = [&](int x, int y, int w, int h) {
        return "{ " + kv("x", num(x)) + ", " + kv("y", num(y)) + ", " + kv("w", num(w)) + ", " +
               kv("h", num(h)) + " }";
    };
    std::string o = "{ " + kv("frames", "[") + "\n";
    for (std::size_t i = 0; i < s.cells.size(); ++i) {
        const SheetCell& c = s.cells[i];
        o += "  {\n";
        o += "   " + kv("filename", jsonString(frameName + " " + std::to_string(i))) + ",\n";
        o += "   " + kv("frame", rect(c.x, c.y, c.w, c.h)) + ",\n";
        o += "   " + kv("rotated", "false") + ",\n";
        o += "   " + kv("trimmed", "false") + ",\n";
        o += "   " + kv("spriteSourceSize", rect(0, 0, c.w, c.h)) + ",\n";
        o += "   " + kv("sourceSize", size(c.w, c.h)) + ",\n";
        o += "   " + kv("duration", num(c.durationMs)) + "\n";
        o += i + 1 < s.cells.size() ? "  },\n" : "  }\n";
    }
    o += " ],\n";
    o += " " + kv("meta", "{") + "\n";
    o += "  " + kv("app", jsonString(kAppTitle)) + ",\n";
    o += "  " + kv("version", jsonString(kAppVersion)) + ",\n";
    o += "  " + kv("image", jsonString(imageFile)) + ",\n";
    o += "  " + kv("format", jsonString("RGBA8888")) + ",\n";
    o += "  " + kv("size", size(s.image.w, s.image.h)) + ",\n";
    o += "  " + kv("scale", jsonString("1")) + ",\n";
    o += "  " + kv("frameTags", "[");
    for (std::size_t i = 0; i < tags.size(); ++i) {
        const AnimTag& t = tags[i];
        const char* dir = t.direction == TAG_REVERSE    ? "reverse"
                          : t.direction == TAG_PINGPONG ? "pingpong"
                                                        : "forward";
        o += i == 0 ? "\n" : ",\n";
        o += "   { " + kv("name", jsonString(t.name)) + ", " + kv("from", num(t.from)) + ", " +
             kv("to", num(t.to)) + ", " + kv("direction", jsonString(dir)) + " }";
    }
    o += tags.empty() ? "]\n" : "\n  ]\n";
    o += " }\n}\n";
    return o;
}

}  // namespace tjtm
