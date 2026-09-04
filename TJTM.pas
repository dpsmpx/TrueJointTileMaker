///True Joint Tile Maker — редактор пиксельной графики и тайлсетов.
///
///Все настройки хранятся в Config.txt (модуль TJTMConfig), цветовая математика
///вынесена в TJTMColor. Здесь остаются состояние документа, отрисовка и ввод.
///
///Соглашение по кнопкам мыши, единое для всей программы:
///  ЛКМ  — применить: рисовать, залить, положить цвет или тайл в ячейку;
///  ПКМ  — взять: пипетка, забрать цвет или тайл из ячейки;
///  Ролик как кнопка — служебное действие (обмен цветов, загрузка/сохранение).
uses GraphABC, System.Drawing, System.Drawing.Imaging, TJTMColor, TJTMConfig;

const
  APP_TITLE = 'True Joint Tile Maker';
  APP_VERSION = '0.3.0';

  ///Холст редактирования.
  CANVAS_W = 512;
  CANVAS_H = 512;
  ///Панель инструментов справа от холста.
  PANEL_X = CANVAS_W;
  PANEL_W = 100;
  WIN_W = CANVAS_W + PANEL_W;
  WIN_H = CANVAS_H;

  MIN_TILE = CFG_MIN_TILE;
  MAX_TILE = CFG_MAX_TILE;

  PAL_COLS = 8;
  PAL_ROWS = 8;
  TSET_COLS = 8;
  TSET_ROWS = 8;
  TSET_CELL = 12;

  ///Вертикальная разметка панели. Отрисовка и обработка кликов
  ///пользуются одними и теми же границами.
  PAL_Y0 = 0;      PAL_Y1 = 100;
  HSV_Y0 = 100;    HSV_Y1 = 200;
  HUE_Y0 = 200;    HUE_Y1 = 210;
  ALPHA_Y0 = 210;  ALPHA_Y1 = 220;
  TOOLS_Y0 = 222;  TOOLS_Y1 = 258;
  TSET_Y0 = 260;   TSET_Y1 = 356;
  BUF_Y0 = 358;    BUF_Y1 = 454;
  STAT_Y0 = 456;   STAT_Y1 = 484;
  MENU_Y0 = 486;   MENU_Y1 = 510;

  TOOL_CELL = 33;
  BAR_SIZE = 100;
  BAR_H = 10;

  TOOL_PEN = 0;
  TOOL_FILL = 1;
  TOOL_SELECT = 2;

  MIN_ZOOM = 1;
  MAX_ZOOM = 64;

  ///Верхняя граница памяти под историю отмен.
  UNDO_BUDGET_BYTES = 6 * 1024 * 1024;

  ///Коды клавиш заданы числами, чтобы не зависеть от набора констант VK_* в GraphABC.
  KEY_BACK = 8;    KEY_ENTER = 13;  KEY_ESC = 27;   KEY_SPACE = 32;
  KEY_LEFT = 37;   KEY_UP = 38;     KEY_RIGHT = 39; KEY_DOWN = 40;
  KEY_DELETE = 46;
  KEY_1 = 49;      KEY_2 = 50;      KEY_3 = 51;
  KEY_A = 65;      KEY_C = 67;      KEY_G = 71;     KEY_H = 72;
  KEY_L = 76;      KEY_M = 77;      KEY_N = 78;     KEY_P = 80;
  KEY_R = 82;      KEY_S = 83;      KEY_V = 86;     KEY_X = 88;
  KEY_Y = 89;      KEY_Z = 90;
  KEY_NUMPLUS = 107; KEY_NUMMINUS = 109;
  KEY_PLUS = 187;  KEY_MINUS = 189;

  MB_LEFT = 1;
  MB_RIGHT = 2;

var
  ///Размеры холста, продублированы в переменные ради читаемости формул.
  W: integer := CANVAS_W;
  H: integer := CANVAS_H;

  ///Размер тайла. Всегда совпадает с фактическими размерами Tile.
  TW: integer := 32;
  TH: integer := 32;
  ///Само изображение: Tile[x, y].
  Tile: array[,] of Color;

  ///Буфер обмена. Живёт независимо от выделения и переживает смену тайла.
  Buffer: array[,] of Color;

  ///Прямоугольник выделения в координатах тайла.
  SelX, SelY, SelW, SelH: integer;
  HasSelection: boolean;
  ///Идёт перетаскивание выделения — рисуем предпросмотр буфера.
  DraggingSelection: boolean;

  ///Палитра 8x8 и тайлсет 8x8. Тайлсет хранится плоским массивом из 64 элементов.
  Palette: array[,] of Color;
  TileSet: array of array[,] of Color;

  ///Масштаб и сдвиг холста.
  PixelSize: integer := 8;
  ViewX, ViewY: integer;
  GridOn: boolean;
  CheckerOn: boolean := true;

  ///Рабочие цвета и их представление в HSV.
  FirstColor: Color;
  SecondColor: Color;
  Hue, Sat, Val: integer;

  Tool: integer := TOOL_PEN;
  ToolPics: array of Picture;

  ///Текущий файл и признак несохранённых изменений.
  CurrentFile: string := '';
  Modified: boolean;

  ///История отмен: снимки Tile целиком.
  UndoStack: array of array[,] of Color;
  RedoStack: array of array[,] of Color;

  ///Маска текущего штриха: каждый пиксель закрашивается не более одного раза,
  ///иначе полупрозрачная кисть накладывалась бы сама на себя.
  StrokeMask: array[,] of boolean;

  ///Кэш полос выбора цвета. HSV-квадрат пересчитывается только при смене тона.
  RawHSV: array of byte;
  RawHue: array of byte;
  RawAlpha: array of byte;
  BmpHSV, BmpHue, BmpAlpha: Bitmap;
  CachedHue: integer := -1;

  ///Состояние ввода.
  KeyPressed: boolean;
  KeyPending: boolean;
  PendingKey: integer;
  MouseX, MouseY: integer;
  MousePressed: boolean;
  ClickPending: boolean;
  ClickX, ClickY, ClickButton: integer;
  WheelAccum: integer;

  NeedRepaint: boolean := true;
  ConfigPath: string;
  BaseDir: string;
  ///Во время штриха каждый закрашенный пиксель сразу выводится на экран,
  ///иначе пришлось бы перерисовывать весь холст на каждое движение мыши.
  LiveDraw: boolean;
  ///Последний показанный заголовок окна и позиция курсора в статусной строке.
  ///Нужны, чтобы не трогать интерфейс, когда ничего не изменилось.
  LastTitle: string := '';
  LastStatX: integer := -100000;
  LastStatY: integer := -100000;

  ///Цвета интерфейса.
  cBack := RGB(238, 238, 240);
  cFace := RGB(225, 226, 230);
  cHover := RGB(250, 250, 252);
  cBorder := RGB(150, 152, 158);
  cText := RGB(24, 24, 28);
  cTextDim := RGB(140, 142, 148);
  cPanel := RGB(205, 206, 212);
  cCanvas := RGB(120, 122, 128);
  cCheckA := RGB(200, 200, 206);
  cCheckB := RGB(168, 168, 176);
  cAccent := RGB(60, 110, 200);

// ---------------------------------------------------------------------------
// Мелкие утилиты
// ---------------------------------------------------------------------------

///Опережающее объявление. Рисование отдельного пикселя требуется уже в PaintPixel,
///а вся отрисовка описана ниже, вместе с остальным выводом на экран.
procedure DrawTilePixel(x, y: integer); forward;

///Короткая пауза. Стоит во всех циклах ожидания, чтобы программа
///не занимала ядро процессора вхолостую.
procedure Idle;
begin
  System.Threading.Thread.Sleep(4);
end;

///Папка программы. При неудаче — текущий каталог.
function DetectBaseDir: string;
begin
  Result := '';
  try
    Result := System.IO.Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly.Location);
  except
    Result := '';
  end;
  if Result = '' then
    try
      Result := System.IO.Directory.GetCurrentDirectory;
    except
      Result := '';
    end;
end;

///Достраивает относительный путь до полного, отталкиваясь от папки программы.
function ResolvePath(p: string): string;
begin
  Result := p;
  if p = '' then exit;
  try
    if not System.IO.Path.IsPathRooted(p) then
      Result := System.IO.Path.Combine(BaseDir, p);
  except
    Result := p;
  end;
end;

///Имя файла без пути. Пустая строка, если пути нет.
function FileNameOnly(p: string): string;
begin
  Result := '';
  if p = '' then exit;
  try
    Result := System.IO.Path.GetFileName(p);
  except
    Result := p;
  end;
end;

///Существует ли файл. Любая ошибка доступа трактуется как "нет".
function FileThere(p: string): boolean;
begin
  Result := false;
  if p = '' then exit;
  try
    Result := System.IO.File.Exists(p);
  except
    Result := false;
  end;
end;

///Запоминает файл и его папку в настройках и сразу сохраняет Config.txt.
procedure RememberFile(p: string);
begin
  if p = '' then exit;
  CfgLastFile := p;
  try
    CfgLastFolder := System.IO.Path.GetDirectoryName(p);
  except
    CfgLastFolder := '';
  end;
  ConfigSave(ConfigPath);
end;

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------

procedure OnKeyDownHandler(key: integer);
begin
  KeyPressed := true;
  PendingKey := key;
  KeyPending := true;
end;

procedure OnKeyUpHandler(key: integer);
begin
  KeyPressed := false;
end;

