///Карты локаций Andors-Love для True Joint Tile Maker.
///
///Формат разобран по src/world.cpp игры и проверен на всех картах из
///data/maps: разбор и обратная запись дают файл байт в байт.
///
///  name  <название>
///  size  <ширина> <высота>
///  grid
///  <ровно height строк ровно по width символов>
///  objects
///  npc|item|exit|spawn|chest|note|bed|sign <x> <y> <остальное>
///  end
///
///Сетка хранит только геометрию: проходимо или нет. Всё остальное описано
///объектами. У объекта разбираются только вид и координаты, а хвост строки
///сохраняется как есть: редактор не понимает условия перехода и содержимое
///сундуков, но и не должен их терять при пересохранении.
unit TJTMMap;

interface

const
  ///Коды клеток в порядке enum Tile из src/types.h игры.
  MT_FLOOR = 0;
  MT_WALL = 1;
  MT_WATER = 2;
  MT_TREE = 3;
  MT_GRASS = 4;
  MT_ROAD = 5;
  MT_DEADWATER = 6;
  MT_COUNT = 7;

  ///Границы размера локации. Игра не задаёт верхнюю, но редактору нужна.
  MAP_MIN_SIDE = 4;
  MAP_MAX_SIDE = 256;

type
  ///Объект локации. Kind и координаты разобраны, Rest — хвост строки как есть.
  MapObject = record
    Kind: string;
    X, Y: integer;
    Rest: string;
  end;

var
  MapName: string;
  MapW, MapH: integer;
  ///Клетки локации: MapGrid[x, y], значения MT_*.
  MapGrid: array[,] of byte;
  MapObjects: array of MapObject;
  ///Причина, по которой разбор не удался.
  MapError: string;
  ///Замечание по последнему разбору: файл открыт, но что-то пришлось поправить.
  MapWarning: string;

function MapTileChar(code: integer): char;
function MapCharTile(c: char): integer;
function MapTileName(code: integer): string;
function MapTileWalkable(code: integer): boolean;
///Виды объектов, которые понимает игра.
function MapKindCount: integer;
function MapKindName(i: integer): string;
///Заготовка хвоста строки для нового объекта данного вида.
function MapKindTemplate(i: integer): string;
function MapKindKnown(kind: string): boolean;

procedure MapNew(w, h: integer);
function MapLoad(FileName: string): boolean;
function MapSave(FileName: string): boolean;
///Строка объекта в том виде, в каком она попадёт в файл.
function MapObjectLine(o: MapObject): string;
///Меняет размер, сохраняя содержимое в общей части.
procedure MapResize(nw, nh: integer);
///Индекс последнего объекта в клетке или -1.
function MapObjectAt(x, y: integer): integer;
procedure MapAddObject(kind: string; x, y: integer; rest: string);
procedure MapDeleteObject(idx: integer);
///Проверка локации. Пустая строка означает, что замечаний нет.
function MapValidate: string;

implementation

const
  TILE_CHARS = '.#~T,=:';

function MapTileChar(code: integer): char;
begin
  if (code < 0) or (code >= MT_COUNT) then Result := '#'
  else Result := TILE_CHARS[code + 1];
end;

function MapCharTile(c: char): integer;
begin
  // Неизвестный символ игра считает стеной; повторяем это поведение.
  Result := MT_WALL;
  for var i := 0 to MT_COUNT - 1 do
    if TILE_CHARS[i + 1] = c then
    begin
      Result := i;
      exit;
    end;
end;

function MapTileName(code: integer): string;
begin
  case code of
    MT_FLOOR: Result := 'пол';
    MT_WALL: Result := 'стена';
    MT_WATER: Result := 'вода';
    MT_TREE: Result := 'дерево';
    MT_GRASS: Result := 'трава';
    MT_ROAD: Result := 'дорога';
    MT_DEADWATER: Result := 'стоячая вода';
    else Result := '?';
  end;
end;

function MapTileWalkable(code: integer): boolean;
begin
  Result := (code = MT_FLOOR) or (code = MT_GRASS) or
            (code = MT_ROAD) or (code = MT_DEADWATER);
end;

function MapKindCount: integer;
begin
  Result := 8;
end;

function MapKindName(i: integer): string;
begin
  case i of
    0: Result := 'npc';
    1: Result := 'item';
    2: Result := 'exit';
    3: Result := 'spawn';
    4: Result := 'chest';
    5: Result := 'note';
    6: Result := 'bed';
    else Result := 'sign';
  end;
