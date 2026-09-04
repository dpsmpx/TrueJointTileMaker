///Настройки True Joint Tile Maker: чтение и запись Config.txt.
///Формат файла — "ключ = значение", по строке на параметр, порядок значения не имеет.
///Строки, начинающиеся с # или ;, считаются комментариями.
///Файл читается и пишется в UTF-8; для совместимости со старыми версиями
///при неудаче делается вторая попытка в CP1251.
///Любая ошибка чтения не считается фатальной: применяются значения по умолчанию.
unit TJTMConfig;

interface

const
  ///Допустимый диапазон стороны тайла.
  CFG_MIN_TILE = 4;
  CFG_MAX_TILE = 512;

var
  ///Показывать справку при запуске.
  CfgShowHelpOnStart: boolean;
  ///Показывать главное меню при запуске.
  CfgShowMenuOnStart: boolean;
  ///Ширина нового тайла по умолчанию.
  CfgDefaultWidth: integer;
  ///Высота нового тайла по умолчанию.
  CfgDefaultHeight: integer;
  ///Показывать сетку между пикселями.
  CfgShowGrid: boolean;
  ///Показывать шахматку под полупрозрачными пикселями.
  CfgShowChecker: boolean;
  ///Изображение, загружаемое при запуске. Пустая строка — не загружать.
  CfgStartupImage: string;
  ///Открывать при запуске последний файл вместо стартового изображения.
  CfgAutoLoadLast: boolean;
  ///Последний открытый или сохранённый файл. Заполняется программой.
  CfgLastFile: string;
  ///Папка, в которой открывались диалоги в прошлый раз. Заполняется программой.
  CfgLastFolder: string;
  ///Файл палитры 8x8, загружаемый при запуске.
  CfgPaletteFile: string;

///Сбрасывает все настройки к значениям по умолчанию.
procedure ConfigSetDefaults;
///Читает файл настроек. Возвращает true, если распознан хотя бы один параметр.
function ConfigLoad(FileName: string): boolean;
///Записывает файл настроек вместе с комментариями. Возвращает true при успехе.
function ConfigSave(FileName: string): boolean;
///Представление логического значения так, как оно пишется в файл.
function RuBool(b: boolean): string;

implementation

procedure ConfigSetDefaults;
begin
  CfgShowHelpOnStart := false;
  CfgShowMenuOnStart := true;
  CfgDefaultWidth := 32;
  CfgDefaultHeight := 32;
  CfgShowGrid := false;
  CfgShowChecker := true;
  CfgStartupImage := 'Картинки/debug.png';
  CfgAutoLoadLast := false;
  CfgLastFile := '';
  CfgLastFolder := '';
  CfgPaletteFile := 'Resources/Palitra.png';
end;

function RuBool(b: boolean): string;
begin
  if b then Result := 'да' else Result := 'нет';
end;

///Разбирает логическое значение. Понимает русские и английские написания.
///При нераспознанном значении возвращает def, а не падает.
function ParseBool(s: string; def: boolean): boolean;
begin
  s := s.Trim.ToLower;
  if (s = 'да') or (s = 'yes') or (s = 'true') or (s = '1') or (s = 'вкл') then
    Result := true
  else if (s = 'нет') or (s = 'no') or (s = 'false') or (s = '0') or (s = 'выкл') then
    Result := false
  else
    Result := def;
end;

///Разбирает целое с ограничением диапазона. Мусор в файле даёт def, а не исключение.
function ParseInt(s: string; def, lo, hi: integer): integer;
var
  v: integer;
begin
  try
    v := StrToInt(s.Trim);
  except
    v := def;
  end;
  if v < lo then v := lo;
  if v > hi then v := hi;
  Result := v;
end;

///Убирает метку порядка байтов, если она попала в начало первой строки.
function StripBOM(s: string): string;
begin
  Result := s;
  while (Length(Result) > 0) and (Result[1] = Chr(65279)) do
    Result := Result.Substring(1);
end;

///Применяет строки файла к настройкам. Возвращает число распознанных параметров.
function ApplyLines(lines: array of string): integer;
var
  s, key, val: string;
  p, n: integer;