procedure OnMouseDownHandler(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MousePressed := true;
  ClickX := x;
  ClickY := y;
  ClickButton := mb;
  ClickPending := true;
end;

procedure OnMouseMoveHandler(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
end;

procedure OnMouseUpHandler(x, y, mb: integer);
begin
  MouseX := x;
  MouseY := y;
  MousePressed := false;
end;

///Колесо только копит отсчёты. Рисовать из обработчика события нельзя:
///он выполняется в потоке интерфейса параллельно основному циклу.
procedure OnWheelHandler(Sender: object; e: System.Windows.Forms.MouseEventArgs);
begin
  WheelAccum := WheelAccum + Sign(e.Delta);
end;

///Забирает накопленный поворот колеса.
function TakeWheel: integer;
begin
  Result := WheelAccum;
  WheelAccum := 0;
end;

///Забирает нажатие клавиши. Ноль означает, что нажатий не было.
///Такой съём заменяет прежние циклы ожидания отпускания клавиши.
function TakeKey: integer;
begin
  if KeyPending then
  begin
    Result := PendingKey;
    KeyPending := false;
  end
  else
    Result := 0;
end;

procedure WaitMouseRelease;
begin
  while MousePressed do Idle;
end;

///Кнопка мыши, отличная от левой и правой, считается средней (роликом).
function IsMiddle(b: integer): boolean;
begin
  Result := (b <> MB_LEFT) and (b <> MB_RIGHT);
end;

// ---------------------------------------------------------------------------
// Координаты холста
// ---------------------------------------------------------------------------

function TileToScreenX(tx: integer): integer;
begin
  Result := ViewX + tx * PixelSize;
end;

function TileToScreenY(ty: integer): integer;
begin
  Result := ViewY + ty * PixelSize;
end;

function ScreenToTileX(sx: integer): integer;
begin
  Result := Floor((sx - ViewX) / PixelSize);
end;

function ScreenToTileY(sy: integer): integer;
begin
  Result := Floor((sy - ViewY) / PixelSize);
end;

///Лежит ли клетка внутри тайла. Единственная проверка границ,
///которой пользуются все операции рисования.
function InTile(x, y: integer): boolean;
begin
  Result := (x >= 0) and (y >= 0) and (x < TW) and (y < TH);
end;

///Толщина зазора сетки. На мелком масштабе сетка съела бы всё изображение.
function GridGap: integer;
begin
  if GridOn and (PixelSize >= 4) then Result := 1 else Result := 0;
end;

///Ставит вид так, чтобы тайл был по центру, а при масштабе крупнее холста
///не давал увести изображение за край.
procedure ClampView;
var
  fullW, fullH: integer;
begin
  fullW := TW * PixelSize;
  fullH := TH * PixelSize;
  if fullW <= W then ViewX := (W - fullW) div 2
  else ViewX := ClampI(ViewX, W - fullW, 0);
  if fullH <= H then ViewY := (H - fullH) div 2
  else ViewY := ClampI(ViewY, H - fullH, 0);
end;

///Подбирает масштаб так, чтобы тайл целиком помещался на холсте.
procedure FitScale;
begin
  PixelSize := Min(W div Max(1, TW), H div Max(1, TH));
  PixelSize := ClampI(PixelSize, MIN_ZOOM, MAX_ZOOM);
  ClampView;
end;

procedure ZoomBy(dir: integer);
var
  old: integer;
begin
  old := PixelSize;
  if dir > 0 then
  begin
    if PixelSize < MAX_ZOOM then PixelSize := PixelSize * 2;
  end
  else
    if PixelSize > MIN_ZOOM then PixelSize := PixelSize div 2;
  if PixelSize <> old then
  begin
    ClampView;
    NeedRepaint := true;
  end;
end;

// ---------------------------------------------------------------------------
// Документ: история, выделение, правки
// ---------------------------------------------------------------------------

///Полная копия сетки цветов.
function CloneGrid(g: array[,] of Color): array[,] of Color;
var
  gw, gh: integer;
begin
  if g = nil then
  begin
    SetLength(Result, 0, 0);
    exit;
  end;
  gw := Length(g, 0);
  gh := Length(g, 1);
  SetLength(Result, gw, gh);
  for var i := 0 to gh - 1 do
    for var j := 0 to gw - 1 do
      Result[j, i] := g[j, i];
end;

///Сколько снимков истории влезает в отведённый объём памяти.
function UndoDepth: integer;
var
  per: integer;
begin
  per := TW * TH * 4;
  if per < 1 then per := 1;
  Result := ClampI(UNDO_BUDGET_BYTES div per, 3, 64);
end;

procedure ClearHistory;
begin
  SetLength(UndoStack, 0);
  SetLength(RedoStack, 0);
end;

///Снимает состояние перед изменением. Вызывается один раз на операцию,
///а не на каждый закрашенный пиксель.
procedure PushUndo;
var
  limit: integer;
begin
  if (TW < 1) or (TH < 1) then exit;
  SetLength(UndoStack, Length(UndoStack) + 1);
  UndoStack[Length(UndoStack) - 1] := CloneGrid(Tile);
  limit := UndoDepth;
  while Length(UndoStack) > limit do
  begin
    for var i := 0 to Length(UndoStack) - 2 do
      UndoStack[i] := UndoStack[i + 1];
    SetLength(UndoStack, Length(UndoStack) - 1);
  end;
  SetLength(RedoStack, 0);
  Modified := true;
end;

procedure DoUndo;
begin
  if Length(UndoStack) = 0 then exit;
  SetLength(RedoStack, Length(RedoStack) + 1);
  RedoStack[Length(RedoStack) - 1] := CloneGrid(Tile);
  Tile := UndoStack[Length(UndoStack) - 1];
  SetLength(UndoStack, Length(UndoStack) - 1);
  Modified := true;
  NeedRepaint := true;
end;

procedure DoRedo;
begin
  if Length(RedoStack) = 0 then exit;
  SetLength(UndoStack, Length(UndoStack) + 1);
  UndoStack[Length(UndoStack) - 1] := CloneGrid(Tile);
  Tile := RedoStack[Length(RedoStack) - 1];
  SetLength(RedoStack, Length(RedoStack) - 1);
  Modified := true;
  NeedRepaint := true;
end;

///Сбрасывает выделение. Буфер обмена при этом не трогается —
///он живёт отдельно и переживает смену тайла.
procedure ResetSelection;
begin
  SelX := 0;
  SelY := 0;
  SelW := 0;
  SelH := 0;
  HasSelection := false;
  DraggingSelection := false;
end;

///Ставит новый документ. Единственное место, где меняются TW и TH.
procedure SetTile(newTile: array[,] of Color; fileName: string);
begin
  Tile := newTile;
  TW := Length(Tile, 0);
  TH := Length(Tile, 1);
  CurrentFile := fileName;
  Modified := false;
  ResetSelection;
  ClearHistory;
  FitScale;
  NeedRepaint := true;
end;

procedure NewTileOfSize(nw, nh: integer);
var
  t: array[,] of Color;
begin
  nw := ClampI(nw, MIN_TILE, MAX_TILE);
  nh := ClampI(nh, MIN_TILE, MAX_TILE);
  SetLength(t, nw, nh);
  for var i := 0 to nh - 1 do
    for var j := 0 to nw - 1 do
      t[j, i] := ARGB(255, 255, 255, 255);
  SetTile(t, '');
end;

///Цвет шахматки под прозрачными пикселями. Считается в координатах тайла,
///поэтому при перемещении вида клетки не «плывут».
function CheckerColor(x, y: integer): Color;
begin
  if ((x div 4) + (y div 4)) mod 2 = 0 then Result := cCheckA else Result := cCheckB;
end;

///Цвет пикселя тайла так, как он должен выглядеть на экране.
function DisplayColor(x, y: integer): Color;
begin
  Result := Tile[x, y];
  if CheckerOn and (Result.A < 255) then
    Result := BlendOver(Result, CheckerColor(x, y));
end;

///Закрашивает пиксель текущим цветом с учётом прозрачности.
///Маска штриха не даёт наложить полупрозрачную кисть дважды на одно место.
procedure PaintPixel(x, y: integer);
begin
  if not InTile(x, y) then exit;
  if (Length(StrokeMask, 0) = TW) and (Length(StrokeMask, 1) = TH) then
  begin
    if StrokeMask[x, y] then exit;
    StrokeMask[x, y] := true;
  end;
  Tile[x, y] := BlendOver(FirstColor, Tile[x, y]);
  if LiveDraw then DrawTilePixel(x, y);
end;

///Отрезок по Брезенхэму. Нужен, чтобы при быстром движении мыши
///в штрихе не оставалось пропусков.
procedure PaintLine(x0, y0, x1, y1: integer);
var
  dx, dy, sx, sy, err, e2: integer;
begin
  dx := Abs(x1 - x0);
  dy := -Abs(y1 - y0);
  if x0 < x1 then sx := 1 else sx := -1;
  if y0 < y1 then sy := 1 else sy := -1;
  err := dx + dy;
  while true do
  begin
    PaintPixel(x0, y0);
    if (x0 = x1) and (y0 = y1) then break;
    e2 := 2 * err;
    if e2 >= dy then
    begin
      err := err + dy;
      x0 := x0 + sx;
    end;
    if e2 <= dx then
    begin
      err := err + dx;
      y0 := y0 + sy;
    end;
  end;
end;

///Заливка области одного цвета. Обход в ширину с отдельным массивом посещённых
///клеток: изображение по ходу работы не портится служебными цветами.
procedure FloodFill(sx, sy: integer; newColor: Color);
var
  visited: array[,] of boolean;
  qx, qy: array of integer;
  head, tail: integer;
  target: Color;

  procedure Push(x, y: integer);
  begin
    if not InTile(x, y) then exit;
    if visited[x, y] then exit;
    if not SameColor(Tile[x, y], target) then exit;
    visited[x, y] := true;
    qx[tail] := x;
    qy[tail] := y;
    tail := tail + 1;
  end;

begin
  if not InTile(sx, sy) then exit;
  target := Tile[sx, sy];
  if SameColor(target, newColor) then exit;

  SetLength(visited, TW, TH);
  SetLength(qx, TW * TH);
  SetLength(qy, TW * TH);
  head := 0;
  tail := 0;
  Push(sx, sy);
  while head < tail do
  begin
    var x := qx[head];
    var y := qy[head];
    head := head + 1;
    Tile[x, y] := newColor;
    Push(x + 1, y);
    Push(x - 1, y);
    Push(x, y + 1);
    Push(x, y - 1);
  end;
end;

///Копирует выделенную область в буфер обмена.
procedure CopySelectionToBuffer;
begin
  if not HasSelection then exit;
  if (SelW < 1) or (SelH < 1) then exit;
  SetLength(Buffer, SelW, SelH);
  for var i := 0 to SelH - 1 do
    for var j := 0 to SelW - 1 do
      Buffer[j, i] := Tile[SelX + j, SelY + i];
end;

///Вставляет буфер так, что выходящая за край часть просто отбрасывается.
procedure PasteBufferAt(px, py: integer);
var
  bw, bh: integer;
begin
  bw := Length(Buffer, 0);
  bh := Length(Buffer, 1);
  if (bw < 1) or (bh < 1) then exit;
  for var i := 0 to bh - 1 do
    for var j := 0 to bw - 1 do
      if InTile(px + j, py + i) then
        if Buffer[j, i].A > 0 then
          Tile[px + j, py + i] := Buffer[j, i];
end;

procedure ClearTile;
begin
  PushUndo;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      Tile[j, i] := ARGB(255, 255, 255, 255);
  NeedRepaint := true;
end;

procedure RandomizeTile;
begin
  PushUndo;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      if Random(0, 3) = 0 then
        Tile[j, i] := RGB(128 + Random(64), 128 + Random(64), 128 + Random(64))
      else
        Tile[j, i] := RGB(64 + Random(128), 64 + Random(128), 64 + Random(128));
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Рабочие цвета
// ---------------------------------------------------------------------------

///Обновляет HSV-представление по текущему первому цвету.
///Тон сохраняется, когда цвет серый: у серого тона нет, и сбрасывать
///положение ползунка в ноль было бы неудобно.
procedure SyncHSVFromColor;
var
  c: HSVColor;
begin
  c := ColorToHSV(FirstColor);
  if c.S > 0 then Hue := c.H;
  Sat := c.S;
  Val := c.V;
end;

///Собирает первый цвет из HSV, сохраняя выбранную прозрачность.
procedure ApplyHSVToColor;
var
  c: Color;
begin
  c := HSVtoRGB(Hue, Sat, Val);
  FirstColor := ARGB(FirstColor.A, c.R, c.G, c.B);
end;

procedure PickColor(c: Color);
begin
  FirstColor := c;
  SyncHSVFromColor;
  NeedRepaint := true;
end;

procedure SwapColors;
var
  t: Color;
begin
  t := FirstColor;
  FirstColor := SecondColor;
  SecondColor := t;
  SyncHSVFromColor;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Геометрия панели. Границы ячеек задаются одной функцией на каждую сетку,
// и отрисовка и попадание мыши считают их одинаково.
// ---------------------------------------------------------------------------

function PalCellX(j: integer): integer;
begin
  Result := PANEL_X + Round(j * PANEL_W / PAL_COLS);
end;

function PalCellY(i: integer): integer;
begin
  Result := PAL_Y0 + Round(i * (PAL_Y1 - PAL_Y0) / PAL_ROWS);
end;

///Ячейка палитры под точкой. Перебор вместо деления гарантирует,
///что индекс всегда попадает в границы массива.
function PalHit(px, py: integer; var cj, ci: integer): boolean;
begin
  Result := false;
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
      if (px >= PalCellX(j)) and (px < PalCellX(j + 1)) and
         (py >= PalCellY(i)) and (py < PalCellY(i + 1)) then
      begin
        cj := j;
        ci := i;
        Result := true;
        exit;
      end;
end;

function TSetIndex(j, i: integer): integer;
begin
  Result := i * TSET_COLS + j;
end;

function TSetCellX(j: integer): integer;
begin
  Result := PANEL_X + 2 + j * TSET_CELL;
end;

function TSetCellY(i: integer): integer;
begin
  Result := TSET_Y0 + i * TSET_CELL;
end;

function TSetHit(px, py: integer; var idx: integer): boolean;
begin
  Result := false;
  for var i := 0 to TSET_ROWS - 1 do
    for var j := 0 to TSET_COLS - 1 do
      if (px >= TSetCellX(j)) and (px < TSetCellX(j) + TSET_CELL) and
         (py >= TSetCellY(i)) and (py < TSetCellY(i) + TSET_CELL) then
      begin
        idx := TSetIndex(j, i);
        Result := true;
        exit;
      end;
end;

function ToolCellX(i: integer): integer;
begin
  Result := PANEL_X + 2 + i * TOOL_CELL;
end;

function ToolHit(px, py: integer; var idx: integer): boolean;
begin
  Result := false;
  for var i := 0 to 2 do
    if (px >= ToolCellX(i)) and (px < ToolCellX(i) + TOOL_CELL - 2) and
       (py >= TOOLS_Y0) and (py < TOOLS_Y1) then
    begin
      idx := i;
      Result := true;
      exit;
    end;
end;

// ---------------------------------------------------------------------------
// Примитивы интерфейса
// ---------------------------------------------------------------------------

///Рисует кнопку и сообщает, был ли по ней клик в этом кадре.
///Экранам меню и настроек достаточно этой одной функции.
function UIButton(x, y, bw, bh: integer; caption: string; enabled: boolean): boolean;
var
  over: boolean;
begin
  Result := false;
  over := enabled and (MouseX >= x) and (MouseX < x + bw) and
                      (MouseY >= y) and (MouseY < y + bh);
  GraphABC.Brush.Color := cBorder;
  FillRoundRect(x, y, x + bw, y + bh, 6, 6);
  if not enabled then GraphABC.Brush.Color := cFace
  else if over then GraphABC.Brush.Color := cHover
  else GraphABC.Brush.Color := cBack;
  FillRoundRect(x + 1, y + 1, x + bw - 1, y + bh - 1, 6, 6);
  if enabled then GraphABC.Font.Color := cText else GraphABC.Font.Color := cTextDim;
  DrawTextCentered(x + 6, y, x + bw - 6, y + bh, caption);
  if enabled and ClickPending and
     (ClickX >= x) and (ClickX < x + bw) and (ClickY >= y) and (ClickY < y + bh) then
    Result := true;
end;

procedure UILabel(x, y, bw, bh: integer; caption: string; c: Color);
begin
  GraphABC.Font.Color := c;
  DrawTextCentered(x, y, x + bw, y + bh, caption);
end;

// ---------------------------------------------------------------------------
// Отрисовка холста
// ---------------------------------------------------------------------------

///Рисует один пиксель тайла. Используется при штрихе, чтобы не перерисовывать
///весь холст на каждое движение мыши.
procedure DrawTilePixel(x, y: integer);
var
  g: integer;
begin
  if not InTile(x, y) then exit;
  g := GridGap;
  GraphABC.Brush.Color := DisplayColor(x, y);
  FillRect(TileToScreenX(x), TileToScreenY(y),
           TileToScreenX(x + 1) - g, TileToScreenY(y + 1) - g);
end;

procedure DrawCanvas;
var
  j0, j1, i0, i1, g: integer;
  c: Color;
begin
  GraphABC.Brush.Color := cCanvas;
  FillRect(0, 0, W, H);

  // Рисуются только клетки, попадающие в окно: на крупном масштабе
  // это избавляет от перебора всего тайла.
  j0 := Max(0, ScreenToTileX(0));
  j1 := Min(TW - 1, ScreenToTileX(W - 1));
  i0 := Max(0, ScreenToTileY(0));
  i1 := Min(TH - 1, ScreenToTileY(H - 1));
  g := GridGap;

  for var i := i0 to i1 do
    for var j := j0 to j1 do
    begin
      c := DisplayColor(j, i);
      if HasSelection and (j >= SelX) and (j < SelX + SelW) and
                          (i >= SelY) and (i < SelY + SelH) then
        c := BlendOver(ARGB(64, cAccent.R, cAccent.G, cAccent.B), c);
      GraphABC.Brush.Color := c;
      FillRect(TileToScreenX(j), TileToScreenY(i),
               TileToScreenX(j + 1) - g, TileToScreenY(i + 1) - g);
    end;

  // Предпросмотр буфера на новом месте, пока выделение перетаскивают.
  if DraggingSelection then
  begin
    var bw := Length(Buffer, 0);
    var bh := Length(Buffer, 1);
    for var i := 0 to bh - 1 do
      for var j := 0 to bw - 1 do
        if InTile(SelX + j, SelY + i) then
          if Buffer[j, i].A > 0 then
          begin
            GraphABC.Brush.Color := BlendOver(Buffer[j, i], CheckerColor(SelX + j, SelY + i));
            FillRect(TileToScreenX(SelX + j), TileToScreenY(SelY + i),
                     TileToScreenX(SelX + j + 1) - g, TileToScreenY(SelY + i + 1) - g);
          end;
  end;

  // Рамка выделения рисуется четырьмя полосками, а не контуром:
  // так она не зависит от текущего пера и стиля кисти.
  if HasSelection and (SelW > 0) and (SelH > 0) then
  begin
    var rx0 := TileToScreenX(SelX);
    var ry0 := TileToScreenY(SelY);
    var rx1 := TileToScreenX(SelX + SelW);
    var ry1 := TileToScreenY(SelY + SelH);
    GraphABC.Brush.Color := cAccent;
    FillRect(rx0 - 1, ry0 - 1, rx1 + 1, ry0);
    FillRect(rx0 - 1, ry1, rx1 + 1, ry1 + 1);
    FillRect(rx0 - 1, ry0, rx0, ry1);
    FillRect(rx1, ry0, rx1 + 1, ry1);
  end;
end;

// ---------------------------------------------------------------------------
// Отрисовка панели по секциям. Отдельные процедуры нужны, чтобы при
// перетаскивании ползунка перерисовывать только полосы выбора цвета.
// ---------------------------------------------------------------------------

///Пересобирает квадрат HSV, только если сменился тон.
procedure EnsureHSVBitmap;
begin
  if CachedHue = Hue then exit;
  CachedHue := Hue;
  RenderHSVPlane(RawHSV, BAR_SIZE, Hue);
  if BmpHSV <> nil then BmpHSV.Dispose;
  BmpHSV := BytesToImage(RawHSV, BAR_SIZE);
end;

procedure DrawPaletteSection;
begin
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
    begin
      GraphABC.Brush.Color := Palette[j, i];
      FillRect(PalCellX(j), PalCellY(i), PalCellX(j + 1), PalCellY(i + 1));
    end;
end;

procedure DrawColorSection;
var
  mx, my: integer;
begin
  EnsureHSVBitmap;
  if BmpHSV <> nil then GraphBufferGraphics.DrawImage(BmpHSV, PANEL_X, HSV_Y0);
  if BmpHue <> nil then GraphBufferGraphics.DrawImage(BmpHue, PANEL_X, HUE_Y0);
  if BmpAlpha <> nil then GraphBufferGraphics.DrawImage(BmpAlpha, PANEL_X, ALPHA_Y0);

  // Перекрестье в квадрате насыщенность/яркость.
  mx := PANEL_X + PercentToAxis(Sat, BAR_SIZE);
  my := HSV_Y0 + ValueToAxis(Val, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 0, 0, 0);
  FillRect(mx - 1, HSV_Y0, mx + 1, HSV_Y1);
  FillRect(PANEL_X, my - 1, PANEL_X + BAR_SIZE, my + 1);

  // Метка на полосе тонов.
  mx := PANEL_X + HueToAxis(Hue, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 0, 0, 0);
  FillRect(mx - 1, HUE_Y0, mx + 1, HUE_Y1);

  // Метка на полосе прозрачности.
  mx := PANEL_X + ByteToAxis(FirstColor.A, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 220, 40, 40);
  FillRect(mx - 1, ALPHA_Y0, mx + 1, ALPHA_Y1);
end;

function ToolShortName(i: integer): string;
begin
  case i of
    TOOL_PEN: Result := 'Кар';
    TOOL_FILL: Result := 'Зал';
    else Result := 'Выд';
  end;
end;

procedure DrawToolsSection;
var
  pic: Picture;
  x0: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, TOOLS_Y0, PANEL_X + PANEL_W, TOOLS_Y1);
  for var i := 0 to 2 do
  begin
    x0 := ToolCellX(i);
    if Tool = i then GraphABC.Brush.Color := cHover else GraphABC.Brush.Color := cFace;
    FillRect(x0, TOOLS_Y0, x0 + TOOL_CELL - 2, TOOLS_Y1);
    pic := nil;
    if Tool = i then
    begin
      if Length(ToolPics) > i + 3 then pic := ToolPics[i + 3];
    end
    else
      if Length(ToolPics) > i then pic := ToolPics[i];
    // Значки могут отсутствовать на диске: тогда рисуется подпись.
    if pic <> nil then
      pic.Draw(x0 + 3, TOOLS_Y0 + 3)
    else
      UILabel(x0, TOOLS_Y0, TOOL_CELL - 2, TOOLS_Y1 - TOOLS_Y0, ToolShortName(i), cText);
  end;
end;

///Рисует сетку цветов вписанной в квадрат заданного размера.
///Перебор идёт по пикселям приёмника, поэтому стоимость не зависит
///от размера исходного изображения.
procedure DrawGridThumb(g: array[,] of Color; x0, y0, size: integer);
var
  gw, gh, sx, sy: integer;
begin
  if g = nil then exit;
  gw := Length(g, 0);
  gh := Length(g, 1);
  if (gw < 1) or (gh < 1) or (size < 1) then exit;
  for var py := 0 to size - 1 do
    for var px := 0 to size - 1 do
    begin
      sx := ClampI(px * gw div size, 0, gw - 1);
      sy := ClampI(py * gh div size, 0, gh - 1);
      GraphABC.Brush.Color := BlendOver(g[sx, sy], CheckerColor(sx, sy));
      FillRect(x0 + px, y0 + py, x0 + px + 1, y0 + py + 1);
    end;
end;

procedure DrawTileSetSection;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, TSET_Y0, PANEL_X + PANEL_W, TSET_Y1);
  for var i := 0 to TSET_ROWS - 1 do
    for var j := 0 to TSET_COLS - 1 do
      DrawGridThumb(TileSet[TSetIndex(j, i)], TSetCellX(j), TSetCellY(i), TSET_CELL - 1);