end;

function MapKindTemplate(i: integer): string;
begin
  // Заготовки повторяют порядок полей из разбора в world.cpp.
  case i of
    0: Result := 'elder';
    1: Result := 'bread 1';
    2: Result := 'forest 2 8';
    3: Result := 'rat 2 4';
    4: Result := '0 - bread:1';
    5: Result := 'ink';
    6: Result := '';
    else Result := 'Текст таблички';
  end;
end;

function MapKindKnown(kind: string): boolean;
begin
  Result := false;
  for var i := 0 to MapKindCount - 1 do
    if MapKindName(i) = kind then
    begin
      Result := true;
      exit;
    end;
end;

function IsBlank(c: char): boolean;
begin
  Result := (c = ' ') or (c = Chr(9));
end;

///Первые maxWords слов строки и остаток после них.
///parts получает maxWords + 1 элемент: слова, затем хвост.
procedure SplitHead(s: string; maxWords: integer; var parts: array of string);
var
  i, n: integer;
  cur: string;
begin
  SetLength(parts, maxWords + 1);
  for var k := 0 to maxWords do parts[k] := '';
  i := 1;
  n := 0;
  while n < maxWords do
  begin
    while (i <= Length(s)) and IsBlank(s[i]) do i := i + 1;
    if i > Length(s) then break;
    cur := '';
    while (i <= Length(s)) and (not IsBlank(s[i])) do
    begin
      cur := cur + s[i];
      i := i + 1;
    end;
    parts[n] := cur;
    n := n + 1;
  end;
  while (i <= Length(s)) and IsBlank(s[i]) do i := i + 1;
  if i <= Length(s) then parts[maxWords] := s.Substring(i - 1);
end;

function ToIntDef(s: string; def: integer): integer;
begin
  try
    Result := StrToInt(s.Trim);
  except
    Result := def;
  end;
end;

function StripCR(s: string): string;
begin
  Result := s;
  while (Length(Result) > 0) and (Result[Length(Result)] = Chr(13)) do
    Result := Result.Substring(0, Length(Result) - 1);
  while (Length(Result) > 0) and (Result[1] = Chr(65279)) do
    Result := Result.Substring(1);
end;

procedure MapNew(w, h: integer);
begin
  if w < MAP_MIN_SIDE then w := MAP_MIN_SIDE;
  if w > MAP_MAX_SIDE then w := MAP_MAX_SIDE;
  if h < MAP_MIN_SIDE then h := MAP_MIN_SIDE;
  if h > MAP_MAX_SIDE then h := MAP_MAX_SIDE;
  MapW := w;
  MapH := h;
  SetLength(MapGrid, w, h);
  // Пол внутри, стена по краю: локация сразу замкнута, как в картах игры.
  for var y := 0 to h - 1 do
    for var x := 0 to w - 1 do
      if (x = 0) or (y = 0) or (x = w - 1) or (y = h - 1) then
        MapGrid[x, y] := MT_WALL
      else
        MapGrid[x, y] := MT_FLOOR;
  SetLength(MapObjects, 0);
  MapName := 'Новая локация';
  MapError := '';
  MapWarning := '';
end;

function MapObjectLine(o: MapObject): string;
begin
  Result := o.Kind + ' ' + IntToStr(o.X) + ' ' + IntToStr(o.Y);
  if o.Rest <> '' then Result := Result + ' ' + o.Rest;
end;

function MapLoad(FileName: string): boolean;
var
  lines: array of string;
  parts: array of string;
  i, y, x, w, h: integer;
  s, key, row: string;
  gridDone: boolean;