begin
  n := 0;
  if lines = nil then
  begin
    Result := 0;
    exit;
  end;

  for var i := 0 to lines.Length - 1 do
  begin
    s := lines[i];
    if s = nil then continue;
    s := StripBOM(s).Trim;
    if s = '' then continue;
    if (s[1] = '#') or (s[1] = ';') then continue;

    p := s.IndexOf('=');
    if p <= 0 then continue;
    key := s.Substring(0, p).Trim.ToLower;
    val := s.Substring(p + 1).Trim;

    if key = 'showhelponstart' then
      begin CfgShowHelpOnStart := ParseBool(val, CfgShowHelpOnStart); n := n + 1 end
    else if key = 'showmenuonstart' then
      begin CfgShowMenuOnStart := ParseBool(val, CfgShowMenuOnStart); n := n + 1 end
    else if key = 'defaulttilewidth' then
      begin CfgDefaultWidth := ParseInt(val, CfgDefaultWidth, CFG_MIN_TILE, CFG_MAX_TILE); n := n + 1 end
    else if key = 'defaulttileheight' then
      begin CfgDefaultHeight := ParseInt(val, CfgDefaultHeight, CFG_MIN_TILE, CFG_MAX_TILE); n := n + 1 end
    else if key = 'showgrid' then
      begin CfgShowGrid := ParseBool(val, CfgShowGrid); n := n + 1 end
    else if key = 'showchecker' then
      begin CfgShowChecker := ParseBool(val, CfgShowChecker); n := n + 1 end
    else if key = 'startupimage' then
      begin CfgStartupImage := val; n := n + 1 end
    else if key = 'autoloadlastfile' then
      begin CfgAutoLoadLast := ParseBool(val, CfgAutoLoadLast); n := n + 1 end
    else if key = 'lastfile' then
      begin CfgLastFile := val; n := n + 1 end
    else if key = 'lastfolder' then
      begin CfgLastFolder := val; n := n + 1 end
    else if key = 'palettefile' then
      begin CfgPaletteFile := val; n := n + 1 end;
  end;

  Result := n;
end;

function ConfigLoad(FileName: string): boolean;
var
  lines: array of string;
  ok: boolean;
  known: integer;
begin
  ConfigSetDefaults;
  Result := false;
  if FileName = '' then exit;

  ok := false;
  try
    ok := System.IO.File.Exists(FileName);
  except
    ok := false;
  end;
  if not ok then exit;

  lines := nil;
  try
    lines := System.IO.File.ReadAllLines(FileName, System.Text.Encoding.UTF8);
  except
    lines := nil;
  end;

  known := ApplyLines(lines);

  // Файл от прежних версий мог быть сохранён в CP1251: пробуем ещё раз.
  if known = 0 then
  begin
    lines := nil;
    try
      lines := System.IO.File.ReadAllLines(FileName, System.Text.Encoding.GetEncoding(1251));
    except
      lines := nil;
    end;
    known := ApplyLines(lines);
  end;

  Result := known > 0;
end;

///Добавляет строку в конец динамического массива.
procedure AddLine(var arr: array of string; s: string);
begin
  SetLength(arr, Length(arr) + 1);
  arr[Length(arr) - 1] := s;
end;

function ConfigSave(FileName: string): boolean;
var
  lines: array of string;
begin
  Result := false;
  if FileName = '' then exit;

  SetLength(lines, 0);
  AddLine(lines, '# True Joint Tile Maker — файл настроек.');
  AddLine(lines, '# Формат: ключ = значение. Строки с # или ; в начале игнорируются.');
  AddLine(lines, '# Логические значения: да / нет.');
  AddLine(lines, '# Файл сохраняется в кодировке UTF-8.');
  AddLine(lines, '');
  AddLine(lines, '# Показывать справку при запуске.');
  AddLine(lines, 'ShowHelpOnStart = ' + RuBool(CfgShowHelpOnStart));
  AddLine(lines, '');
  AddLine(lines, '# Показывать главное меню при запуске.');
  AddLine(lines, 'ShowMenuOnStart = ' + RuBool(CfgShowMenuOnStart));
  AddLine(lines, '');
  AddLine(lines, '# Размер нового тайла по умолчанию, ' +
                 IntToStr(CFG_MIN_TILE) + '..' + IntToStr(CFG_MAX_TILE) + '.');
  AddLine(lines, '# Используется только когда стартовое изображение не загрузилось.');
  AddLine(lines, 'DefaultTileWidth = ' + IntToStr(CfgDefaultWidth));
  AddLine(lines, 'DefaultTileHeight = ' + IntToStr(CfgDefaultHeight));
  AddLine(lines, '');
  AddLine(lines, '# Показывать сетку между пикселями.');
  AddLine(lines, 'ShowGrid = ' + RuBool(CfgShowGrid));
  AddLine(lines, '');
  AddLine(lines, '# Показывать шахматку под полупрозрачными пикселями.');
  AddLine(lines, 'ShowChecker = ' + RuBool(CfgShowChecker));
  AddLine(lines, '');
  AddLine(lines, '# Изображение, загружаемое при запуске. Пусто — не загружать.');
  AddLine(lines, '# Размер тайла берётся из самого изображения.');
  AddLine(lines, 'StartupImage = ' + CfgStartupImage);
  AddLine(lines, '');
  AddLine(lines, '# Открывать последний файл вместо стартового изображения.');
  AddLine(lines, 'AutoLoadLastFile = ' + RuBool(CfgAutoLoadLast));
  AddLine(lines, '');
  AddLine(lines, '# Файл палитры 8x8, загружаемый при запуске.');
  AddLine(lines, 'PaletteFile = ' + CfgPaletteFile);
  AddLine(lines, '');
  AddLine(lines, '# Заполняется программой.');
  AddLine(lines, 'LastFile = ' + CfgLastFile);
  AddLine(lines, 'LastFolder = ' + CfgLastFolder);

  try
    System.IO.File.WriteAllLines(FileName, lines, System.Text.Encoding.UTF8);
    Result := true;
  except
    Result := false;
  end;
end;

end.