end;

procedure DrawBufferSection;
var
  bw, bh, side: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, BUF_Y0, PANEL_X + PANEL_W, BUF_Y1);
  bw := Length(Buffer, 0);
  bh := Length(Buffer, 1);
  if (bw < 1) or (bh < 1) then
  begin
    UILabel(PANEL_X, BUF_Y0, PANEL_W, BUF_Y1 - BUF_Y0, 'буфер пуст', cTextDim);
    exit;
  end;
  side := Min(PANEL_W - 4, BUF_Y1 - BUF_Y0 - 4);
  DrawGridThumb(Buffer, PANEL_X + 2, BUF_Y0 + 2, side);
end;

procedure DrawStatusSection;
var
  tx, ty: integer;
  s: string;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, STAT_Y0, PANEL_X + PANEL_W, STAT_Y1);
  tx := ScreenToTileX(MouseX);
  ty := ScreenToTileY(MouseY);
  if (MouseX < W) and InTile(tx, ty) then
    s := 'X ' + IntToStr(tx) + '  Y ' + IntToStr(ty)
  else
    s := '—';
  s := s + NewLine + IntToStr(TW) + 'x' + IntToStr(TH) + '  ' + IntToStr(PixelSize) + 'x';
  UILabel(PANEL_X, STAT_Y0, PANEL_W, STAT_Y1 - STAT_Y0, s, cText);
