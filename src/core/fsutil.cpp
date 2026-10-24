#include "core/fsutil.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace tjtm {

namespace fs = std::filesystem;

namespace {

// Путь в UTF-8 -> path. На Windows std::filesystem иначе принял бы строку
// за кодовую страницу ANSI и испортил бы кириллицу в именах.
fs::path toPath(const std::string& p) {
    std::string s = p;
#ifndef _WIN32
    // Config.txt, записанный на Windows, может держать пути с обратной
    // косой чертой; на остальных системах это тот же разделитель.
    std::replace(s.begin(), s.end(), '\\', '/');
#endif
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

std::string fromPath(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

bool isSep(char c) {
    return c == '/' || c == '\\';
}

}  // namespace

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream f(toPath(path), std::ios::binary);
    if (!f) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad()) return std::nullopt;
    return data;
}

bool writeFile(const std::string& path, std::string_view data) {
    std::ofstream f(toPath(path), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.close();
    return !f.fail();
}

bool fileExists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(toPath(path), ec);
}

bool dirExists(const std::string& path) {
    if (path.empty()) return false;
    std::error_code ec;
    return fs::is_directory(toPath(path), ec);
}

bool makeDirs(const std::string& path) {
    std::error_code ec;
    fs::create_directories(toPath(path), ec);
    return dirExists(path);
}

bool isPathRooted(const std::string& p) {
    if (p.empty()) return false;
    if (isSep(p[0])) return true;
    // «C:» и «C:\» — тоже корень, как в Path.IsPathRooted на Windows.
    return p.size() >= 2 && p[1] == ':' && std::isalpha(static_cast<unsigned char>(p[0])) != 0;
}

std::string pathCombine(const std::string& a, const std::string& b) {
    if (b.empty()) return a;
    if (a.empty() || isPathRooted(b)) return b;
    if (isSep(a.back())) return a + b;
#ifdef _WIN32
    return a + "\\" + b;
#else
    return a + "/" + b;
#endif
}

std::string pathDirName(const std::string& p) {
    const std::size_t k = p.find_last_of("/\\");
    if (k == std::string::npos) return {};
    if (k == 0) return p.substr(0, 1);
    return p.substr(0, k);
}

std::string pathFileName(const std::string& p) {
    const std::size_t k = p.find_last_of("/\\");
    return k == std::string::npos ? p : p.substr(k + 1);
}

std::string pathStem(const std::string& p) {
    std::string n = pathFileName(p);
    const std::size_t dot = n.find_last_of('.');
    if (dot != std::string::npos) n.resize(dot);
    return n;
}

std::string pathExtLower(const std::string& p) {
    const std::string n = pathFileName(p);
    const std::size_t dot = n.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = n.substr(dot);
    for (char& c : e)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

std::vector<std::string> listFiles(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    for (fs::directory_iterator it(toPath(dir), ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (it->is_regular_file(ec2)) out.push_back(fromPath(it->path().filename()));
    }
    // Порядок как у Array.Sort над именами: без учёта регистра, а при
    // равенстве — по коду символа, чтобы порядок был однозначным.
    std::sort(out.begin(), out.end(), [](const std::string& x, const std::string& y) {
        const std::size_t n = std::min(x.size(), y.size());
        for (std::size_t i = 0; i < n; ++i) {
            const int cx = std::tolower(static_cast<unsigned char>(x[i]));
            const int cy = std::tolower(static_cast<unsigned char>(y[i]));
            if (cx != cy) return cx < cy;
        }
        if (x.size() != y.size()) return x.size() < y.size();
        return x < y;
    });
    return out;
}

}  // namespace tjtm
