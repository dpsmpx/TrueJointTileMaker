# True Joint Tile Maker 0.6.0 на PascalABC.NET

Исходная реализация редактора. Перенос на C++ идёт в корне репозитория, а
эта версия больше не меняется: она эталон, с которым перенос сверяется —
см. [docs/porting.md](../docs/porting.md).

| Файл | Содержимое |
| --- | --- |
| `TJTM.pas` | состояние документов, отрисовка, ввод, экраны интерфейса |
| `TJTMColor.pas` | преобразования HSV и RGB, смешивание с прозрачностью, растровые буферы |
| `TJTMConfig.pas` | чтение и запись `Config.txt` |
| `TJTMMap.pas` | разбор, запись и проверка карт локаций Andors-Love |

## Сборка на Windows

Откройте `TJTM.pas` в среде [PascalABC.NET](http://pascalabc.net/) и нажмите
F9, или из командной строки:

```
pabcnetcclear.exe TJTM.pas
```

Программа ищет `Config.txt`, `Resources/` и `Картинки/` рядом с exe, а они
лежат в корне репозитория. Скопируйте `TJTM.exe` в корень или эти три пути
рядом с ним. Без них программа тоже запустится — с радужной палитрой и
пустым тайлом, а `Config.txt` создаст заново.

## Сборка и запуск в Linux

Под Mono, компилятором PascalABC.NET из исходников:

```bash
tools/setup-env.sh         # один раз: Mono, .NET SDK, компилятор
tools/gui.sh ref build     # собрать в build/ref вместе с ресурсами
tools/gui.sh ref start     # запустить; без экрана — в Xvfb
```