end;

procedure DrawMenuButtonSection;
var
  caption: string;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, MENU_Y0 - 2, PANEL_X + PANEL_W, MENU_Y1 + 2);
  caption := 'Меню';
  if Modified then caption := 'Меню *';
  UIButton(PANEL_X + 4, MENU_Y0, PANEL_W - 8, MENU_Y1 - MENU_Y0, caption, true);
end;

procedure DrawPanel;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, 0, PANEL_X + PANEL_W, H);
  DrawPaletteSection;
  DrawColorSection;
  DrawToolsSection;
  DrawTileSetSection;
  DrawBufferSection;
  DrawStatusSection;
  DrawMenuButtonSection;
end;

///Полная перерисовка окна.
procedure Repaint;
begin
  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    DrawCanvas;
    DrawPanel;
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  Redraw;
  NeedRepaint := false;
end;

///Перерисовка только холста. Нужна при протяжке выделения:
///панель при этом не меняется, а её миниатюры — самая дорогая часть кадра.
procedure RepaintCanvasOnly;
begin
  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    DrawCanvas;
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  Redraw;
end;

///Перерисовка только полос выбора цвета. Нужна при перетаскивании ползунков,
///чтобы не пересобирать миниатюры тайлсета на каждом кадре.
procedure RepaintColorOnly;
begin
  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    DrawColorSection;
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  Redraw;
end;

// ---------------------------------------------------------------------------
// Модальные экраны
// ---------------------------------------------------------------------------

///Разбивает строку на слова по пробелам.
function SplitWords(s: string): array of string;
var
  cur: string;
begin
  SetLength(Result, 0);
  cur := '';
  for var i := 1 to Length(s) do
    if s[i] = ' ' then
    begin
      if cur <> '' then
      begin
        SetLength(Result, Length(Result) + 1);
        Result[Length(Result) - 1] := cur;
        cur := '';
      end;
    end
    else
      cur := cur + s[i];
  if cur <> '' then
  begin
    SetLength(Result, Length(Result) + 1);
    Result[Length(Result) - 1] := cur;
  end;
end;

///Переносит абзац по словам в строки не шире maxWidth.
function WrapParagraph(s: string; maxWidth: integer): array of string;
var
  words: array of string;
  cur, probe: string;
begin
  SetLength(Result, 0);
  words := SplitWords(s);
  if Length(words) = 0 then
  begin
    SetLength(Result, 1);
    Result[0] := '';
    exit;
  end;
  cur := '';
  for var i := 0 to Length(words) - 1 do
  begin
    if cur = '' then probe := words[i] else probe := cur + ' ' + words[i];
    if (cur <> '') and (TextWidth(probe) > maxWidth) then
    begin
      SetLength(Result, Length(Result) + 1);
      Result[Length(Result) - 1] := cur;
      cur := words[i];
    end
    else
      cur := probe;
  end;
  SetLength(Result, Length(Result) + 1);
  Result[Length(Result) - 1] := cur;
end;

///Разворачивает массив абзацев в массив готовых к выводу строк.
function LayoutText(paragraphs: array of string; maxWidth: integer): array of string;
var
  part: array of string;
begin
  SetLength(Result, 0);
  for var i := 0 to Length(paragraphs) - 1 do
  begin
    part := WrapParagraph(paragraphs[i], maxWidth);
    for var k := 0 to Length(part) - 1 do
    begin
      SetLength(Result, Length(Result) + 1);
      Result[Length(Result) - 1] := part[k];
    end;
  end;
end;

///Экран с прокручиваемым текстом. Закрывается кнопкой, Enter или Escape.
procedure ShowTextScreen(title: string; paragraphs: array of string);
var
  lines: array of string;
  lineH, visible, top, maxTop, k: integer;
  done: boolean;
begin
  done := false;
  top := 0;
  ClickPending := false;
  KeyPending := false;
  lineH := TextHeight('Wg') + 2;
  if lineH < 8 then lineH := 8;
  visible := (H - 120) div lineH;
  if visible < 1 then visible := 1;
  lines := LayoutText(paragraphs, WIN_W - 80);

  while not done do
  begin
    maxTop := Max(0, Length(lines) - visible);
    top := ClampI(top, 0, maxTop);

    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 14, WIN_W, 44, title);

    GraphABC.Brush.Color := cFace;
    FillRoundRect(24, 52, WIN_W - 24, H - 56, 6, 6);
    GraphABC.Font.Color := cText;
    for var i := 0 to visible - 1 do
    begin
      k := top + i;
      if k >= Length(lines) then break;
      TextOut(36, 60 + i * lineH, lines[k]);
    end;

    if maxTop > 0 then
    begin
      UILabel(WIN_W - 220, H - 52, 200, 20,
              'строки ' + IntToStr(top + 1) + '–' + IntToStr(Min(Length(lines), top + visible)) +
              ' из ' + IntToStr(Length(lines)), cTextDim);
      if UIButton(24, H - 50, 60, 28, '▲', top > 0) then top := top - visible;
      if UIButton(90, H - 50, 60, 28, '▼', top < maxTop) then top := top + visible;
    end;
    if UIButton(WIN_W div 2 - 60, H - 50, 120, 28, 'Закрыть', true) then done := true;

    Redraw;
    ClickPending := false;

    var key := TakeKey;
    if (key = KEY_ESC) or (key = KEY_ENTER) or (key = KEY_BACK) then done := true;
    if key = KEY_DOWN then top := top + 1;
    if key = KEY_UP then top := top - 1;

    top := top - TakeWheel * 3;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

///Короткое сообщение с одной кнопкой.
procedure ShowMessage(title, text: string);
var
  p: array of string;
begin
  SetLength(p, 1);
  p[0] := text;
  ShowTextScreen(title, p);
end;

///Вопрос с тремя вариантами. Результат: 1 — да, 0 — нет, -1 — отмена.
function Ask3(title, text, yes, no_, cancel: string): integer;
var
  done: boolean;
  res: integer;
begin
  res := -1;
  done := false;
  ClickPending := false;
  KeyPending := false;
  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, H div 2 - 90, WIN_W, H div 2 - 50, title);
    GraphABC.Font.Color := cText;
    DrawTextCentered(40, H div 2 - 46, WIN_W - 40, H div 2 + 10, text);

    if UIButton(WIN_W div 2 - 270, H div 2 + 30, 170, 34, yes, true) then
    begin
      res := 1;
      done := true;
    end;
    if UIButton(WIN_W div 2 - 85, H div 2 + 30, 170, 34, no_, true) then
    begin
      res := 0;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 100, H div 2 + 30, 170, 34, cancel, true) then
    begin
      res := -1;
      done := true;
    end;

    Redraw;
    ClickPending := false;
    var key := TakeKey;
    if key = KEY_ESC then
    begin
      res := -1;
      done := true;
    end;
    Idle;
  end;
  WaitMouseRelease;
  NeedRepaint := true;
  Result := res;
end;

///Экран создания нового тайла. Заменяет прежний ввод чисел с клавиатуры,
///который блокировал программу на ReadInteger.
///Возвращает true, если пользователь подтвердил создание.
function AskNewTileSize(var nw, nh: integer): boolean;
var
  done, ok: boolean;
  presets: array of integer;
begin
  SetLength(presets, 6);
  presets[0] := 8; presets[1] := 16; presets[2] := 32;
  presets[3] := 64; presets[4] := 128; presets[5] := 256;

  nw := ClampI(nw, MIN_TILE, MAX_TILE);
  nh := ClampI(nh, MIN_TILE, MAX_TILE);
  done := false;
  ok := false;
  ClickPending := false;
  KeyPending := false;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 30, WIN_W, 66, 'Создать новый тайл');
    UILabel(0, 70, WIN_W, 26,
            'Размер от ' + IntToStr(MIN_TILE) + ' до ' + IntToStr(MAX_TILE) + ' пикселей', cTextDim);

    UILabel(60, 120, 160, 34, 'Ширина', cText);
    if UIButton(230, 120, 46, 34, '−8', nw > MIN_TILE) then nw := ClampI(nw - 8, MIN_TILE, MAX_TILE);
    if UIButton(282, 120, 46, 34, '−1', nw > MIN_TILE) then nw := ClampI(nw - 1, MIN_TILE, MAX_TILE);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(334, 120, 434, 154, 6, 6);
    UILabel(334, 120, 100, 34, IntToStr(nw), cText);
    if UIButton(440, 120, 46, 34, '+1', nw < MAX_TILE) then nw := ClampI(nw + 1, MIN_TILE, MAX_TILE);
    if UIButton(492, 120, 46, 34, '+8', nw < MAX_TILE) then nw := ClampI(nw + 8, MIN_TILE, MAX_TILE);

    UILabel(60, 170, 160, 34, 'Высота', cText);
    if UIButton(230, 170, 46, 34, '−8', nh > MIN_TILE) then nh := ClampI(nh - 8, MIN_TILE, MAX_TILE);
    if UIButton(282, 170, 46, 34, '−1', nh > MIN_TILE) then nh := ClampI(nh - 1, MIN_TILE, MAX_TILE);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(334, 170, 434, 204, 6, 6);
    UILabel(334, 170, 100, 34, IntToStr(nh), cText);
    if UIButton(440, 170, 46, 34, '+1', nh < MAX_TILE) then nh := ClampI(nh + 1, MIN_TILE, MAX_TILE);
    if UIButton(492, 170, 46, 34, '+8', nh < MAX_TILE) then nh := ClampI(nh + 8, MIN_TILE, MAX_TILE);

    UILabel(60, 230, 200, 30, 'Быстрый выбор', cTextDim);
    for var i := 0 to Length(presets) - 1 do
      if UIButton(60 + i * 82, 264, 76, 34, IntToStr(presets[i]), true) then
      begin
        nw := presets[i];
        nh := presets[i];
      end;

    if UIButton(WIN_W div 2 - 180, 350, 170, 38, 'Создать', true) then
    begin
      ok := true;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, 350, 170, 38, 'Отмена', true) then
    begin
      ok := false;
      done := true;
    end;

    Redraw;
    ClickPending := false;
    var key := TakeKey;
    if key = KEY_ESC then
    begin
      ok := false;
      done := true;
    end;
    if key = KEY_ENTER then
    begin
      ok := true;
      done := true;
    end;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
  Result := ok;