begin
  Result := false;
  MapError := '';
  MapWarning := '';

  lines := nil;
  try
    lines := System.IO.File.ReadAllLines(FileName, System.Text.Encoding.UTF8);
  except
    lines := nil;
  end;
  if lines = nil then
  begin
    MapError := 'не удалось прочитать файл';
    exit;
  end;

  MapName := '';
  w := 0;
  h := 0;
  gridDone := false;
  SetLength(MapObjects, 0);

  i := 0;
  while i < lines.Length do
  begin
    s := lines[i];
    i := i + 1;
    if s = nil then continue;
    s := StripCR(s);
    if s = '' then continue;
    if s[1] = ';' then continue;

    SplitHead(s, 1, parts);
    key := parts[0].ToLower;

    if key = 'name' then
      MapName := parts[1]

    else if key = 'size' then
    begin
      SplitHead(parts[1], 2, parts);
      w := ToIntDef(parts[0], 0);
      h := ToIntDef(parts[1], 0);
      if (w < 1) or (h < 1) then
      begin
        MapError := 'строка ' + IntToStr(i) + ': size ожидает два положительных числа';
        exit;
      end;
      if (w > MAP_MAX_SIDE) or (h > MAP_MAX_SIDE) then
      begin
        MapError := 'локация больше ' + IntToStr(MAP_MAX_SIDE) + ' клеток по стороне';
        exit;
      end;
    end

    else if key = 'grid' then
    begin
      if (w < 1) or (h < 1) then
      begin
        MapError := 'строка ' + IntToStr(i) + ': grid до объявления size';
        exit;
      end;
      SetLength(MapGrid, w, h);
      for y := 0 to h - 1 do
      begin
        if i >= lines.Length then
        begin
          MapError := 'сетка обрывается: не хватает строк';
          exit;
        end;
        row := StripCR(lines[i]);
        i := i + 1;
        // Строку не той длины дополняем стеной или обрезаем, но предупреждаем:
        // файл лучше открыть и дать поправить, чем отказаться его читать.
        if Length(row) <> w then
          MapWarning := 'строка сетки ' + IntToStr(y + 1) + ' была длиной ' +
                        IntToStr(Length(row)) + ' вместо ' + IntToStr(w) +
                        '; недостающее заполнено стеной';
        for x := 0 to w - 1 do
          if x < Length(row) then
            MapGrid[x, y] := MapCharTile(row[x + 1])
          else
            MapGrid[x, y] := MT_WALL;
      end;
      gridDone := true;
    end

    else if key = 'objects' then
      continue

    else if key = 'end' then
      break

    else if MapKindKnown(key) then
    begin
      SplitHead(s, 3, parts);
      MapAddObject(parts[0], ToIntDef(parts[1], 0), ToIntDef(parts[2], 0), parts[3]);
    end

    else
    begin
      MapError := 'строка ' + IntToStr(i) + ': неизвестная директива ' + key;
      exit;
    end;
  end;

  if not gridDone then
  begin
    MapError := 'в файле нет сетки';
    exit;
  end;

  MapW := w;
  MapH := h;
  Result := true;
end;

function MapSave(FileName: string): boolean;
var
  lines: array of string;
  row: string;

  procedure Add(s: string);
  begin
    SetLength(lines, Length(lines) + 1);
    lines[Length(lines) - 1] := s;
  end;

begin
  Result := false;
  if (FileName = '') or (MapW < 1) or (MapH < 1) then exit;

  SetLength(lines, 0);
  Add('name ' + MapName);
  Add('size ' + IntToStr(MapW) + ' ' + IntToStr(MapH));
  Add('grid');
  for var y := 0 to MapH - 1 do
  begin
    row := '';
    for var x := 0 to MapW - 1 do
      row := row + MapTileChar(MapGrid[x, y]);
    Add(row);
  end;
  Add('objects');
  for var k := 0 to Length(MapObjects) - 1 do
    Add(MapObjectLine(MapObjects[k]));
  Add('end');

  try
    // Игра читает карты как UTF-8 и сама снимает CR, но файлы в репозитории
    // хранятся с переводом строки LF — пишем так же.
    // Карты игры лежат без метки порядка байтов и с переводом строки LF —
    // пишем так же, чтобы файл не отличался от остальных в data/maps.
    var nl := '';
    nl := nl + Chr(10);
    var sw := new System.IO.StreamWriter(FileName, false, new System.Text.UTF8Encoding(false));
    try
      sw.NewLine := nl;
      for var k := 0 to Length(lines) - 1 do
        sw.WriteLine(lines[k]);
    finally
      sw.Close;
    end;
    Result := true;
  except
    Result := false;
  end;
end;

procedure MapResize(nw, nh: integer);
var
  old: array[,] of byte;
  ow, oh: integer;
