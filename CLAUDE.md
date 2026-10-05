# True Joint Tile Maker

Редактор пиксельной графики и карт игры Andors-Love. Идёт перенос с
PascalABC.NET на C++ (SDL2). Подробности — `docs/porting.md`.

## Правила

- `pascal/` — оригинал 0.6.0 и эталон. Не править: перенос сверяется с ним.
- Поведение переносится по коду оригинала и по исходникам GraphABC и
  PABCSystem в `$PABCNET_DIR/bin/Lib`, а не по описаниям.
- Язык комментариев, коммитов и документации — русский, как в оригинале.
  Комментарий объясняет, почему сделано так, а не что делает строка.
- Формат карт — договор с игрой: перенос обязан читать и писать все карты
  `$ANDORS_LOVE_DIR/data/maps` байт в байт.

## Команды

```bash
tools/setup-env.sh                                   # инструменты (в облаке — хук)
cmake --preset debug && cmake --build --preset debug # сборка
ctest --preset debug                                 # тесты; asan, clang — тоже пресеты
tools/lint.sh                                        # clang-format, clang-tidy, cppcheck, shellcheck
tools/gui.sh ref|port start|key|click|drag|shot|stop # окна оригинала и переноса
tools/gui.sh compare a.png b.png diff.png            # попиксельное сравнение
```

Пути к эталону и корпусу: `. ~/.local/share/tjtm-tools/env.sh`
(`PABCNET_DIR`, `ANDORS_LOVE_DIR`).