end;

///Точная настройка цвета по каналам ARGB.
///Положение ползунка и обработка клика считаются одной парой формул,
///поэтому маркер всегда встаёт ровно под курсор.
procedure ShowColorScreen;
const
  SX0 = 70;
  SX1 = 540;
var
  done: boolean;
  ch: array of integer;
  names: array of string;
  y, mx: integer;

  function ValueToX(v: integer): integer;
  begin
    Result := SX0 + Round(ClampI(v, 0, 255) * (SX1 - SX0) / 255);
  end;

  function XToValue(px: integer): integer;
  begin
    Result := ClampI(Round((px - SX0) * 255 / (SX1 - SX0)), 0, 255);
  end;

begin
  SetLength(ch, 4);
  SetLength(names, 4);
  ch[0] := FirstColor.A; names[0] := 'Прозрачность';
  ch[1] := FirstColor.R; names[1] := 'Красный';
  ch[2] := FirstColor.G; names[2] := 'Зелёный';
  ch[3] := FirstColor.B; names[3] := 'Синий';

  done := false;
  ClickPending := false;
  KeyPending := false;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 16, WIN_W, 50, 'Точный выбор цвета');

    for var i := 0 to 3 do
    begin
      y := 74 + i * 54;
      UILabel(0, y - 22, WIN_W, 20, names[i] + ': ' + IntToStr(ch[i]), cText);
      GraphABC.Brush.Color := cFace;
      FillRoundRect(SX0 - 6, y, SX1 + 6, y + 22, 5, 5);
      GraphABC.Brush.Color := cBorder;
      FillRect(SX0, y + 9, SX1, y + 13);
      mx := ValueToX(ch[i]);
      GraphABC.Brush.Color := cAccent;
      FillRoundRect(mx - 7, y - 2, mx + 7, y + 24, 4, 4);
      // Тащить ползунок можно, удерживая кнопку в его полосе.
      if MousePressed and (MouseY >= y - 6) and (MouseY <= y + 28) then
        ch[i] := XToValue(MouseX);
    end;

    GraphABC.Brush.Color := cBorder;
    FillRect(70, 296, 542, 366);
    // Шахматка кладётся блоками по 8 пикселей, а не по одному:
    // экран перерисовывается на каждом кадре, пока тащат ползунок.
    for var bx := 0 to 58 do
      for var by := 0 to 8 do
        if (bx + by) mod 2 = 0 then
        begin
          GraphABC.Brush.Color := cCheckA;
          FillRect(71 + bx * 8, 297 + by * 8,
                   Min(541, 79 + bx * 8), Min(365, 305 + by * 8));
        end;
    GraphABC.Brush.Color := ARGB(ch[0], ch[1], ch[2], ch[3]);
    FillRect(71, 297, 541, 365);

    if UIButton(WIN_W div 2 - 180, 400, 170, 38, 'Применить', true) then
    begin
      FirstColor := ARGB(ch[0], ch[1], ch[2], ch[3]);
      SyncHSVFromColor;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, 400, 170, 38, 'Отмена', true) then done := true;

    Redraw;
    ClickPending := false;
    var key := TakeKey;
    if (key = KEY_ESC) or (key = KEY_BACK) then done := true;
    if key = KEY_ENTER then
    begin
      FirstColor := ARGB(ch[0], ch[1], ch[2], ch[3]);
      SyncHSVFromColor;
      done := true;
    end;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Файловые операции. Каждый вход в файловую систему обёрнут в try:
// испорченный или чужой файл не должен ронять редактор.
// ---------------------------------------------------------------------------

const
  PNG_FILTER = 'Изображения PNG (*.png)|*.png|Все файлы (*.*)|*.*';

///Диалог открытия. Пустая строка означает отмену.
function AskOpenFile(title: string): string;
begin
  Result := '';
  try
    var dlg := System.Windows.Forms.OpenFileDialog.Create;
    dlg.Title := title;
    dlg.Filter := PNG_FILTER;
    var dir := ResolvePath(CfgLastFolder);
    if (dir <> '') and System.IO.Directory.Exists(dir) then dlg.InitialDirectory := dir;
    if dlg.ShowDialog = System.Windows.Forms.DialogResult.OK then Result := dlg.FileName;
  except
    Result := '';
  end;
  WaitMouseRelease;
  NeedRepaint := true;
end;

///Диалог сохранения. Пустая строка означает отмену.
function AskSaveFile(title, suggested: string): string;
begin
  Result := '';
  try
    var dlg := System.Windows.Forms.SaveFileDialog.Create;
    dlg.Title := title;
    dlg.Filter := PNG_FILTER;
    dlg.DefaultExt := 'png';
    dlg.AddExtension := true;
    if suggested <> '' then dlg.FileName := suggested;
    var dir := ResolvePath(CfgLastFolder);
    if (dir <> '') and System.IO.Directory.Exists(dir) then dlg.InitialDirectory := dir;
    if dlg.ShowDialog = System.Windows.Forms.DialogResult.OK then Result := dlg.FileName;
  except
    Result := '';
  end;
  WaitMouseRelease;
  NeedRepaint := true;
end;

///Читает картинку в сетку цветов. Возвращает false при любой неудаче.
function ReadImageGrid(path: string; var g: array[,] of Color; quiet: boolean): boolean;
begin
  Result := false;
  if not FileThere(path) then
  begin
    if not quiet then ShowMessage('Файл не найден', path);
    exit;
  end;
  try
    var pic := Picture.Create(path);
    if (pic.Width < 1) or (pic.Height < 1) then exit;
    if (pic.Width > MAX_TILE) or (pic.Height > MAX_TILE) then
    begin
      if not quiet then
        ShowMessage('Слишком большое изображение',
                    'Максимальный размер тайла — ' + IntToStr(MAX_TILE) + ' на ' +
                    IntToStr(MAX_TILE) + ' пикселей. В файле ' +
                    IntToStr(pic.Width) + ' на ' + IntToStr(pic.Height) + '.');
      exit;
    end;
    SetLength(g, pic.Width, pic.Height);
    for var i := 0 to pic.Height - 1 do
      for var j := 0 to pic.Width - 1 do
        g[j, i] := pic.GetPixel(j, i);
    Result := true;
  except
    on e: Exception do
      if not quiet then ShowMessage('Не удалось открыть файл', e.Message);
  end;
end;

///Загружает тайл из файла. Размер документа берётся из самого изображения,
///поэтому размер из настроек здесь не участвует.
///remember = false для автозагрузки при старте: стартовая картинка
///не должна подменять собой последний файл пользователя.
function LoadTileFrom(path: string; quiet, remember: boolean): boolean;
var
  g: array[,] of Color;
begin
  Result := false;
  if not ReadImageGrid(path, g, quiet) then exit;
  SetTile(g, path);
  if remember then RememberFile(path);
  Result := true;
end;

function SaveTileTo(path: string): boolean;
begin
  Result := false;
  if path = '' then exit;
  try
    var pic := Picture.Create(TW, TH);
    for var i := 0 to TH - 1 do
      for var j := 0 to TW - 1 do
        pic.SetPixel(j, i, Tile[j, i]);
    pic.Save(path);
    CurrentFile := path;
    Modified := false;
    RememberFile(path);
    Result := true;
  except
    on e: Exception do
      ShowMessage('Не удалось сохранить файл', e.Message);
  end;
end;

///Сохраняет сетку цветов в файл.
function SaveGridTo(g: array[,] of Color; path: string): boolean;
var
  gw, gh: integer;
begin
  Result := false;
  gw := Length(g, 0);
  gh := Length(g, 1);
  if (path = '') or (gw < 1) or (gh < 1) then exit;
  try
    var pic := Picture.Create(gw, gh);
    for var i := 0 to gh - 1 do
      for var j := 0 to gw - 1 do
        pic.SetPixel(j, i, g[j, i]);
    pic.Save(path);
    Result := true;
  except
    on e: Exception do
      ShowMessage('Не удалось сохранить файл', e.Message);
  end;
end;

// ---------------------------------------------------------------------------
// Палитра и тайлсет
// ---------------------------------------------------------------------------

///Заполняет палитру из картинки, растягивая её до 8x8.
function LoadPaletteFrom(path: string; quiet: boolean): boolean;
var
  g: array[,] of Color;
  gw, gh: integer;
begin
  Result := false;
  if not ReadImageGrid(path, g, quiet) then exit;
  gw := Length(g, 0);
  gh := Length(g, 1);
  if (gw < 1) or (gh < 1) then exit;
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
      Palette[j, i] := g[ClampI(j * gw div PAL_COLS, 0, gw - 1),
                         ClampI(i * gh div PAL_ROWS, 0, gh - 1)];
  NeedRepaint := true;
  Result := true;
end;

procedure PaletteIO;
var
  answer: integer;
  path: string;
begin
  answer := Ask3('Палитра', 'Загрузить палитру из файла или сохранить текущую?',
                 'Загрузить', 'Сохранить', 'Отмена');
  if answer = 1 then
  begin
    path := AskOpenFile('Загрузить палитру');
    if path <> '' then LoadPaletteFrom(path, false);
  end
  else if answer = 0 then
  begin
    path := AskSaveFile('Сохранить палитру', 'palette.png');
    if path <> '' then SaveGridTo(Palette, path);
  end;
end;

///Служебный цвет-разделитель в файле тайлсета.
///Он отмечает правый и нижний край каждого тайла внутри ячейки.
function TileSetMarker: Color;
begin
  Result := RGB(1, 2, 3);
end;

///Загружает тайлсет 8x8. Сканирование границ ограничено размером ячейки,
///поэтому файл без разделителей больше не уводит чтение за край картинки.
function LoadTileSetFrom(path: string; quiet: boolean): boolean;
var
  g, cell: array[,] of Color;
  imgW, imgH, cellW, cellH, tw2, th2, idx: integer;
begin
  Result := false;
  if not ReadImageGrid(path, g, quiet) then exit;
  imgW := Length(g, 0);
  imgH := Length(g, 1);
  cellW := imgW div TSET_COLS;
  cellH := imgH div TSET_ROWS;
  if (cellW < 1) or (cellH < 1) then
  begin
    if not quiet then
      ShowMessage('Не похоже на тайлсет',
                  'Картинка должна делиться на 8 ячеек по каждой стороне.');
    exit;
  end;

  for var i := 0 to TSET_ROWS - 1 do
    for var j := 0 to TSET_COLS - 1 do
    begin
      tw2 := 0;
      while (tw2 < cellW) and (not SameColor(g[j * cellW + tw2, i * cellH], TileSetMarker)) do
        tw2 := tw2 + 1;
      th2 := 0;
      while (th2 < cellH) and (not SameColor(g[j * cellW, i * cellH + th2], TileSetMarker)) do
        th2 := th2 + 1;

      idx := TSetIndex(j, i);
      if (tw2 < 1) or (th2 < 1) then
        SetLength(cell, 0, 0)
      else
      begin
        SetLength(cell, tw2, th2);
        for var y := 0 to th2 - 1 do
          for var x := 0 to tw2 - 1 do
            cell[x, y] := g[j * cellW + x, i * cellH + y];
      end;
      TileSet[idx] := cell;
    end;

  NeedRepaint := true;
  Result := true;
