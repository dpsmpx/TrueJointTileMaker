// Сжатие deflate и обёртка zlib для PNG и APNG.
//
// Своё, а не библиотечное: редактору нужны ровно поток zlib внутри PNG и
// CRC его кусков, а внешняя библиотека — это ещё одна зависимость, которую
// на Windows пришлось бы искать и собирать. Распаковка понимает весь
// формат deflate; упаковка ищет повторы хэш-цепочками с ленивым выбором и
// для каждого блока берёт самое короткое из трёх: без сжатия, с
// фиксированными и с собственными кодами Хаффмана.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tjtm {

std::uint32_t crc32(const std::uint8_t* data, std::size_t n, std::uint32_t crc = 0);
std::uint32_t adler32(const std::uint8_t* data, std::size_t n, std::uint32_t adler = 1);

/// Поток deflate без обёртки. Уровень: 0 — без сжатия, 1..9 — длина
/// хэш-цепочки, как у zlib.
std::vector<std::uint8_t> deflateRaw(const std::uint8_t* data, std::size_t n, int level = 6);
/// Поток zlib: заголовок, deflate, Adler-32.
std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data, int level = 6);

/// Распаковка deflate без обёртки. false и текст ошибки, если поток
/// испорчен. maxOut ограничивает размер результата: испорченный файл не
/// должен выесть всю память.
bool inflateRaw(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                std::string& error, std::size_t maxOut = std::size_t(1) << 31);
bool zlibDecompress(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                    std::string& error, std::size_t maxOut = std::size_t(1) << 31);

}  // namespace tjtm