begin
  if nw < MAP_MIN_SIDE then nw := MAP_MIN_SIDE;
  if nw > MAP_MAX_SIDE then nw := MAP_MAX_SIDE;
  if nh < MAP_MIN_SIDE then nh := MAP_MIN_SIDE;
  if nh > MAP_MAX_SIDE then nh := MAP_MAX_SIDE;
  if (nw = MapW) and (nh = MapH) then exit;

  old := MapGrid;
  ow := MapW;
  oh := MapH;
  SetLength(MapGrid, nw, nh);
  for var y := 0 to nh - 1 do
    for var x := 0 to nw - 1 do
      if (x < ow) and (y < oh) then
        MapGrid[x, y] := old[x, y]
      else
        MapGrid[x, y] := MT_WALL;
  MapW := nw;
  MapH := nh;

  // Объекты, оказавшиеся за новой границей, убираем: игра требует,
  // чтобы все координаты лежали внутри локации.
  var k := 0;
  while k < Length(MapObjects) do
    if (MapObjects[k].X >= MapW) or (MapObjects[k].Y >= MapH) then
      MapDeleteObject(k)
    else
      k := k + 1;
end;

function MapObjectAt(x, y: integer): integer;
begin
  Result := -1;
  for var k := 0 to Length(MapObjects) - 1 do
    if (MapObjects[k].X = x) and (MapObjects[k].Y = y) then Result := k;
end;

procedure MapAddObject(kind: string; x, y: integer; rest: string);
begin
  SetLength(MapObjects, Length(MapObjects) + 1);
  MapObjects[Length(MapObjects) - 1].Kind := kind;
  MapObjects[Length(MapObjects) - 1].X := x;
  MapObjects[Length(MapObjects) - 1].Y := y;
  MapObjects[Length(MapObjects) - 1].Rest := rest;
end;

procedure MapDeleteObject(idx: integer);
begin
  if (idx < 0) or (idx >= Length(MapObjects)) then exit;
  for var k := idx to Length(MapObjects) - 2 do
    MapObjects[k] := MapObjects[k + 1];
  SetLength(MapObjects, Length(MapObjects) - 1);
end;

function MapValidate: string;
var
  seen: array[,] of boolean;
  qx, qy: array of integer;
  head, tail, walkable, reached, regions: integer;
  o: MapObject;

  procedure Push(x, y: integer);
  begin
    if (x < 0) or (y < 0) or (x >= MapW) or (y >= MapH) then exit;
    if seen[x, y] then exit;
    if not MapTileWalkable(MapGrid[x, y]) then exit;
    seen[x, y] := true;
    qx[tail] := x;
    qy[tail] := y;
    tail := tail + 1;
  end;

begin
  Result := '';
  if (MapW < 1) or (MapH < 1) then
  begin
    Result := 'Локация пуста.';
    exit;
  end;

  // Объекты должны стоять внутри локации и на проходимой клетке:
  // именно это проверяет tools/gen_maps.py в самой игре.
  for var k := 0 to Length(MapObjects) - 1 do
  begin
    o := MapObjects[k];
    if (o.X < 0) or (o.Y < 0) or (o.X >= MapW) or (o.Y >= MapH) then
      Result := Result + MapObjectLine(o) + ' — координаты вне локации' + NewLine
    else if not MapTileWalkable(MapGrid[o.X, o.Y]) then
      Result := Result + MapObjectLine(o) + ' — стоит на непроходимой клетке (' +
                MapTileName(MapGrid[o.X, o.Y]) + ')' + NewLine;
  end;

  // Связность проходимой части: игрок должен добираться до всего.
  SetLength(seen, MapW, MapH);
  SetLength(qx, MapW * MapH);
  SetLength(qy, MapW * MapH);
  walkable := 0;
  for var y := 0 to MapH - 1 do
    for var x := 0 to MapW - 1 do
      if MapTileWalkable(MapGrid[x, y]) then walkable := walkable + 1;

  regions := 0;
  reached := 0;
  for var y := 0 to MapH - 1 do
    for var x := 0 to MapW - 1 do
      if MapTileWalkable(MapGrid[x, y]) and (not seen[x, y]) then
      begin
        regions := regions + 1;
        head := 0;
        tail := 0;
        Push(x, y);
        while head < tail do
        begin
          var cx := qx[head];
          var cy := qy[head];
          head := head + 1;
          reached := reached + 1;
          Push(cx + 1, cy);
          Push(cx - 1, cy);
          Push(cx, cy + 1);
          Push(cx, cy - 1);
        end;
      end;

  if walkable = 0 then
    Result := Result + 'В локации нет ни одной проходимой клетки.' + NewLine
  else if regions > 1 then
    Result := Result + 'Проходимая часть разбита на ' + IntToStr(regions) +
              ' несвязанных областей: до части локации не дойти.' + NewLine;
end;

end.