end;

function SaveTileSetTo(path: string): boolean;
var
  cell: array[,] of Color;
  cellW, cellH: integer;
begin
  Result := false;
  if path = '' then exit;
  cellW := 1;
  cellH := 1;
  for var k := 0 to Length(TileSet) - 1 do
  begin
    if Length(TileSet[k], 0) > cellW then cellW := Length(TileSet[k], 0);
    if Length(TileSet[k], 1) > cellH then cellH := Length(TileSet[k], 1);
  end;
  // Запас в один пиксель гарантирует, что разделитель всегда есть,
  // даже если тайл занимает ячейку целиком.
  cellW := cellW + 1;
  cellH := cellH + 1;

  try
    var pic := Picture.Create(TSET_COLS * cellW, TSET_ROWS * cellH);
    pic.Clear(TileSetMarker);
    for var i := 0 to TSET_ROWS - 1 do
      for var j := 0 to TSET_COLS - 1 do
      begin
        cell := TileSet[TSetIndex(j, i)];
        for var y := 0 to Length(cell, 1) - 1 do
          for var x := 0 to Length(cell, 0) - 1 do
            pic.SetPixel(j * cellW + x, i * cellH + y, cell[x, y]);
      end;
    pic.Save(path);
    Result := true;
  except
    on e: Exception do
      ShowMessage('Не удалось сохранить тайлсет', e.Message);
  end;
end;

procedure TileSetIO;
var
  answer: integer;
  path: string;
begin
  answer := Ask3('Тайлсет', 'Загрузить тайлсет из файла или сохранить текущий?',
                 'Загрузить', 'Сохранить', 'Отмена');
  if answer = 1 then
  begin
    path := AskOpenFile('Загрузить тайлсет');
    if path <> '' then LoadTileSetFrom(path, false);
  end
  else if answer = 0 then
  begin
    path := AskSaveFile('Сохранить тайлсет', 'tileset.png');
    if path <> '' then SaveTileSetTo(path);
  end;
end;

// ---------------------------------------------------------------------------
// Команды меню
// ---------------------------------------------------------------------------

function CommandSaveAs: boolean;
var
  path, suggest: string;
begin
  suggest := FileNameOnly(CurrentFile);
  if suggest = '' then suggest := 'tile.png';
  path := AskSaveFile('Сохранить тайл', suggest);
  if path = '' then Result := false else Result := SaveTileTo(path);
end;

function CommandSave: boolean;
begin
  if CurrentFile = '' then Result := CommandSaveAs
  else Result := SaveTileTo(CurrentFile);
end;

///Спрашивает про несохранённые изменения. false — операцию нужно отменить.
function ConfirmDiscard(what: string): boolean;
var
  answer: integer;
begin
  Result := true;
  if not Modified then exit;
  answer := Ask3('Несохранённые изменения',
                 'В тайле есть изменения, которых нет в файле. ' + what,
                 'Сохранить', 'Не сохранять', 'Отмена');
  if answer = 1 then Result := CommandSave
  else if answer = 0 then Result := true
  else Result := false;
end;

procedure CommandNew;
var
  nw, nh: integer;
begin
  if not ConfirmDiscard('Создать новый тайл?') then exit;
  nw := CfgDefaultWidth;
  nh := CfgDefaultHeight;
  if AskNewTileSize(nw, nh) then NewTileOfSize(nw, nh);
end;

procedure CommandOpen;
var
  path: string;
begin
  if not ConfirmDiscard('Открыть другой файл?') then exit;
  path := AskOpenFile('Открыть тайл');
  if path <> '' then LoadTileFrom(path, false, true);
end;

procedure CommandOpenLast;
var
  path: string;
begin
  path := ResolvePath(CfgLastFile);
  if not FileThere(path) then
  begin
    ShowMessage('Файл недоступен',
                'Последний файл не найден по пути: ' + CfgLastFile);
    exit;
  end;
  if not ConfirmDiscard('Открыть последний файл?') then exit;
  LoadTileFrom(path, false, true);
end;

procedure CommandSaveSelection;
var
  path: string;
  g: array[,] of Color;
begin
  if HasSelection and (SelW > 0) and (SelH > 0) then CopySelectionToBuffer;
  g := Buffer;
  if (Length(g, 0) < 1) or (Length(g, 1) < 1) then
  begin
    ShowMessage('Нечего сохранять',
                'Сначала выделите область инструментом выделения или возьмите тайл из тайлсета.');
    exit;
  end;
  path := AskSaveFile('Сохранить выделение', 'selection.png');
  if path <> '' then SaveGridTo(g, path);
end;

procedure QuitApp;
begin
  ConfigSave(ConfigPath);
  Halt;
end;

procedure CommandExit;
begin
  if not ConfirmDiscard('Выйти из программы?') then exit;
  QuitApp;
end;

// ---------------------------------------------------------------------------
// Справка и настройки
// ---------------------------------------------------------------------------

procedure ShowHelpScreen;
var
  p: array of string;

  procedure Add(s: string);
  begin
    SetLength(p, Length(p) + 1);
    p[Length(p) - 1] := s;
  end;

begin
  SetLength(p, 0);
  Add(APP_TITLE + ', версия ' + APP_VERSION + '.');
  Add('Редактор пиксельной графики и тайлсетов.');
  Add('');
  Add('ОБЩЕЕ ПРАВИЛО МЫШИ');
  Add('Левая кнопка применяет: рисует, заливает, кладёт цвет или тайл в ячейку.');
  Add('Правая кнопка берёт: работает пипеткой, забирает цвет или тайл из ячейки.');
  Add('Нажатие на ролик выполняет служебное действие: меняет местами рабочие цвета,');
  Add('а на палитре и тайлсете открывает загрузку и сохранение.');
  Add('');
  Add('ХОЛСТ');
  Add('Карандаш: левая кнопка рисует первым цветом с учётом прозрачности,');
  Add('правая берёт цвет пикселя.');
  Add('Заливка: левая кнопка заливает связную область одного цвета.');
  Add('Выделение: протяните рамку левой кнопкой, чтобы выделить область и');
  Add('скопировать её в буфер. Одиночный щелчок вне выделения вставляет буфер.');
  Add('Потяните за выделенную область, чтобы скопировать её в новое место.');
  Add('Вращение ролика меняет масштаб.');
  Add('');
  Add('ПАНЕЛЬ СПРАВА, сверху вниз');
  Add('Палитра восемь на восемь. Квадрат насыщенности и яркости.');
  Add('Полоса тона. Полоса прозрачности. Выбор инструмента.');
  Add('Тайлсет восемь на восемь. Миниатюра буфера. Координаты. Кнопка меню.');
  Add('');
  Add('КЛАВИАТУРА');
  Add('Escape — главное меню.');
  Add('N — создать тайл. L — загрузить. S — сохранить. Shift не требуется.');
  Add('A — сохранить выделение или буфер в отдельный файл.');
  Add('Z — отменить действие. Y — повторить отменённое.');
  Add('V — вставить буфер в позицию курсора.');
  Add('X — поменять местами первый и второй цвета.');
  Add('P — точный выбор цвета по каналам ARGB.');
  Add('G — сетка. C — шахматка под прозрачными пикселями.');
  Add('R — заполнить тайл случайными цветами. Delete — очистить тайл.');
  Add('1, 2, 3 — карандаш, заливка, выделение.');
  Add('Плюс и минус — масштаб. Стрелки — сдвиг изображения.');
  Add('Пробел — вернуть изображение в центр. H — эта справка.');
  Add('');
  Add('НАСТРОЙКИ');
  Add('Все параметры хранятся в файле Config.txt рядом с программой в кодировке');
  Add('UTF-8, по строке на параметр в виде «ключ = значение». Файл можно править');
  Add('вручную или через пункт «Настройки» в меню.');
  Add('При запуске программа пробует открыть стартовое изображение; размер тайла');
  Add('берётся из него. Если открыть не удалось, создаётся пустой тайл того');
  Add('размера, который задан в настройках.');
  Add('');
  Add('БЛАГОДАРНОСТИ');
  Add('Автор программы — DeadPixel, vk.com/deadpixel_programmer.');
  Add('Команде PascalABC.NET за язык, среду разработки и библиотеку GraphABC,');
  Add('на которой построен весь интерфейс редактора.');
  Add('Всем, кто пробовал редактор, присылал замечания и находил ошибки.');
  Add('');
  Add('Программа распространяется по лицензии GNU GPL версии 3.');

  ShowTextScreen('Справка', p);
end;

///Обрезает строку слева, пока она не влезет в заданную ширину.
function FitText(s: string; maxW: integer): string;
begin
  Result := s;
  if TextWidth(Result) <= maxW then exit;
  while (Length(Result) > 1) and (TextWidth('…' + Result) > maxW) do
    Result := Result.Substring(1);
  Result := '…' + Result;
end;

procedure UILabelLeft(x, y, bh: integer; caption: string; c: Color);
begin
  GraphABC.Font.Color := c;
  TextOut(x, y + (bh - TextHeight(caption)) div 2, caption);
end;

///Экран настроек. Правки применяются к копиям и записываются в Config.txt
///только по кнопке «Сохранить».
procedure ShowSettingsScreen;
var
  done: boolean;
  vHelp, vMenu, vGrid, vChecker, vAutoLast: boolean;
  vW, vH: integer;
  vStartup, vPalette: string;
  y, rowH, step, labX, ctlX: integer;
  path: string;
begin
  vHelp := CfgShowHelpOnStart;
  vMenu := CfgShowMenuOnStart;
  vGrid := CfgShowGrid;
  vChecker := CfgShowChecker;
  vAutoLast := CfgAutoLoadLast;
  vW := CfgDefaultWidth;
  vH := CfgDefaultHeight;
  vStartup := CfgStartupImage;
  vPalette := CfgPaletteFile;

  rowH := 30;
  step := 38;
  labX := 26;
  ctlX := 330;
  done := false;
  ClickPending := false;
  KeyPending := false;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 12, WIN_W, 44, 'Настройки');
    UILabel(0, 44, WIN_W, 20, 'Сохраняются в Config.txt рядом с программой', cTextDim);

    y := 76;
    UILabelLeft(labX, y, rowH, 'Показывать справку при запуске', cText);
    if UIButton(ctlX, y, 110, rowH, RuBool(vHelp), true) then vHelp := not vHelp;

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Показывать меню при запуске', cText);
    if UIButton(ctlX, y, 110, rowH, RuBool(vMenu), true) then vMenu := not vMenu;

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Показывать сетку', cText);
    if UIButton(ctlX, y, 110, rowH, RuBool(vGrid), true) then vGrid := not vGrid;

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Шахматка под прозрачными пикселями', cText);
    if UIButton(ctlX, y, 110, rowH, RuBool(vChecker), true) then vChecker := not vChecker;

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Открывать последний файл при запуске', cText);
    if UIButton(ctlX, y, 110, rowH, RuBool(vAutoLast), true) then vAutoLast := not vAutoLast;

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Ширина нового тайла', cText);
    if UIButton(ctlX, y, 44, rowH, '−', vW > MIN_TILE) then vW := ClampI(vW - 1, MIN_TILE, MAX_TILE);
    UILabel(ctlX + 48, y, 60, rowH, IntToStr(vW), cText);
    if UIButton(ctlX + 112, y, 44, rowH, '+', vW < MAX_TILE) then vW := ClampI(vW + 1, MIN_TILE, MAX_TILE);
    if UIButton(ctlX + 166, y, 50, rowH, '×2', vW * 2 <= MAX_TILE) then vW := ClampI(vW * 2, MIN_TILE, MAX_TILE);
    if UIButton(ctlX + 222, y, 50, rowH, '÷2', vW div 2 >= MIN_TILE) then vW := ClampI(vW div 2, MIN_TILE, MAX_TILE);

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Высота нового тайла', cText);
    if UIButton(ctlX, y, 44, rowH, '−', vH > MIN_TILE) then vH := ClampI(vH - 1, MIN_TILE, MAX_TILE);
    UILabel(ctlX + 48, y, 60, rowH, IntToStr(vH), cText);
    if UIButton(ctlX + 112, y, 44, rowH, '+', vH < MAX_TILE) then vH := ClampI(vH + 1, MIN_TILE, MAX_TILE);
    if UIButton(ctlX + 166, y, 50, rowH, '×2', vH * 2 <= MAX_TILE) then vH := ClampI(vH * 2, MIN_TILE, MAX_TILE);
    if UIButton(ctlX + 222, y, 50, rowH, '÷2', vH div 2 >= MIN_TILE) then vH := ClampI(vH div 2, MIN_TILE, MAX_TILE);

    y := y + step;
    UILabelLeft(labX, y, rowH, 'Стартовое изображение', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(labX, y + rowH, WIN_W - 250, y + rowH + 24, 4, 4);
    if vStartup = '' then
      UILabelLeft(labX + 6, y + rowH, 24, 'не задано', cTextDim)
    else
      UILabelLeft(labX + 6, y + rowH, 24, FitText(vStartup, WIN_W - 270), cText);
    if UIButton(WIN_W - 240, y, 110, rowH, 'Выбрать', true) then
    begin
      path := AskOpenFile('Стартовое изображение');
      if path <> '' then vStartup := path;
    end;
    if UIButton(WIN_W - 124, y, 100, rowH, 'Очистить', vStartup <> '') then vStartup := '';

    y := y + step + 24;
    UILabelLeft(labX, y, rowH, 'Файл палитры', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(labX, y + rowH, WIN_W - 250, y + rowH + 24, 4, 4);
    if vPalette = '' then
      UILabelLeft(labX + 6, y + rowH, 24, 'не задан', cTextDim)
    else
      UILabelLeft(labX + 6, y + rowH, 24, FitText(vPalette, WIN_W - 270), cText);
    if UIButton(WIN_W - 240, y, 110, rowH, 'Выбрать', true) then
    begin
      path := AskOpenFile('Файл палитры');
      if path <> '' then vPalette := path;
    end;
    if UIButton(WIN_W - 124, y, 100, rowH, 'Очистить', vPalette <> '') then vPalette := '';

    if UIButton(WIN_W div 2 - 190, H - 52, 180, 36, 'Сохранить', true) then
    begin
      CfgShowHelpOnStart := vHelp;
      CfgShowMenuOnStart := vMenu;
      CfgShowGrid := vGrid;
      CfgShowChecker := vChecker;
      CfgAutoLoadLast := vAutoLast;
      CfgDefaultWidth := vW;
      CfgDefaultHeight := vH;
      CfgStartupImage := vStartup;
      CfgPaletteFile := vPalette;
      GridOn := vGrid;
      CheckerOn := vChecker;
      if not ConfigSave(ConfigPath) then
        ShowMessage('Настройки не сохранены',
                    'Не удалось записать файл ' + ConfigPath +
                    '. Проверьте права на запись в папку программы.');
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, H - 52, 180, 36, 'Отмена', true) then done := true;

    Redraw;
    ClickPending := false;
    var key := TakeKey;
    if key = KEY_ESC then done := true;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Главное меню
// ---------------------------------------------------------------------------

procedure ShowMainMenu;
var
  done: boolean;
  y, bx, bw, bh, stp: integer;
  lastPath, lastCaption, info: string;
  lastOk: boolean;
begin
  done := false;
  ClickPending := false;
  KeyPending := false;
  bw := 380;
  bh := 34;
  stp := 40;
  bx := (WIN_W - bw) div 2;

  while not done do
  begin
    lastPath := ResolvePath(CfgLastFile);
    lastOk := FileThere(lastPath);
    if CfgLastFile = '' then lastCaption := 'Открыть последний файл — нет'
    else lastCaption := 'Открыть последний файл — ' + FileNameOnly(CfgLastFile);

    if CurrentFile = '' then info := 'Новый тайл, не сохранён'
    else info := FileNameOnly(CurrentFile);
    info := info + '   ' + IntToStr(TW) + ' на ' + IntToStr(TH);
    if Modified then info := info + '   есть несохранённые изменения';

    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 18, WIN_W, 54, APP_TITLE);
    UILabel(0, 56, WIN_W, 22, info, cTextDim);

    y := 92;
    if UIButton(bx, y, bw, bh, 'Продолжить редактирование', true) then done := true;

    y := y + stp;
    if UIButton(bx, y, bw, bh, FitText(lastCaption, bw - 20), lastOk) then
    begin
      CommandOpenLast;
      done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Создать файл', true) then
    begin
      CommandNew;
      done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Загрузить файл', true) then
    begin
      CommandOpen;
      done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Сохранить', true) then
    begin
      if CommandSave then done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Сохранить как', true) then
    begin
      if CommandSaveAs then done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Настройки', true) then ShowSettingsScreen;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Справка и благодарности', true) then ShowHelpScreen;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Выход', true) then CommandExit;

    UILabel(0, H - 32, WIN_W, 22,
            'версия ' + APP_VERSION + '   ·   настройки в Config.txt   ·   Escape закрывает меню',
            cTextDim);

    Redraw;
    ClickPending := false;
    var key := TakeKey;
    if key = KEY_ESC then done := true;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Инструменты холста
// ---------------------------------------------------------------------------

///Штрих карандашом. Промежуточные точки достраиваются отрезком,
///поэтому при быстром движении мыши в линии не остаётся пропусков.
procedure DoPenStroke(startX, startY: integer);
var
  lastX, lastY, mx, my: integer;
begin
  PushUndo;
  SetLength(StrokeMask, TW, TH);
  LiveDraw := true;
  lastX := startX;
  lastY := startY;

  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    PaintPixel(startX, startY);
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  Redraw;

  while MousePressed do
  begin
    mx := ScreenToTileX(MouseX);
    my := ScreenToTileY(MouseY);
    if (mx <> lastX) or (my <> lastY) then
    begin
      System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
      try
        PaintLine(lastX, lastY, mx, my);
      finally
        System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
      end;
      lastX := mx;
      lastY := my;
      Redraw;
    end;
    Idle;
  end;

  LiveDraw := false;
  SetLength(StrokeMask, 0, 0);
  NeedRepaint := true;
end;

///Протяжка рамки выделения. Обе граничные точки заведомо лежат внутри тайла,
///поэтому прямоугольник не может выйти за края массива.
procedure DefineSelection(ax, ay: integer);
var
  bx, by, nx, ny: integer;
begin
  bx := ax;
  by := ay;
  HasSelection := true;
  SelX := ax;
  SelY := ay;
  SelW := 1;
  SelH := 1;

  while MousePressed do
  begin
    nx := ClampI(ScreenToTileX(MouseX), 0, TW - 1);
    ny := ClampI(ScreenToTileY(MouseY), 0, TH - 1);
    if (nx <> bx) or (ny <> by) then
    begin
      bx := nx;
      by := ny;
      SelX := Min(ax, bx);
      SelY := Min(ay, by);
      SelW := Abs(bx - ax) + 1;
      SelH := Abs(by - ay) + 1;
      RepaintCanvasOnly;
    end;
    Idle;
  end;

  // Щелчок без протяжки означает вставку буфера в эту точку.
  if (bx = ax) and (by = ay) then
  begin
    HasSelection := false;
    if (Length(Buffer, 0) > 0) and (Length(Buffer, 1) > 0) then
    begin
      PushUndo;
      PasteBufferAt(ax, ay);
    end;
    NeedRepaint := true;
    exit;
  end;

  CopySelectionToBuffer;
  NeedRepaint := true;
end;

///Перетаскивание выделения. Содержимое копируется на новое место,
///исходная область остаётся нетронутой.
procedure DragSelection(mx, my: integer);
var
  offX, offY, nx, ny: integer;
begin
  CopySelectionToBuffer;
  offX := mx - SelX;
  offY := my - SelY;
  DraggingSelection := true;

  while MousePressed do
  begin
    nx := ScreenToTileX(MouseX) - offX;
    ny := ScreenToTileY(MouseY) - offY;
    if (nx <> SelX) or (ny <> SelY) then
    begin
      SelX := nx;
      SelY := ny;
      RepaintCanvasOnly;
    end;
    Idle;
  end;

  DraggingSelection := false;
  PushUndo;
  PasteBufferAt(SelX, SelY);
  SelW := Min(SelW, TW);
  SelH := Min(SelH, TH);
  SelX := ClampI(SelX, 0, TW - SelW);
  SelY := ClampI(SelY, 0, TH - SelH);
  NeedRepaint := true;
end;

procedure DoSelectTool(mx, my: integer);
begin
  if HasSelection and (SelW > 0) and (SelH > 0) and
     (mx >= SelX) and (mx < SelX + SelW) and
     (my >= SelY) and (my < SelY + SelH) then
    DragSelection(mx, my)
  else
    DefineSelection(mx, my);
end;

procedure HandleCanvasClick;
var
  mx, my: integer;
begin
  if IsMiddle(ClickButton) then
  begin
    SwapColors;
    exit;
  end;

  mx := ScreenToTileX(ClickX);
  my := ScreenToTileY(ClickY);

  if not InTile(mx, my) then
  begin
    if (Tool = TOOL_SELECT) and HasSelection then
    begin
      ResetSelection;
      NeedRepaint := true;
    end;
    exit;
  end;

  if ClickButton = MB_RIGHT then
  begin
    PickColor(Tile[mx, my]);
    exit;
  end;

  case Tool of
    TOOL_PEN: DoPenStroke(mx, my);
    TOOL_FILL:
      begin
        PushUndo;
        FloodFill(mx, my, FirstColor);
        NeedRepaint := true;
      end;
    TOOL_SELECT: DoSelectTool(mx, my);
  end;
end;

// ---------------------------------------------------------------------------
// Панель инструментов
// ---------------------------------------------------------------------------

procedure DragHSVSquare;
begin
  while MousePressed do
  begin
    Sat := AxisToPercent(MouseX - PANEL_X, BAR_SIZE);
    Val := AxisToValue(MouseY - HSV_Y0, BAR_SIZE);
    ApplyHSVToColor;
    RepaintColorOnly;
    Idle;
  end;
  NeedRepaint := true;
end;

procedure DragHueBar;
begin
  while MousePressed do
  begin
    Hue := AxisToHue(MouseX - PANEL_X, BAR_SIZE);
    ApplyHSVToColor;
    RepaintColorOnly;
    Idle;
  end;
  NeedRepaint := true;
end;

procedure DragAlphaBar;
begin
  while MousePressed do
  begin
    FirstColor := ARGB(AxisToByte(MouseX - PANEL_X, BAR_SIZE),
                       FirstColor.R, FirstColor.G, FirstColor.B);
    RepaintColorOnly;
    Idle;
  end;
  NeedRepaint := true;
end;

procedure HandlePaletteClick;
var
  cj, ci: integer;
begin
  if IsMiddle(ClickButton) then
  begin
    PaletteIO;
    exit;
  end;
  if not PalHit(ClickX, ClickY, cj, ci) then exit;
  if ClickButton = MB_RIGHT then
    PickColor(Palette[cj, ci])
  else
  begin
    Palette[cj, ci] := FirstColor;
    NeedRepaint := true;
  end;
end;

procedure HandleTileSetClick;
var
  idx: integer;
begin
  if IsMiddle(ClickButton) then
  begin
    TileSetIO;
    exit;
  end;
  if not TSetHit(ClickX, ClickY, idx) then exit;

  if ClickButton = MB_RIGHT then
  begin
    if (Length(TileSet[idx], 0) > 0) and (Length(TileSet[idx], 1) > 0) then
    begin
      Buffer := CloneGrid(TileSet[idx]);
      NeedRepaint := true;
    end;
  end
  else
    if (Length(Buffer, 0) > 0) and (Length(Buffer, 1) > 0) then
    begin
      TileSet[idx] := CloneGrid(Buffer);
      NeedRepaint := true;
    end;
end;

///Разбор клика по панели. Ветки взаимоисключающие, поэтому клик
///обрабатывается ровно одной секцией.
procedure HandlePanelClick;
var
  py, idx: integer;
begin
  py := ClickY;

  if (py >= PAL_Y0) and (py < PAL_Y1) then
    HandlePaletteClick
  else if (py >= HSV_Y0) and (py < HSV_Y1) then
  begin
    if IsMiddle(ClickButton) then ShowColorScreen else DragHSVSquare;
  end
  else if (py >= HUE_Y0) and (py < HUE_Y1) then
  begin
    if IsMiddle(ClickButton) then ShowColorScreen else DragHueBar;
  end
  else if (py >= ALPHA_Y0) and (py < ALPHA_Y1) then
  begin
    if IsMiddle(ClickButton) then ShowColorScreen else DragAlphaBar;
  end
  else if (py >= TOOLS_Y0) and (py < TOOLS_Y1) then
  begin
    if ToolHit(ClickX, ClickY, idx) then
    begin
      Tool := idx;
      NeedRepaint := true;
    end;
  end
  else if (py >= TSET_Y0) and (py < TSET_Y1) then
    HandleTileSetClick
  else if (py >= MENU_Y0) and (py < MENU_Y1) then
    ShowMainMenu;
end;

// ---------------------------------------------------------------------------
// Клавиатура
// ---------------------------------------------------------------------------

procedure HandleEditorKey(key: integer);
var
  step: integer;
begin
  step := Max(1, PixelSize * 4);
  case key of
    KEY_ESC: ShowMainMenu;
    KEY_M: ShowMainMenu;
    KEY_H: ShowHelpScreen;
    KEY_N: CommandNew;
    KEY_L: CommandOpen;
    KEY_S: CommandSave;
    KEY_A: CommandSaveSelection;
    KEY_P: ShowColorScreen;
    KEY_Z: DoUndo;
    KEY_Y: DoRedo;
    KEY_X: SwapColors;
    KEY_R: RandomizeTile;
    KEY_DELETE: ClearTile;
    KEY_V:
      if (Length(Buffer, 0) > 0) and (Length(Buffer, 1) > 0) then
      begin
        PushUndo;
        PasteBufferAt(ScreenToTileX(MouseX), ScreenToTileY(MouseY));
        NeedRepaint := true;
      end;
    KEY_G:
      begin
        GridOn := not GridOn;
        CfgShowGrid := GridOn;
        ConfigSave(ConfigPath);
        NeedRepaint := true;
      end;
    KEY_C:
      begin
        CheckerOn := not CheckerOn;
        CfgShowChecker := CheckerOn;
        ConfigSave(ConfigPath);
        NeedRepaint := true;
      end;
    KEY_1: begin Tool := TOOL_PEN; NeedRepaint := true end;
    KEY_2: begin Tool := TOOL_FILL; NeedRepaint := true end;
    KEY_3: begin Tool := TOOL_SELECT; NeedRepaint := true end;
    KEY_PLUS, KEY_NUMPLUS: ZoomBy(1);
    KEY_MINUS, KEY_NUMMINUS: ZoomBy(-1);
    KEY_LEFT: begin ViewX := ViewX + step; ClampView; NeedRepaint := true end;
    KEY_RIGHT: begin ViewX := ViewX - step; ClampView; NeedRepaint := true end;
    KEY_UP: begin ViewY := ViewY + step; ClampView; NeedRepaint := true end;
    KEY_DOWN: begin ViewY := ViewY - step; ClampView; NeedRepaint := true end;
    KEY_SPACE:
      begin
        FitScale;
        NeedRepaint := true;
      end;
  end;
end;

// ---------------------------------------------------------------------------
// Запуск
// ---------------------------------------------------------------------------

///Пытается загрузить картинку. Отсутствие файла не считается ошибкой:
///вместо значка будет нарисована подпись.
function TryLoadPicture(path: string): Picture;
begin
  Result := nil;
  try
    if System.IO.File.Exists(path) then Result := Picture.Create(path);
  except
    Result := nil;
  end;
end;

procedure LoadToolIcons;
var
  names: array of string;
begin
  SetLength(names, 6);
  names[0] := '_Pen.png';
  names[1] := 'Fill.png';
  names[2] := 'Vd.png';
  names[3] := 'sPen.png';
  names[4] := 'sFill.png';
  names[5] := 'sVd.png';
  SetLength(ToolPics, 6);
  for var i := 0 to 5 do
    ToolPics[i] := TryLoadPicture(ResolvePath(System.IO.Path.Combine('Resources', names[i])));
end;

procedure InitCollections;
var
  empty: array[,] of Color;
begin
  SetLength(Palette, PAL_COLS, PAL_ROWS);
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
      Palette[j, i] := HSVtoRGB(j * 45, 30 + i * 10, 100 - i * 8);

  SetLength(TileSet, TSET_COLS * TSET_ROWS);
  SetLength(empty, 0, 0);
  for var k := 0 to Length(TileSet) - 1 do
    TileSet[k] := empty;

  SetLength(Buffer, 0, 0);
  SetLength(StrokeMask, 0, 0);
  ClearHistory;
end;

procedure InitColorBars;
begin
  SetLength(RawHSV, BAR_SIZE * BAR_SIZE * 4);
  SetLength(RawHue, BAR_SIZE * BAR_H * 4);
  SetLength(RawAlpha, BAR_SIZE * BAR_H * 4);
  RenderHuePlane(RawHue, BAR_SIZE);
  BmpHue := BytesToImage(RawHue, BAR_SIZE);
  RenderAlphaPlane(RawAlpha, BAR_SIZE);
  BmpAlpha := BytesToImage(RawAlpha, BAR_SIZE);
  CachedHue := -1;
end;

///Заголовок окна показывает имя файла и признак изменений.
procedure SyncTitle;
var
  t: string;
begin
  t := APP_TITLE + ' ' + APP_VERSION + ' — ';
  if CurrentFile = '' then t := t + 'новый тайл'
  else t := t + FileNameOnly(CurrentFile);
  if Modified then t := t + ' *';
  if t <> LastTitle then
  begin
    LastTitle := t;
    Window.Title := t;
  end;
end;

procedure AppInit;
var
  loaded: boolean;
begin
  BaseDir := DetectBaseDir;
  ConfigPath := ResolvePath('Config.txt');

  Window.SetSize(WIN_W, WIN_H);
  W := CANVAS_W;
  H := CANVAS_H;
  CenterWindow;
  Window.IsFixedSize := true;
  Window.Title := APP_TITLE + ' ' + APP_VERSION;
  LockDrawing;
  GraphABC.Font.Size := 10;

  OnKeyDown := OnKeyDownHandler;
  OnKeyUp := OnKeyUpHandler;
  OnMouseDown := OnMouseDownHandler;
  OnMouseMove := OnMouseMoveHandler;
  OnMouseUp := OnMouseUpHandler;
  GraphABC.GraphABCControl.MouseWheel += OnWheelHandler;

  // Настройки. Отсутствующий файл не ошибка: берутся значения по умолчанию
  // и файл создаётся, чтобы его было легко найти и поправить.
  if not ConfigLoad(ConfigPath) then ConfigSave(ConfigPath);
  GridOn := CfgShowGrid;
  CheckerOn := CfgShowChecker;

  FirstColor := ARGB(255, 0, 0, 0);
  SecondColor := ARGB(255, 255, 255, 255);
  SyncHSVFromColor;

  InitCollections;
  InitColorBars;
  LoadToolIcons;
  if CfgPaletteFile <> '' then LoadPaletteFrom(ResolvePath(CfgPaletteFile), true);

  // Порядок открытия документа при запуске.
  // Размер из настроек нужен только тогда, когда открыть картинку не удалось.
  loaded := false;
  if CfgAutoLoadLast and (CfgLastFile <> '') then
    loaded := LoadTileFrom(ResolvePath(CfgLastFile), true, false);
  if (not loaded) and (CfgStartupImage <> '') then
    loaded := LoadTileFrom(ResolvePath(CfgStartupImage), true, false);
  if not loaded then
    NewTileOfSize(CfgDefaultWidth, CfgDefaultHeight);

  SyncTitle;
  Repaint;

  if CfgShowHelpOnStart then ShowHelpScreen;
  if CfgShowMenuOnStart then ShowMainMenu;
end;

///Один шаг главного цикла: разбор ввода и перерисовка по необходимости.
///Ожидание всегда идёт через Idle, поэтому в простое программа
///не нагружает процессор.
procedure AppStep;
var
  wheel, key, tx, ty: integer;
begin
  if NeedRepaint then Repaint;
  SyncTitle;

  // Колесо могло провернуться на несколько щелчков между кадрами.
  wheel := TakeWheel;
  while wheel > 0 do
  begin
    ZoomBy(1);
    wheel := wheel - 1;
  end;
  while wheel < 0 do
  begin
    ZoomBy(-1);
    wheel := wheel + 1;
  end;

  key := TakeKey;
  if key <> 0 then HandleEditorKey(key);

  if ClickPending then
  begin
    ClickPending := false;
    if ClickX < W then HandleCanvasClick else HandlePanelClick;
  end;

  // Координаты курсора обновляются точечно, без полной перерисовки окна.
  tx := ScreenToTileX(MouseX);
  ty := ScreenToTileY(MouseY);
  if (tx <> LastStatX) or (ty <> LastStatY) then
  begin
    LastStatX := tx;
    LastStatY := ty;
    if not NeedRepaint then
    begin
      System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
      try
        DrawStatusSection;
      finally
        System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
      end;
      Redraw;
    end;
  end;

  Idle;
end;

begin
  AppInit;
  while true do
    AppStep;
end.
