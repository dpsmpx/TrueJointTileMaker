///True Joint Tile Maker — редактор пиксельной графики, тайлсетов и карт локаций.
///
///Два режима работы над одним окном:
///  тайл — рисование пиксельной картинки;
///  карта — локация игры Andors-Love в формате data/maps/*.map.
///Оба режима пользуются общими панорамой, масштабом, историей отмен,
///выделением и обзорной панелью.
///
///Настройки хранятся в Config.txt (TJTMConfig), цветовая математика — в
///TJTMColor, разбор и запись карт — в TJTMMap.
///
///Соглашение по кнопкам мыши, единое для всей программы:
///  ЛКМ  — применить: рисовать, залить, положить цвет, поставить объект;
///  ПКМ  — взять: пипетка цвета или местности, убрать объект;
///  ролик как кнопка — служебное: протяжка панорамирует, щелчок без
///         протяжки меняет местами рабочие цвета.
uses GraphABC, System.Drawing, System.Drawing.Imaging, TJTMColor, TJTMConfig, TJTMMap;

const
  APP_TITLE = 'True Joint Tile Maker';
  APP_VERSION = '0.5.0';

  ///Холст редактирования и панель справа от него.
  CANVAS_W = 600;
  CANVAS_H = 560;
  PANEL_X = CANVAS_W;
  PANEL_W = 180;
  WIN_W = CANVAS_W + PANEL_W;
  WIN_H = CANVAS_H;

  MIN_TILE = CFG_MIN_TILE;
  MAX_TILE = CFG_MAX_TILE;

  PAL_COLS = 8;
  PAL_ROWS = 8;
  ///Пиксель не связан ни с одной ячейкой палитры.
  IDX_NONE = 255;

  ///Вертикальная разметка панели. Отрисовка и попадание мыши считают
  ///границы по одним и тем же константам.
  TOOLS_Y0 = 4;     TOOLS_Y1 = 112;
  SLOT1_Y0 = 118;   SLOT1_Y1 = 246;
  SLOT2_Y0 = 250;   SLOT2_Y1 = 350;
  HUE_Y0 = 352;     HUE_Y1 = 364;
  ALPHA_Y0 = 366;   ALPHA_Y1 = 378;
  OVER_Y0 = 384;    OVER_Y1 = 480;
  INFO_Y0 = 484;    INFO_Y1 = 528;
  MENU_Y0 = 532;    MENU_Y1 = 556;

  ///В режиме тайла SLOT1 занимает палитра, SLOT2 — квадрат насыщенности.
  ///В режиме карты SLOT1 — виды местности, SLOT2 — виды объектов.
  PAL_Y0 = SLOT1_Y0;   PAL_Y1 = SLOT1_Y1;
  HSV_Y0 = SLOT2_Y0;   HSV_Y1 = SLOT2_Y1;

  TOOL_COLS = 4;
  TOOL_CW = 44;
  TOOL_CH = 36;

  BAR_SIZE = 100;
  BAR_H = 12;

  ///Инструменты режима тайла.
  T_PEN = 0;
  T_ERASER = 1;
  T_FILL = 2;
  T_PICK = 3;
  T_LINE = 4;
  T_RECT = 5;
  T_ELLIPSE = 6;
  T_DITHER = 7;
  T_SELECT = 8;
  T_TILE_COUNT = 9;

  ///Инструменты режима карты.
  M_PAINT = 0;
  M_FILL = 1;
  M_RECT = 2;
  M_OBJ = 3;
  M_SELECT = 4;
  T_MAP_COUNT = 5;

  DOC_TILE = 0;
  DOC_MAP = 1;

  MIN_ZOOM = 1;
  MAX_ZOOM = 64;

  ///Верхняя граница памяти под историю отмен.
  UNDO_BUDGET_BYTES = 6 * 1024 * 1024;
  ///Выше этого числа клеток бесшовный предпросмотр отключается:
  ///перерисовка стала бы заметно медленнее пользы от неё.
  SEAMLESS_CELL_LIMIT = 40000;

  ///Коды клавиш заданы числами, чтобы не зависеть от набора VK_* в GraphABC.
  KEY_BACK = 8;    KEY_TAB = 9;     KEY_ENTER = 13;  KEY_ESC = 27;
  KEY_SPACE = 32;
  KEY_LEFT = 37;   KEY_UP = 38;     KEY_RIGHT = 39;  KEY_DOWN = 40;
  KEY_DELETE = 46;
  KEY_0 = 48;      KEY_1 = 49;      KEY_2 = 50;      KEY_3 = 51;
  KEY_4 = 52;      KEY_5 = 53;      KEY_6 = 54;      KEY_7 = 55;
  KEY_8 = 56;      KEY_9 = 57;
  KEY_A = 65;      KEY_B = 66;      KEY_C = 67;      KEY_D = 68;
  KEY_E = 69;      KEY_F = 70;      KEY_G = 71;      KEY_H = 72;
  KEY_I = 73;      KEY_J = 74;      KEY_K = 75;      KEY_L = 76;
  KEY_M = 77;      KEY_N = 78;      KEY_O = 79;      KEY_P = 80;
  KEY_Q = 81;      KEY_R = 82;      KEY_S = 83;      KEY_T = 84;
  KEY_U = 85;      KEY_V = 86;      KEY_W = 87;      KEY_X = 88;
  KEY_Y = 89;      KEY_Z = 90;
  KEY_SHIFT = 16;  KEY_CTRL = 17;   KEY_ALT = 18;
  KEY_NUMPLUS = 107; KEY_NUMMINUS = 109;
  KEY_PLUS = 187;  KEY_MINUS = 189;

  MB_LEFT = 1;
  MB_RIGHT = 2;

var
  ///Размеры холста.
  W: integer := CANVAS_W;
  H: integer := CANVAS_H;

  ///Что редактируется сейчас: тайл или карта.
  DocMode: integer := DOC_TILE;

  ///Документ режима тайла.
  TW: integer := 32;
  TH: integer := 32;
  Tile: array[,] of Color;

  ///Буфер обмена цветов. Живёт отдельно от выделения и переживает смену тайла.
  Buffer: array[,] of Color;
  ///Буфер обмена клеток карты.
  MapBuffer: array[,] of byte;

  ///Прямоугольник выделения в координатах документа.
  SelX, SelY, SelW, SelH: integer;
  HasSelection: boolean;
  DraggingSelection: boolean;

  Palette: array[,] of Color;
  ///Привязка пикселей тайла к ячейкам палитры: номер ячейки или IDX_NONE.
  ///Массив идёт рядом с Tile и всегда того же размера.
  TileIdx: array[,] of byte;
  ///Индексный режим: правка ячейки палитры перекрашивает связанные пиксели.
  IndexedOn: boolean;

  ///Панорама и масштаб, общие для обоих режимов.
  PixelSize: integer := 8;
  ViewX, ViewY: integer;
  GridOn: boolean;
  CheckerOn: boolean := true;
  ///Бесшовный предпросмотр: тайл рисуется повторяющимся.
  SeamlessOn: boolean;
  ///Рисование заворачивается через край тайла.
  WrapOn: boolean;
  ///Оси симметрии при рисовании.
  SymX, SymY: boolean;
  ///Кисть пишет цвет поверх, не смешивая. Ластик всегда работает так.
  BrushReplace: boolean;

  FirstColor: Color;
  SecondColor: Color;
  ///Ячейки палитры, из которых взяты рабочие цвета. Держатся согласованными
  ///с самим цветом: индекс либо IDX_NONE, либо указывает на ячейку,
  ///чей цвет совпадает с рабочим.
  FirstIdx: byte := IDX_NONE;
  SecondIdx: byte := IDX_NONE;
  Hue, Sat, Val: integer;

  ///Выбранный инструмент отдельно для каждого режима.
  ToolTile: integer := T_PEN;
  ToolMap: integer := M_PAINT;

  ///Выбранная местность и вид объекта в режиме карты.
  MapBrush: integer := MT_GRASS;
  MapObjKind: integer := 0;

  CurrentFile: string := '';
  CurrentMapFile: string := '';
  ///Признак несохранённых правок ведётся для каждого документа отдельно,
  ///иначе переключение режима теряло бы состояние второго.
  TileModified, MapModified: boolean;

  ///История отмен. Для каждого режима своя, снимками целого документа.
  UndoStack: array of array[,] of Color;
  RedoStack: array of array[,] of Color;
  ///Привязки к палитре ходят по истории вместе с цветами: иначе отмена
  ///вернула бы прежние цвета, но оставила привязки от отменённой операции.
  UndoIdx: array of array[,] of byte;
  RedoIdx: array of array[,] of byte;
  ///Сама палитра тоже. В индексном режиме правка ячейки меняет тайл, и
  ///отменять их порознь бессмысленно: пиксели вернулись бы к прежнему цвету,
  ///а ячейка осталась новой, и связь между ними потерялась бы.
  UndoPal: array of array[,] of Color;
  RedoPal: array of array[,] of Color;
  MapUndoGrid: array of array[,] of byte;
  MapRedoGrid: array of array[,] of byte;
  MapUndoObjs: array of array of MapObject;
  MapRedoObjs: array of array of MapObject;

  ///Маска текущего штриха: пиксель закрашивается не более одного раза,
  ///иначе полупрозрачная кисть накладывалась бы сама на себя.
  StrokeMask: array[,] of boolean;

  ///Кэш полос выбора цвета.
  RawHSV: array of byte;
  RawHue: array of byte;
  RawAlpha: array of byte;
  BmpHSV, BmpHue, BmpAlpha: Bitmap;
  CachedHue: integer := -1;
  ///Кэш обзорной панели. Картинка пересобирается только когда документ
  ///изменился: при панорамировании двигается лишь рамка видимой области.
  BmpOver: Bitmap;
  RawOver: array of byte;
  OverDirty: boolean := true;
  OverCacheW: integer := -1;
  OverCacheH: integer := -1;
  OverCacheMode: integer := -1;

  ///Состояние ввода.
  KeyPressed: boolean;
  KeyPending: boolean;
  PendingKey: integer;
  ShiftDown, CtrlDown, AltDown: boolean;
  MouseX, MouseY: integer;
  MousePressed: boolean;
  ClickPending: boolean;
  ClickX, ClickY, ClickButton: integer;
  WheelAccum: integer;
  ///Приём текста работает только пока открыто поле ввода.
  TextInputActive: boolean;
  TextInputBuf: string;

  NeedRepaint: boolean := true;
  ConfigPath: string;
  BaseDir: string;
  LiveDraw: boolean;
  ///Фигура рисуется предпросмотром, а не в документ.
  ShapePreview: boolean;
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
  cWarn := RGB(190, 60, 40);

///Опережающие объявления: обе процедуры нужны раньше, чем описана отрисовка.
procedure DrawTilePixel(x, y: integer); forward;
procedure DrawPreviewPixel(x, y: integer); forward;

// ---------------------------------------------------------------------------
// Мелкие утилиты
// ---------------------------------------------------------------------------

///Короткая пауза. Стоит во всех циклах ожидания, чтобы программа
///не занимала ядро процессора вхолостую.
procedure Idle;
begin
  System.Threading.Thread.Sleep(4);
end;

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
procedure RememberFile(p: string; isMap: boolean);
begin
  if p = '' then exit;
  if isMap then CfgLastMapFile := p else CfgLastFile := p;
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
  if key = KEY_SHIFT then ShiftDown := true;
  if key = KEY_CTRL then CtrlDown := true;
  if key = KEY_ALT then AltDown := true;
  PendingKey := key;
  KeyPending := true;
end;

procedure OnKeyUpHandler(key: integer);
begin
  KeyPressed := false;
  if key = KEY_SHIFT then ShiftDown := false;
  if key = KEY_CTRL then CtrlDown := false;
  if key = KEY_ALT then AltDown := false;
end;

///Символы собираются только когда открыто поле ввода. Управляющие коды
///приходят через OnKeyDown, поэтому здесь они отбрасываются.
procedure OnKeyPressHandler(ch: char);
begin
  if not TextInputActive then exit;
  if ch < ' ' then exit;
  if Length(TextInputBuf) > 400 then exit;
  TextInputBuf := TextInputBuf + ch;
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

///Колесо только копит отсчёты: рисовать из обработчика события нельзя,
///он выполняется в потоке интерфейса параллельно основному циклу.
procedure OnWheelHandler(Sender: object; e: System.Windows.Forms.MouseEventArgs);
begin
  WheelAccum := WheelAccum + Sign(e.Delta);
end;

function TakeWheel: integer;
begin
  Result := WheelAccum;
  WheelAccum := 0;
end;

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

function IsMiddle(b: integer): boolean;
begin
  Result := (b <> MB_LEFT) and (b <> MB_RIGHT);
end;

// ---------------------------------------------------------------------------
// Координаты документа. Панорама, масштаб и обзор одинаковы для обоих режимов,
// поэтому размер документа спрашивается через DocW и DocH.
// ---------------------------------------------------------------------------

function DocW: integer;
begin
  if DocMode = DOC_MAP then Result := MapW else Result := TW;
end;

function DocH: integer;
begin
  if DocMode = DOC_MAP then Result := MapH else Result := TH;
end;

///Есть ли несохранённые правки в текущем документе.
function IsModified: boolean;
begin
  if DocMode = DOC_MAP then Result := MapModified else Result := TileModified;
end;

procedure MarkModified;
begin
  if DocMode = DOC_MAP then MapModified := true else TileModified := true;
  OverDirty := true;
end;

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

///Лежит ли клетка внутри документа. Единственная проверка границ,
///которой пользуются все операции.
function InDoc(x, y: integer): boolean;
begin
  Result := (x >= 0) and (y >= 0) and (x < DocW) and (y < DocH);
end;

function GridGap: integer;
begin
  if GridOn and (PixelSize >= 4) then Result := 1 else Result := 0;
end;

procedure ClampView;
var
  fullW, fullH: integer;
begin
  fullW := DocW * PixelSize;
  fullH := DocH * PixelSize;
  if fullW <= W then ViewX := (W - fullW) div 2
  else ViewX := ClampI(ViewX, W - fullW, 0);
  if fullH <= H then ViewY := (H - fullH) div 2
  else ViewY := ClampI(ViewY, H - fullH, 0);
end;

procedure FitScale;
begin
  PixelSize := Min(W div Max(1, DocW), H div Max(1, DocH));
  PixelSize := ClampI(PixelSize, MIN_ZOOM, MAX_ZOOM);
  ClampView;
end;

///Масштабирование вокруг точки экрана: клетка под курсором остаётся на месте.
procedure ZoomAt(dir, ax, ay: integer);
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
  if PixelSize = old then exit;
  ViewX := ax - Round((ax - ViewX) * PixelSize / old);
  ViewY := ay - Round((ay - ViewY) * PixelSize / old);
  ClampView;
  NeedRepaint := true;
end;

procedure ResetSelection;
begin
  SelX := 0;
  SelY := 0;
  SelW := 0;
  SelH := 0;
  HasSelection := false;
  DraggingSelection := false;
end;

// ---------------------------------------------------------------------------
// Привязка пикселей к палитре.
//
// У каждого пикселя тайла есть номер ячейки палитры, из которой взят его цвет,
// либо IDX_NONE. Правка ячейки перекрашивает связанные с ней пиксели, поэтому
// набор тайлов можно перекрасить целиком, не перерисовывая ни одного.
//
// Привязка — всегда только подсказка, а не источник истины: цвет пикселя
// остаётся в Tile, и перед перекраской он сверяется с прежним цветом ячейки.
// Поэтому устаревшая привязка не может испортить картинку — она лишь не
// сработает. Это позволяет заводить привязки где удобно, не доказывая, что
// все пути записи в Tile их поддерживают.
// ---------------------------------------------------------------------------

///Сравнивает цвета, не глядя на прозрачность.
function SameRGB(a, b: Color): boolean;
begin
  Result := (a.R = b.R) and (a.G = b.G) and (a.B = b.B);
end;

///Номер ячейки палитры по её месту в сетке.
function PalIndexAt(j, i: integer): byte;
begin
  Result := i * PAL_COLS + j;
end;

///Цвет ячейки палитры по номеру. Для IDX_NONE возвращает прозрачный.
function PalColorOf(idx: byte): Color;
begin
  if idx >= PAL_COLS * PAL_ROWS then Result := ARGB(0, 0, 0, 0)
  else Result := Palette[idx mod PAL_COLS, idx div PAL_COLS];
end;

///Первая ячейка палитры такого же цвета. IDX_NONE, если такого цвета в
///палитре нет. Прозрачность не участвует: полупрозрачный мазок цветом
///палитры остаётся связанным с ней.
function IndexOfColor(c: Color): byte;
begin
  Result := IDX_NONE;
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
      if SameRGB(c, Palette[j, i]) then
      begin
        Result := PalIndexAt(j, i);
        exit;
      end;
end;

///Приводит массив привязок к размеру тайла. Несовпадение размеров означает,
///что тайл сменился, и прежние привязки к нему уже не относятся.
procedure EnsureTileIdx;
begin
  if (Length(TileIdx, 0) = TW) and (Length(TileIdx, 1) = TH) then exit;
  SetLength(TileIdx, TW, TH);
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      TileIdx[j, i] := IDX_NONE;
end;

///Записывает привязку с проверкой границ, чтобы вызывающему не приходилось
///повторять её на каждом месте записи в Tile.
procedure SetIdx(x, y: integer; v: byte);
begin
  if (Length(TileIdx, 0) <> TW) or (Length(TileIdx, 1) <> TH) then exit;
  if (x < 0) or (y < 0) or (x >= TW) or (y >= TH) then exit;
  TileIdx[x, y] := v;
end;

///Заново выводит привязки из цветов: пиксель связывается с ячейкой палитры
///того же цвета. Так тайл, нарисованный до включения режима или открытый из
///файла, становится управляемым палитрой. Возвращает число связанных пикселей.
function RebindTileToPalette: integer;
begin
  Result := 0;
  EnsureTileIdx;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
    begin
      TileIdx[j, i] := IndexOfColor(Tile[j, i]);
      if TileIdx[j, i] <> IDX_NONE then Result := Result + 1;
    end;
end;

///Перекрашивает пиксели, связанные с ячейкой палитры. Цвет каждого сверяется
///с прежним цветом ячейки: пиксель, который с тех пор перекрасили иначе,
///не трогается. Прозрачность пикселя сохраняется — меняется только цвет.
function RecolorPaletteEntry(idx: byte; oldC, newC: Color): integer;
begin
  Result := 0;
  if idx = IDX_NONE then exit;
  if (Length(TileIdx, 0) <> TW) or (Length(TileIdx, 1) <> TH) then exit;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      if (TileIdx[j, i] = idx) and SameRGB(Tile[j, i], oldC) then
      begin
        Tile[j, i] := ARGB(Tile[j, i].A, newC.R, newC.G, newC.B);
        Result := Result + 1;
      end;
end;

///Сколько пикселей тайла связано с палитрой.
function BoundPixelCount: integer;
begin
  Result := 0;
  if (Length(TileIdx, 0) <> TW) or (Length(TileIdx, 1) <> TH) then exit;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      if TileIdx[j, i] <> IDX_NONE then Result := Result + 1;
end;

// ---------------------------------------------------------------------------
// История отмен. Снимок берётся один раз на операцию, а не на каждый пиксель.
// ---------------------------------------------------------------------------

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

function CloneBytes(g: array[,] of byte): array[,] of byte;
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

function CloneObjs(src: array of MapObject): array of MapObject;
begin
  SetLength(Result, Length(src));
  for var i := 0 to Length(src) - 1 do
    Result[i] := src[i];
end;

function UndoDepth: integer;
var
  per: integer;
begin
  per := DocW * DocH * 4;
  if per < 1 then per := 1;
  Result := ClampI(UNDO_BUDGET_BYTES div per, 3, 64);
end;

procedure ClearTileHistory;
begin
  SetLength(UndoStack, 0);
  SetLength(RedoStack, 0);
  SetLength(UndoIdx, 0);
  SetLength(RedoIdx, 0);
  SetLength(UndoPal, 0);
  SetLength(RedoPal, 0);
end;

procedure ClearMapHistory;
begin
  SetLength(MapUndoGrid, 0);
  SetLength(MapRedoGrid, 0);
  SetLength(MapUndoObjs, 0);
  SetLength(MapRedoObjs, 0);
end;

procedure ClearHistory;
begin
  ClearTileHistory;
  ClearMapHistory;
end;

procedure PushUndo;
var
  limit: integer;
begin
  limit := UndoDepth;
  if DocMode = DOC_MAP then
  begin
    if (MapW < 1) or (MapH < 1) then exit;
    SetLength(MapUndoGrid, Length(MapUndoGrid) + 1);
    MapUndoGrid[Length(MapUndoGrid) - 1] := CloneBytes(MapGrid);
    SetLength(MapUndoObjs, Length(MapUndoObjs) + 1);
    MapUndoObjs[Length(MapUndoObjs) - 1] := CloneObjs(MapObjects);
    while Length(MapUndoGrid) > limit do
    begin
      for var i := 0 to Length(MapUndoGrid) - 2 do
      begin
        MapUndoGrid[i] := MapUndoGrid[i + 1];
        MapUndoObjs[i] := MapUndoObjs[i + 1];
      end;
      SetLength(MapUndoGrid, Length(MapUndoGrid) - 1);
      SetLength(MapUndoObjs, Length(MapUndoObjs) - 1);
    end;
    SetLength(MapRedoGrid, 0);
    SetLength(MapRedoObjs, 0);
  end
  else
  begin
    if (TW < 1) or (TH < 1) then exit;
    EnsureTileIdx;
    SetLength(UndoStack, Length(UndoStack) + 1);
    UndoStack[Length(UndoStack) - 1] := CloneGrid(Tile);
    SetLength(UndoIdx, Length(UndoIdx) + 1);
    UndoIdx[Length(UndoIdx) - 1] := CloneBytes(TileIdx);
    SetLength(UndoPal, Length(UndoPal) + 1);
    UndoPal[Length(UndoPal) - 1] := CloneGrid(Palette);
    while Length(UndoStack) > limit do
    begin
      for var i := 0 to Length(UndoStack) - 2 do
      begin
        UndoStack[i] := UndoStack[i + 1];
        UndoIdx[i] := UndoIdx[i + 1];
        UndoPal[i] := UndoPal[i + 1];
      end;
      SetLength(UndoStack, Length(UndoStack) - 1);
      SetLength(UndoIdx, Length(UndoIdx) - 1);
      SetLength(UndoPal, Length(UndoPal) - 1);
    end;
    SetLength(RedoStack, 0);
    SetLength(RedoIdx, 0);
    SetLength(RedoPal, 0);
  end;
  MarkModified;
end;

procedure DoUndo;
begin
  if DocMode = DOC_MAP then
  begin
    if Length(MapUndoGrid) = 0 then exit;
    SetLength(MapRedoGrid, Length(MapRedoGrid) + 1);
    MapRedoGrid[Length(MapRedoGrid) - 1] := CloneBytes(MapGrid);
    SetLength(MapRedoObjs, Length(MapRedoObjs) + 1);
    MapRedoObjs[Length(MapRedoObjs) - 1] := CloneObjs(MapObjects);
    MapGrid := MapUndoGrid[Length(MapUndoGrid) - 1];
    MapObjects := MapUndoObjs[Length(MapUndoObjs) - 1];
    SetLength(MapUndoGrid, Length(MapUndoGrid) - 1);
    SetLength(MapUndoObjs, Length(MapUndoObjs) - 1);
    MapW := Length(MapGrid, 0);
    MapH := Length(MapGrid, 1);
  end
  else
  begin
    if Length(UndoStack) = 0 then exit;
    EnsureTileIdx;
    SetLength(RedoStack, Length(RedoStack) + 1);
    RedoStack[Length(RedoStack) - 1] := CloneGrid(Tile);
    SetLength(RedoIdx, Length(RedoIdx) + 1);
    RedoIdx[Length(RedoIdx) - 1] := CloneBytes(TileIdx);
    SetLength(RedoPal, Length(RedoPal) + 1);
    RedoPal[Length(RedoPal) - 1] := CloneGrid(Palette);
    Tile := UndoStack[Length(UndoStack) - 1];
    SetLength(UndoStack, Length(UndoStack) - 1);
    TW := Length(Tile, 0);
    TH := Length(Tile, 1);
    if Length(UndoIdx) > 0 then
    begin
      TileIdx := UndoIdx[Length(UndoIdx) - 1];
      SetLength(UndoIdx, Length(UndoIdx) - 1);
    end;
    if Length(UndoPal) > 0 then
    begin
      Palette := UndoPal[Length(UndoPal) - 1];
      SetLength(UndoPal, Length(UndoPal) - 1);
    end;
    EnsureTileIdx;
  end;
  ResetSelection;
  ClampView;
  MarkModified;
  NeedRepaint := true;
end;

procedure DoRedo;
begin
  if DocMode = DOC_MAP then
  begin
    if Length(MapRedoGrid) = 0 then exit;
    SetLength(MapUndoGrid, Length(MapUndoGrid) + 1);
    MapUndoGrid[Length(MapUndoGrid) - 1] := CloneBytes(MapGrid);
    SetLength(MapUndoObjs, Length(MapUndoObjs) + 1);
    MapUndoObjs[Length(MapUndoObjs) - 1] := CloneObjs(MapObjects);
    MapGrid := MapRedoGrid[Length(MapRedoGrid) - 1];
    MapObjects := MapRedoObjs[Length(MapRedoObjs) - 1];
    SetLength(MapRedoGrid, Length(MapRedoGrid) - 1);
    SetLength(MapRedoObjs, Length(MapRedoObjs) - 1);
    MapW := Length(MapGrid, 0);
    MapH := Length(MapGrid, 1);
  end
  else
  begin
    if Length(RedoStack) = 0 then exit;
    EnsureTileIdx;
    SetLength(UndoStack, Length(UndoStack) + 1);
    UndoStack[Length(UndoStack) - 1] := CloneGrid(Tile);
    SetLength(UndoIdx, Length(UndoIdx) + 1);
    UndoIdx[Length(UndoIdx) - 1] := CloneBytes(TileIdx);
    SetLength(UndoPal, Length(UndoPal) + 1);
    UndoPal[Length(UndoPal) - 1] := CloneGrid(Palette);
    Tile := RedoStack[Length(RedoStack) - 1];
    SetLength(RedoStack, Length(RedoStack) - 1);
    TW := Length(Tile, 0);
    TH := Length(Tile, 1);
    if Length(RedoIdx) > 0 then
    begin
      TileIdx := RedoIdx[Length(RedoIdx) - 1];
      SetLength(RedoIdx, Length(RedoIdx) - 1);
    end;
    if Length(RedoPal) > 0 then
    begin
      Palette := RedoPal[Length(RedoPal) - 1];
      SetLength(RedoPal, Length(RedoPal) - 1);
    end;
    EnsureTileIdx;
  end;
  ResetSelection;
  ClampView;
  MarkModified;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Документ
// ---------------------------------------------------------------------------

procedure SetTile(newTile: array[,] of Color; fileName: string);
begin
  Tile := newTile;
  TW := Length(Tile, 0);
  TH := Length(Tile, 1);
  ///Открытый файл ничего не знает о палитре, поэтому привязки выводятся
  ///из его же цветов: совпал с ячейкой — значит, ею и управляется.
  RebindTileToPalette;
  CurrentFile := fileName;
  DocMode := DOC_TILE;
  TileModified := false;
  ResetSelection;
  ClearTileHistory;
  OverDirty := true;
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

function CheckerColor(x, y: integer): Color;
begin
  if ((x div 4) + (y div 4)) mod 2 = 0 then Result := cCheckA else Result := cCheckB;
end;

function DisplayColor(x, y: integer): Color;
begin
  Result := Tile[x, y];
  if CheckerOn and (Result.A < 255) then
    Result := BlendOver(Result, CheckerColor(x, y));
end;

///Цвет местности на карте.
function MapTileColor(code: integer): Color;
begin
  case code of
    MT_FLOOR: Result := RGB(196, 184, 160);
    MT_WALL: Result := RGB(72, 70, 76);
    MT_WATER: Result := RGB(58, 110, 190);
    MT_TREE: Result := RGB(34, 96, 52);
    MT_GRASS: Result := RGB(108, 168, 78);
    MT_ROAD: Result := RGB(168, 146, 108);
    MT_DEADWATER: Result := RGB(80, 128, 128);
    else Result := RGB(200, 0, 200);
  end;
end;

///Буква, которой объект показан на карте. Взяты обозначения из игры.
function MapObjGlyph(kind: string): string;
begin
  if kind = 'npc' then Result := 'N'
  else if kind = 'item' then Result := '*'
  else if kind = 'exit' then Result := '>'
  else if kind = 'spawn' then Result := 'X'
  else if kind = 'chest' then Result := 'C'
  else if kind = 'note' then Result := 'n'
  else if kind = 'bed' then Result := '&'
  else if kind = 'sign' then Result := '!'
  else Result := '?';
end;

// ---------------------------------------------------------------------------
// Кисть
// ---------------------------------------------------------------------------

///Цвет, которым кисть пишет в эту клетку. Ластик пишет прозрачность,
///дизеринг чередует два рабочих цвета в шахматном порядке.
function BrushColorAt(x, y: integer): Color;
begin
  if ToolTile = T_ERASER then Result := ARGB(0, 0, 0, 0)
  else if ToolTile = T_DITHER then
  begin
    if ((x + y) mod 2) = 0 then Result := FirstColor else Result := SecondColor;
  end
  else
    Result := FirstColor;
end;

///Пишет ли кисть поверх, не смешивая. Ластик иначе не смог бы стирать:
///наложение прозрачного цвета по правилу source-over ничего не меняет.
function BrushReplaces: boolean;
begin
  Result := BrushReplace or (ToolTile = T_ERASER);
end;

///Ячейка палитры, из которой взят цвет кисти для этой клетки. Повторяет
///разбор инструментов из BrushColorAt, чтобы цвет и его привязка не разошлись.
function BrushIndexAt(x, y: integer): byte;
begin
  if ToolTile = T_ERASER then Result := IDX_NONE
  else if ToolTile = T_DITHER then
  begin
    if ((x + y) mod 2) = 0 then Result := FirstIdx else Result := SecondIdx;
  end
  else
    Result := FirstIdx;
end;

///Одна клетка тайла с учётом заворота через край и маски штриха.
procedure PaintOne(x, y: integer);
var
  c: Color;
begin
  if WrapOn and (TW > 0) and (TH > 0) then
  begin
    x := ((x mod TW) + TW) mod TW;
    y := ((y mod TH) + TH) mod TH;
  end;
  if (x < 0) or (y < 0) or (x >= TW) or (y >= TH) then exit;
  if (Length(StrokeMask, 0) = TW) and (Length(StrokeMask, 1) = TH) then
  begin
    if StrokeMask[x, y] then exit;
    StrokeMask[x, y] := true;
  end;
  c := BrushColorAt(x, y);
  if BrushReplaces then
  begin
    Tile[x, y] := c;
    SetIdx(x, y, BrushIndexAt(x, y));
  end
  else
  begin
    Tile[x, y] := BlendOver(c, Tile[x, y]);
    ///Смешанный цвет чаще всего уже не цвет палитры, поэтому привязка
    ///выводится из того, что получилось, а не берётся у кисти.
    SetIdx(x, y, IndexOfColor(Tile[x, y]));
  end;
  if LiveDraw then DrawTilePixel(x, y);
end;

///Клетка тайла вместе с её зеркальными парами по включённым осям.
procedure PaintPixel(x, y: integer);
begin
  PaintOne(x, y);
  if SymX then PaintOne(TW - 1 - x, y);
  if SymY then PaintOne(x, TH - 1 - y);
  if SymX and SymY then PaintOne(TW - 1 - x, TH - 1 - y);
end;

///Одна клетка карты выбранной местностью, с теми же осями симметрии.
procedure PutMapOne(x, y: integer);
begin
  if (x < 0) or (y < 0) or (x >= MapW) or (y >= MapH) then exit;
  if (Length(StrokeMask, 0) = MapW) and (Length(StrokeMask, 1) = MapH) then
  begin
    if StrokeMask[x, y] then exit;
    StrokeMask[x, y] := true;
  end;
  MapGrid[x, y] := MapBrush;
end;

procedure PutMapCell(x, y: integer);
begin
  PutMapOne(x, y);
  if SymX then PutMapOne(MapW - 1 - x, y);
  if SymY then PutMapOne(x, MapH - 1 - y);
  if SymX and SymY then PutMapOne(MapW - 1 - x, MapH - 1 - y);
end;

///Куда фигура кладёт клетку: в документ или в предпросмотр поверх холста.
///Один и тот же растеризатор служит и для показа, и для применения.
procedure EmitPixel(x, y: integer);
begin
  if ShapePreview then
  begin
    DrawPreviewPixel(x, y);
    if SymX then DrawPreviewPixel(DocW - 1 - x, y);
    if SymY then DrawPreviewPixel(x, DocH - 1 - y);
    if SymX and SymY then DrawPreviewPixel(DocW - 1 - x, DocH - 1 - y);
  end
  else if DocMode = DOC_MAP then PutMapCell(x, y)
  else PaintPixel(x, y);
end;

// ---------------------------------------------------------------------------
// Растеризация фигур
// ---------------------------------------------------------------------------

///Отрезок по Брезенхэму. Он же достраивает штрих, когда мышь двигают быстро.
procedure ShapeLine(x0, y0, x1, y1: integer);
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
    EmitPixel(x0, y0);
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

procedure ShapeRect(x0, y0, x1, y1: integer; filled: boolean);
var
  lx, ly, hx, hy: integer;
begin
  lx := Min(x0, x1);
  hx := Max(x0, x1);
  ly := Min(y0, y1);
  hy := Max(y0, y1);
  for var y := ly to hy do
    for var x := lx to hx do
      if filled or (x = lx) or (x = hx) or (y = ly) or (y = hy) then
        EmitPixel(x, y);
end;

///Эллипс, вписанный в рамку. Принадлежность считается по уравнению, а контур —
///как точки внутри, у которых хотя бы один сосед снаружи. Так растеризация
///не имеет особых случаев для вырожденных рамок.
procedure ShapeEllipse(x0, y0, x1, y1: integer; filled: boolean);
var
  cx, cy, ra, rb: real;
  lx, ly, hx, hy: integer;

  function Inside(x, y: integer): boolean;
  var
    dx, dy: real;
  begin
    dx := (x - cx) / ra;
    dy := (y - cy) / rb;
    Result := dx * dx + dy * dy <= 1.0;
  end;

begin
  lx := Min(x0, x1);
  hx := Max(x0, x1);
  ly := Min(y0, y1);
  hy := Max(y0, y1);
  cx := (lx + hx) / 2;
  cy := (ly + hy) / 2;
  ra := (hx - lx) / 2 + 0.5;
  rb := (hy - ly) / 2 + 0.5;
  for var y := ly to hy do
    for var x := lx to hx do
      if Inside(x, y) then
        if filled or (not Inside(x - 1, y)) or (not Inside(x + 1, y)) or
                     (not Inside(x, y - 1)) or (not Inside(x, y + 1)) then
          EmitPixel(x, y);
end;

///Приводит вторую точку рамки к квадрату. Нужно при удержании Shift.
procedure SquarifyTo(ax, ay: integer; var bx, by: integer);
var
  side: integer;
begin
  side := Max(Abs(bx - ax), Abs(by - ay));
  if bx >= ax then bx := ax + side else bx := ax - side;
  if by >= ay then by := ay + side else by := ay - side;
end;

// ---------------------------------------------------------------------------
// Заливка
// ---------------------------------------------------------------------------

///Заливка тайла. Обход в ширину по отдельному массиву посещённых клеток:
///изображение по ходу работы не портится служебными цветами.
procedure FloodFillTile(sx, sy: integer; newColor: Color);
var
  visited: array[,] of boolean;
  qx, qy: array of integer;
  head, tail: integer;
  target: Color;
  ni: byte;

  procedure Push(x, y: integer);
  begin
    if (x < 0) or (y < 0) or (x >= TW) or (y >= TH) then exit;
    if visited[x, y] then exit;
    if not SameColor(Tile[x, y], target) then exit;
    visited[x, y] := true;
    qx[tail] := x;
    qy[tail] := y;
    tail := tail + 1;
  end;

begin
  if (sx < 0) or (sy < 0) or (sx >= TW) or (sy >= TH) then exit;
  target := Tile[sx, sy];
  if SameColor(target, newColor) then exit;
  ///Привязка одна на всю заливку, поэтому палитра просматривается один раз,
  ///а не на каждый залитый пиксель.
  ni := IndexOfColor(newColor);
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
    SetIdx(x, y, ni);
    Push(x + 1, y);
    Push(x - 1, y);
    Push(x, y + 1);
    Push(x, y - 1);
  end;
end;

///Заливка карты выбранной местностью.
procedure FloodFillMap(sx, sy: integer; newCode: integer);
var
  visited: array[,] of boolean;
  qx, qy: array of integer;
  head, tail, target: integer;

  procedure Push(x, y: integer);
  begin
    if (x < 0) or (y < 0) or (x >= MapW) or (y >= MapH) then exit;
    if visited[x, y] then exit;
    if MapGrid[x, y] <> target then exit;
    visited[x, y] := true;
    qx[tail] := x;
    qy[tail] := y;
    tail := tail + 1;
  end;

begin
  if (sx < 0) or (sy < 0) or (sx >= MapW) or (sy >= MapH) then exit;
  target := MapGrid[sx, sy];
  if target = newCode then exit;
  SetLength(visited, MapW, MapH);
  SetLength(qx, MapW * MapH);
  SetLength(qy, MapW * MapH);
  head := 0;
  tail := 0;
  Push(sx, sy);
  while head < tail do
  begin
    var x := qx[head];
    var y := qy[head];
    head := head + 1;
    MapGrid[x, y] := newCode;
    Push(x + 1, y);
    Push(x - 1, y);
    Push(x, y + 1);
    Push(x, y - 1);
  end;
end;

// ---------------------------------------------------------------------------
// Выделение и буферы обмена
// ---------------------------------------------------------------------------

procedure CopySelectionToBuffer;
begin
  if not HasSelection then exit;
  if (SelW < 1) or (SelH < 1) then exit;
  if DocMode = DOC_MAP then
  begin
    SetLength(MapBuffer, SelW, SelH);
    for var i := 0 to SelH - 1 do
      for var j := 0 to SelW - 1 do
        MapBuffer[j, i] := MapGrid[SelX + j, SelY + i];
  end
  else
  begin
    SetLength(Buffer, SelW, SelH);
    for var i := 0 to SelH - 1 do
      for var j := 0 to SelW - 1 do
        Buffer[j, i] := Tile[SelX + j, SelY + i];
  end;
end;

///Вставляет буфер так, что выходящая за край часть отбрасывается.
procedure PasteBufferAt(px, py: integer);
var
  bw, bh: integer;
begin
  if DocMode = DOC_MAP then
  begin
    bw := Length(MapBuffer, 0);
    bh := Length(MapBuffer, 1);
    for var i := 0 to bh - 1 do
      for var j := 0 to bw - 1 do
        if (px + j >= 0) and (py + i >= 0) and (px + j < MapW) and (py + i < MapH) then
          MapGrid[px + j, py + i] := MapBuffer[j, i];
  end
  else
  begin
    bw := Length(Buffer, 0);
    bh := Length(Buffer, 1);
    for var i := 0 to bh - 1 do
      for var j := 0 to bw - 1 do
        if (px + j >= 0) and (py + i >= 0) and (px + j < TW) and (py + i < TH) then
          if Buffer[j, i].A > 0 then
          begin
            Tile[px + j, py + i] := Buffer[j, i];
            ///Буфер хранит только цвета, поэтому привязка вставленного
            ///выводится из них заново.
            SetIdx(px + j, py + i, IndexOfColor(Buffer[j, i]));
          end;
  end;
end;

function BufferFilled: boolean;
begin
  if DocMode = DOC_MAP then
    Result := (Length(MapBuffer, 0) > 0) and (Length(MapBuffer, 1) > 0)
  else
    Result := (Length(Buffer, 0) > 0) and (Length(Buffer, 1) > 0);
end;

///Отражение буфера по горизонтали. Размер не меняется, поэтому результат
///сразу возвращается в выделение, если оно того же размера.
procedure BufferFlipH;
var
  t: array[,] of Color;
  bw, bh: integer;
begin
  bw := Length(Buffer, 0);
  bh := Length(Buffer, 1);
  if (bw < 1) or (bh < 1) then exit;
  SetLength(t, bw, bh);
  for var i := 0 to bh - 1 do
    for var j := 0 to bw - 1 do
      t[j, i] := Buffer[bw - 1 - j, i];
  Buffer := t;
end;

procedure BufferFlipV;
var
  t: array[,] of Color;
  bw, bh: integer;
begin
  bw := Length(Buffer, 0);
  bh := Length(Buffer, 1);
  if (bw < 1) or (bh < 1) then exit;
  SetLength(t, bw, bh);
  for var i := 0 to bh - 1 do
    for var j := 0 to bw - 1 do
      t[j, i] := Buffer[j, bh - 1 - i];
  Buffer := t;
end;

///Поворот буфера на четверть по часовой стрелке. Стороны меняются местами,
///поэтому в выделение результат не возвращается: его вставляют вручную.
procedure BufferRotate90;
var
  t: array[,] of Color;
  bw, bh: integer;
begin
  bw := Length(Buffer, 0);
  bh := Length(Buffer, 1);
  if (bw < 1) or (bh < 1) then exit;
  SetLength(t, bh, bw);
  for var i := 0 to bh - 1 do
    for var j := 0 to bw - 1 do
      t[bh - 1 - i, j] := Buffer[j, i];
  Buffer := t;
end;

///Возвращает буфер в выделение, если размеры совпали.
procedure ApplyBufferToSelection;
begin
  if not HasSelection then exit;
  if (Length(Buffer, 0) <> SelW) or (Length(Buffer, 1) <> SelH) then exit;
  for var i := 0 to SelH - 1 do
    for var j := 0 to SelW - 1 do
    begin
      Tile[SelX + j, SelY + i] := Buffer[j, i];
      SetIdx(SelX + j, SelY + i, IndexOfColor(Buffer[j, i]));
    end;
end;

// ---------------------------------------------------------------------------
// Операции над всем тайлом
// ---------------------------------------------------------------------------

procedure ClearTile;
var
  ni: byte;
begin
  PushUndo;
  EnsureTileIdx;
  ni := IndexOfColor(ARGB(255, 255, 255, 255));
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
    begin
      Tile[j, i] := ARGB(255, 255, 255, 255);
      TileIdx[j, i] := ni;
    end;
  NeedRepaint := true;
end;

procedure RandomizeTile;
begin
  PushUndo;
  EnsureTileIdx;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
    begin
      if Random(0, 3) = 0 then
        Tile[j, i] := RGB(128 + Random(64), 128 + Random(64), 128 + Random(64))
      else
        Tile[j, i] := RGB(64 + Random(128), 64 + Random(128), 64 + Random(128));
      ///Случайный цвет палитре не принадлежит.
      TileIdx[j, i] := IDX_NONE;
    end;
  NeedRepaint := true;
end;

///Заменяет один цвет другим по всему тайлу.
function ReplaceColorEverywhere(fromC, toC: Color): integer;
var
  ni: byte;
begin
  Result := 0;
  EnsureTileIdx;
  ni := IndexOfColor(toC);
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      if SameColor(Tile[j, i], fromC) then
      begin
        Tile[j, i] := toC;
        TileIdx[j, i] := ni;
        Result := Result + 1;
      end;
end;

///Ближайшая ячейка палитры по расстоянию в RGB.
function NearestPaletteIndex(c: Color): byte;
var
  best, d, dr, dg, db: integer;
begin
  Result := IDX_NONE;
  best := -1;
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
    begin
      dr := c.R - Palette[j, i].R;
      dg := c.G - Palette[j, i].G;
      db := c.B - Palette[j, i].B;
      d := dr * dr + dg * dg + db * db;
      if (best < 0) or (d < best) then
      begin
        best := d;
        Result := PalIndexAt(j, i);
      end;
    end;
end;

///Ближайший цвет палитры по расстоянию в RGB. Прозрачность сохраняется.
function NearestPaletteColor(c: Color): Color;
var
  idx: byte;
  p: Color;
begin
  idx := NearestPaletteIndex(c);
  if idx = IDX_NONE then
    Result := c
  else
  begin
    p := PalColorOf(idx);
    Result := ARGB(c.A, p.R, p.G, p.B);
  end;
end;

///Сводит весь тайл к цветам палитры и связывает каждый пиксель с той ячейкой,
///к которой он сведён. После этого палитра управляет всем тайлом целиком.
procedure QuantizeTileToPalette;
var
  idx: byte;
  p: Color;
begin
  EnsureTileIdx;
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
      if Tile[j, i].A > 0 then
      begin
        idx := NearestPaletteIndex(Tile[j, i]);
        if idx <> IDX_NONE then
        begin
          p := PalColorOf(idx);
          Tile[j, i] := ARGB(Tile[j, i].A, p.R, p.G, p.B);
          TileIdx[j, i] := idx;
        end;
      end;
end;

procedure FlipTileH;
var
  t: array[,] of Color;
  n: array[,] of byte;
begin
  EnsureTileIdx;
  SetLength(t, TW, TH);
  SetLength(n, TW, TH);
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
    begin
      t[j, i] := Tile[TW - 1 - j, i];
      n[j, i] := TileIdx[TW - 1 - j, i];
    end;
  Tile := t;
  TileIdx := n;
end;

procedure FlipTileV;
var
  t: array[,] of Color;
  n: array[,] of byte;
begin
  EnsureTileIdx;
  SetLength(t, TW, TH);
  SetLength(n, TW, TH);
  for var i := 0 to TH - 1 do
    for var j := 0 to TW - 1 do
    begin
      t[j, i] := Tile[j, TH - 1 - i];
      n[j, i] := TileIdx[j, TH - 1 - i];
    end;
  Tile := t;
  TileIdx := n;
end;

// ---------------------------------------------------------------------------
// Рабочие цвета
// ---------------------------------------------------------------------------

///Обновляет HSV по первому цвету. У серого цвета тона нет, поэтому
///при насыщенности ноль положение ползунка тона сохраняется.
procedure SyncHSVFromColor;
var
  c: HSVColor;
begin
  c := ColorToHSV(FirstColor);
  if c.S > 0 then Hue := c.H;
  Sat := c.S;
  Val := c.V;
end;

procedure ApplyHSVToColor;
var
  c: Color;
begin
  c := HSVtoRGB(Hue, Sat, Val);
  FirstColor := ARGB(FirstColor.A, c.R, c.G, c.B);
  ///Цвет, набранный ползунками, связан с палитрой только если случайно
  ///совпал с одной из ячеек. Единственная точка, где меняется цвет полосами,
  ///поэтому привязку достаточно пересчитать здесь.
  FirstIdx := IndexOfColor(FirstColor);
end;

procedure PickColor(c: Color);
begin
  FirstColor := c;
  FirstIdx := IndexOfColor(c);
  SyncHSVFromColor;
  NeedRepaint := true;
end;

///Пипетка по тайлу. Вместе с цветом берётся и его привязка, поэтому взятый
///цвет продолжает управляться той же ячейкой палитры, даже если тот же цвет
///стоит в палитре дважды.
procedure PickColorFromTile(x, y: integer);
begin
  if (x < 0) or (y < 0) or (x >= TW) or (y >= TH) then exit;
  PickColor(Tile[x, y]);
  if (Length(TileIdx, 0) <> TW) or (Length(TileIdx, 1) <> TH) then exit;
  if TileIdx[x, y] = IDX_NONE then exit;
  if SameRGB(PalColorOf(TileIdx[x, y]), FirstColor) then FirstIdx := TileIdx[x, y];
end;

procedure SwapColors;
var
  t: Color;
  ti: byte;
begin
  t := FirstColor;
  FirstColor := SecondColor;
  SecondColor := t;
  ti := FirstIdx;
  FirstIdx := SecondIdx;
  SecondIdx := ti;
  SyncHSVFromColor;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Геометрия панели. Границы ячеек задаются одной функцией на каждую сетку,
// и отрисовка и попадание мыши считают их одинаково.
// ---------------------------------------------------------------------------

const
  OVER_W = PANEL_W - 4;
  OVER_H = OVER_Y1 - OVER_Y0;
  PAL_CELL = 16;
  PAL_X0 = PANEL_X + (PANEL_W - PAL_COLS * PAL_CELL) div 2;
  HSV_X0 = PANEL_X + (PANEL_W - BAR_SIZE) div 2;
  ///Виды местности и объектов раскладываются в две колонки.
  MSEL_COLS = 2;
  MSEL_CW = 88;
  MTERR_CH = 32;
  MOBJ_CH = 25;

function ToolCount: integer;
begin
  if DocMode = DOC_MAP then Result := T_MAP_COUNT else Result := T_TILE_COUNT;
end;

function CurTool: integer;
begin
  if DocMode = DOC_MAP then Result := ToolMap else Result := ToolTile;
end;

procedure SetTool(i: integer);
begin
  if DocMode = DOC_MAP then ToolMap := ClampI(i, 0, T_MAP_COUNT - 1)
  else ToolTile := ClampI(i, 0, T_TILE_COUNT - 1);
  NeedRepaint := true;
end;

function ToolCellX(i: integer): integer;
begin
  Result := PANEL_X + 2 + (i mod TOOL_COLS) * TOOL_CW;
end;

function ToolCellY(i: integer): integer;
begin
  Result := TOOLS_Y0 + (i div TOOL_COLS) * TOOL_CH;
end;

function ToolHit(px, py: integer; var idx: integer): boolean;
begin
  Result := false;
  for var i := 0 to ToolCount - 1 do
    if (px >= ToolCellX(i)) and (px < ToolCellX(i) + TOOL_CW - 2) and
       (py >= ToolCellY(i)) and (py < ToolCellY(i) + TOOL_CH - 2) then
    begin
      idx := i;
      Result := true;
      exit;
    end;
end;

function PalCellX(j: integer): integer;
begin
  Result := PAL_X0 + j * PAL_CELL;
end;

function PalCellY(i: integer): integer;
begin
  Result := PAL_Y0 + i * PAL_CELL;
end;

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

function MSelX(i: integer): integer;
begin
  Result := PANEL_X + 2 + (i mod MSEL_COLS) * MSEL_CW;
end;

function TerrCellY(i: integer): integer;
begin
  Result := SLOT1_Y0 + (i div MSEL_COLS) * MTERR_CH;
end;

function ObjCellY(i: integer): integer;
begin
  Result := SLOT2_Y0 + (i div MSEL_COLS) * MOBJ_CH;
end;

function TerrHit(px, py: integer; var idx: integer): boolean;
begin
  Result := false;
  for var i := 0 to MT_COUNT - 1 do
    if (px >= MSelX(i)) and (px < MSelX(i) + MSEL_CW - 2) and
       (py >= TerrCellY(i)) and (py < TerrCellY(i) + MTERR_CH - 2) then
    begin
      idx := i;
      Result := true;
      exit;
    end;
end;

function ObjHit(px, py: integer; var idx: integer): boolean;
begin
  Result := false;
  for var i := 0 to MapKindCount - 1 do
    if (px >= MSelX(i)) and (px < MSelX(i) + MSEL_CW - 2) and
       (py >= ObjCellY(i)) and (py < ObjCellY(i) + MOBJ_CH - 2) then
    begin
      idx := i;
      Result := true;
      exit;
    end;
end;

// ---------------------------------------------------------------------------
// Обзорная панель. Показывает документ целиком и рамку видимой области;
// перетаскивание в ней двигает холст, а панорама холста двигает рамку.
// ---------------------------------------------------------------------------

function OverScale: real;
begin
  Result := Min(OVER_W / Max(1, DocW), OVER_H / Max(1, DocH));
  if Result < 0.02 then Result := 0.02;
end;

function OverPixW: integer;
begin
  Result := ClampI(Round(DocW * OverScale), 1, OVER_W);
end;

function OverPixH: integer;
begin
  Result := ClampI(Round(DocH * OverScale), 1, OVER_H);
end;

function OverOriginX: integer;
begin
  Result := PANEL_X + 2 + (OVER_W - OverPixW) div 2;
end;

function OverOriginY: integer;
begin
  Result := OVER_Y0 + (OVER_H - OverPixH) div 2;
end;

///Ставит центр холста на клетку документа, отвечающую точке обзора.
procedure OverviewCenterOn(px, py: integer);
var
  docX, docY: real;
begin
  docX := (px - OverOriginX) * DocW / OverPixW;
  docY := (py - OverOriginY) * DocH / OverPixH;
  ViewX := Round(W / 2 - docX * PixelSize);
  ViewY := Round(H / 2 - docY * PixelSize);
  ClampView;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Примитивы интерфейса
// ---------------------------------------------------------------------------

///Рисует кнопку и сообщает, был ли по ней клик в этом кадре.
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
  DrawTextCentered(x + 4, y, x + bw - 4, y + bh, caption);
  if enabled and ClickPending and
     (ClickX >= x) and (ClickX < x + bw) and (ClickY >= y) and (ClickY < y + bh) then
    Result := true;
end;

procedure UILabel(x, y, bw, bh: integer; caption: string; c: Color);
begin
  GraphABC.Font.Color := c;
  DrawTextCentered(x, y, x + bw, y + bh, caption);
end;

///Рамка из четырёх полосок. Не зависит от текущего пера и стиля кисти.
procedure FrameRect(x0, y0, x1, y1: integer; c: Color);
begin
  GraphABC.Brush.Color := c;
  FillRect(x0, y0, x1, y0 + 1);
  FillRect(x0, y1 - 1, x1, y1);
  FillRect(x0, y0, x0 + 1, y1);
  FillRect(x1 - 1, y0, x1, y1);
end;

// ---------------------------------------------------------------------------
// Отрисовка холста
// ---------------------------------------------------------------------------

///Один пиксель тайла. Нужен при штрихе, чтобы не перерисовывать весь холст
///на каждое движение мыши.
procedure DrawTilePixel(x, y: integer);
var
  g: integer;
begin
  if (x < 0) or (y < 0) or (x >= TW) or (y >= TH) then exit;
  g := GridGap;
  GraphABC.Brush.Color := DisplayColor(x, y);
  FillRect(TileToScreenX(x), TileToScreenY(y),
           TileToScreenX(x + 1) - g, TileToScreenY(y + 1) - g);
end;

///Клетка предпросмотра фигуры. В документ ничего не пишется.
procedure DrawPreviewPixel(x, y: integer);
var
  g: integer;
begin
  if not InDoc(x, y) then exit;
  g := GridGap;
  GraphABC.Brush.Color := cAccent;
  FillRect(TileToScreenX(x), TileToScreenY(y),
           TileToScreenX(x + 1) - g, TileToScreenY(y + 1) - g);
end;

///Клетки тайла. При включённом бесшовном режиме рисуется не один тайл,
///а повторяющийся узор, и швы видно сразу во время работы.
procedure DrawTileCells;
var
  j0, j1, i0, i1, g, sj, si: integer;
  seamless: boolean;
  c: Color;
begin
  g := GridGap;
  j0 := ScreenToTileX(0);
  j1 := ScreenToTileX(W - 1);
  i0 := ScreenToTileY(0);
  i1 := ScreenToTileY(H - 1);

  // Повтор во весь холст на мелком масштабе стоил бы слишком много клеток,
  // поэтому за пределом бесшовный режим просто не включается.
  seamless := SeamlessOn and (TW > 0) and (TH > 0) and
              ((j1 - j0 + 1) * (i1 - i0 + 1) <= SEAMLESS_CELL_LIMIT);
  if not seamless then
  begin
    j0 := Max(0, j0);
    j1 := Min(TW - 1, j1);
    i0 := Max(0, i0);
    i1 := Min(TH - 1, i1);
  end;

  for var i := i0 to i1 do
    for var j := j0 to j1 do
    begin
      if seamless then
      begin
        sj := ((j mod TW) + TW) mod TW;
        si := ((i mod TH) + TH) mod TH;
      end
      else
      begin
        sj := j;
        si := i;
      end;
      c := DisplayColor(sj, si);
      if seamless and ((j < 0) or (j >= TW) or (i < 0) or (i >= TH)) then
        // Соседние копии слегка притушены, чтобы было видно, где сам тайл.
        c := BlendOver(ARGB(56, 0, 0, 0), c);
      if HasSelection and (j >= SelX) and (j < SelX + SelW) and
                          (i >= SelY) and (i < SelY + SelH) then
        c := BlendOver(ARGB(64, cAccent.R, cAccent.G, cAccent.B), c);
      GraphABC.Brush.Color := c;
      FillRect(TileToScreenX(j), TileToScreenY(i),
               TileToScreenX(j + 1) - g, TileToScreenY(i + 1) - g);
    end;

  if seamless then
    FrameRect(TileToScreenX(0) - 1, TileToScreenY(0) - 1,
              TileToScreenX(TW) + 1, TileToScreenY(TH) + 1, cAccent);
end;

///Клетки карты и значки объектов поверх них.
procedure DrawMapCells;
var
  j0, j1, i0, i1, g: integer;
  c: Color;
  o: MapObject;
  sx, sy: integer;
begin
  g := GridGap;
  j0 := Max(0, ScreenToTileX(0));
  j1 := Min(MapW - 1, ScreenToTileX(W - 1));
  i0 := Max(0, ScreenToTileY(0));
  i1 := Min(MapH - 1, ScreenToTileY(H - 1));

  for var i := i0 to i1 do
    for var j := j0 to j1 do
    begin
      c := MapTileColor(MapGrid[j, i]);
      if HasSelection and (j >= SelX) and (j < SelX + SelW) and
                          (i >= SelY) and (i < SelY + SelH) then
        c := BlendOver(ARGB(80, cAccent.R, cAccent.G, cAccent.B), c);
      GraphABC.Brush.Color := c;
      FillRect(TileToScreenX(j), TileToScreenY(i),
               TileToScreenX(j + 1) - g, TileToScreenY(i + 1) - g);
    end;

  // Объекты рисуются буквой в клетке. На мелком масштабе буква не влезает,
  // поэтому остаётся только заметная точка.
  for var k := 0 to Length(MapObjects) - 1 do
  begin
    o := MapObjects[k];
    if (o.X < j0) or (o.X > j1) or (o.Y < i0) or (o.Y > i1) then continue;
    sx := TileToScreenX(o.X);
    sy := TileToScreenY(o.Y);
    GraphABC.Brush.Color := ARGB(210, 250, 250, 210);
    FillRect(sx + 1, sy + 1, sx + PixelSize - 1, sy + PixelSize - 1);
    if PixelSize >= 10 then
    begin
      GraphABC.Font.Color := RGB(20, 20, 30);
      DrawTextCentered(sx, sy, sx + PixelSize, sy + PixelSize, MapObjGlyph(o.Kind));
    end;
  end;
end;

procedure DrawCanvas;
var
  g, bw, bh: integer;
begin
  GraphABC.Brush.Color := cCanvas;
  FillRect(0, 0, W, H);
  g := GridGap;

  if DocMode = DOC_MAP then DrawMapCells else DrawTileCells;

  // Предпросмотр буфера на новом месте, пока выделение перетаскивают.
  if DraggingSelection and (DocMode = DOC_TILE) then
  begin
    bw := Length(Buffer, 0);
    bh := Length(Buffer, 1);
    for var i := 0 to bh - 1 do
      for var j := 0 to bw - 1 do
        if InDoc(SelX + j, SelY + i) then
          if Buffer[j, i].A > 0 then
          begin
            GraphABC.Brush.Color := BlendOver(Buffer[j, i], CheckerColor(SelX + j, SelY + i));
            FillRect(TileToScreenX(SelX + j), TileToScreenY(SelY + i),
                     TileToScreenX(SelX + j + 1) - g, TileToScreenY(SelY + i + 1) - g);
          end;
  end;

  if HasSelection and (SelW > 0) and (SelH > 0) then
    FrameRect(TileToScreenX(SelX) - 1, TileToScreenY(SelY) - 1,
              TileToScreenX(SelX + SelW) + 1, TileToScreenY(SelY + SelH) + 1, cAccent);

  // Оси симметрии видно на холсте, иначе легко забыть, что они включены.
  if SymX then
  begin
    GraphABC.Brush.Color := ARGB(90, 220, 60, 60);
    FillRect(TileToScreenX(0) + DocW * PixelSize div 2 - 1, TileToScreenY(0),
             TileToScreenX(0) + DocW * PixelSize div 2 + 1, TileToScreenY(DocH));
  end;
  if SymY then
  begin
    GraphABC.Brush.Color := ARGB(90, 220, 60, 60);
    FillRect(TileToScreenX(0), TileToScreenY(0) + DocH * PixelSize div 2 - 1,
             TileToScreenX(DocW), TileToScreenY(0) + DocH * PixelSize div 2 + 1);
  end;
end;

// ---------------------------------------------------------------------------
// Панель. Разбита на секции: при перетаскивании ползунка перерисовываются
// только полосы цвета, а не всё окно с обзором и миниатюрами.
// ---------------------------------------------------------------------------

///Пересобирает квадрат насыщенности только при смене тона.
procedure EnsureHSVBitmap;
begin
  if CachedHue = Hue then exit;
  CachedHue := Hue;
  RenderHSVPlane(RawHSV, BAR_SIZE, Hue);
  if BmpHSV <> nil then BmpHSV.Dispose;
  BmpHSV := BytesToImage(RawHSV, BAR_SIZE);
end;

function ToolLabel(i: integer): string;
begin
  if DocMode = DOC_MAP then
    case i of
      M_PAINT: Result := 'Мест';
      M_FILL: Result := 'Зал';
      M_RECT: Result := 'Прям';
      M_OBJ: Result := 'Объ';
      else Result := 'Выд';
    end
  else
    case i of
      T_PEN: Result := 'Кар';
      T_ERASER: Result := 'Ласт';
      T_FILL: Result := 'Зал';
      T_PICK: Result := 'Пип';
      T_LINE: Result := 'Лин';
      T_RECT: Result := 'Прям';
      T_ELLIPSE: Result := 'Овал';
      T_DITHER: Result := 'Диз';
      else Result := 'Выд';
    end;
end;

procedure DrawToolsSection;
var
  x0, y0: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, 0, PANEL_X + PANEL_W, TOOLS_Y1 + 2);
  for var i := 0 to ToolCount - 1 do
  begin
    x0 := ToolCellX(i);
    y0 := ToolCellY(i);
    GraphABC.Brush.Color := cBorder;
    FillRoundRect(x0, y0, x0 + TOOL_CW - 2, y0 + TOOL_CH - 2, 5, 5);
    if CurTool = i then GraphABC.Brush.Color := cAccent
    else if (MouseX >= x0) and (MouseX < x0 + TOOL_CW - 2) and
            (MouseY >= y0) and (MouseY < y0 + TOOL_CH - 2) then GraphABC.Brush.Color := cHover
    else GraphABC.Brush.Color := cBack;
    FillRoundRect(x0 + 1, y0 + 1, x0 + TOOL_CW - 3, y0 + TOOL_CH - 3, 5, 5);
    if CurTool = i then GraphABC.Font.Color := RGB(255, 255, 255)
    else GraphABC.Font.Color := cText;
    DrawTextCentered(x0, y0, x0 + TOOL_CW - 2, y0 + TOOL_CH - 2, ToolLabel(i));
  end;
end;

procedure DrawPaletteSection;
var
  edge: Color;
  fj, fi: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, SLOT1_Y0 - 2, PANEL_X + PANEL_W, SLOT1_Y1 + 2);
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
    begin
      GraphABC.Brush.Color := BlendOver(Palette[j, i], CheckerColor(j, i));
      FillRect(PalCellX(j), PalCellY(i), PalCellX(j + 1), PalCellY(i + 1));
    end;
  ///Рамка палитры цветом выделения показывает, что индексный режим включён:
  ///в нём правка ячейки перекрашивает тайл, и об этом надо знать заранее.
  if IndexedOn and (DocMode = DOC_TILE) then edge := cAccent else edge := cBorder;
  FrameRect(PAL_X0 - 1, PAL_Y0 - 1,
            PAL_X0 + PAL_COLS * PAL_CELL + 1, PAL_Y0 + PAL_ROWS * PAL_CELL + 1, edge);
  ///Ячейка, из которой взят первый цвет, обведена: видно, какая именно
  ///ячейка поедет за кистью, когда её поправят.
  if (FirstIdx <> IDX_NONE) and (DocMode = DOC_TILE) then
  begin
    fj := FirstIdx mod PAL_COLS;
    fi := FirstIdx div PAL_COLS;
    FrameRect(PalCellX(fj), PalCellY(fi), PalCellX(fj + 1), PalCellY(fi + 1), cAccent);
  end;
end;

procedure DrawColorSection;
var
  mx, my: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, SLOT2_Y0 - 2, PANEL_X + PANEL_W, ALPHA_Y1 + 2);
  EnsureHSVBitmap;
  if BmpHSV <> nil then GraphBufferGraphics.DrawImage(BmpHSV, HSV_X0, HSV_Y0);
  if BmpHue <> nil then GraphBufferGraphics.DrawImage(BmpHue, HSV_X0, HUE_Y0);
  if BmpAlpha <> nil then GraphBufferGraphics.DrawImage(BmpAlpha, HSV_X0, ALPHA_Y0);

  mx := HSV_X0 + PercentToAxis(Sat, BAR_SIZE);
  my := HSV_Y0 + ValueToAxis(Val, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 0, 0, 0);
  FillRect(mx - 1, HSV_Y0, mx + 1, HSV_Y1);
  FillRect(HSV_X0, my - 1, HSV_X0 + BAR_SIZE, my + 1);

  mx := HSV_X0 + HueToAxis(Hue, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 0, 0, 0);
  FillRect(mx - 1, HUE_Y0, mx + 1, HUE_Y1);

  mx := HSV_X0 + ByteToAxis(FirstColor.A, BAR_SIZE);
  GraphABC.Brush.Color := ARGB(200, 220, 40, 40);
  FillRect(mx - 1, ALPHA_Y0, mx + 1, ALPHA_Y1);

  // Оба рабочих цвета: первым рисуют, второй участвует в дизеринге и обмене.
  GraphABC.Brush.Color := BlendOver(FirstColor, cCheckA);
  FillRect(PANEL_X + 4, SLOT2_Y0, PANEL_X + 34, SLOT2_Y0 + 30);
  FrameRect(PANEL_X + 4, SLOT2_Y0, PANEL_X + 34, SLOT2_Y0 + 30, cBorder);
  GraphABC.Brush.Color := BlendOver(SecondColor, cCheckA);
  FillRect(PANEL_X + 4, SLOT2_Y0 + 34, PANEL_X + 34, SLOT2_Y0 + 64);
  FrameRect(PANEL_X + 4, SLOT2_Y0 + 34, PANEL_X + 34, SLOT2_Y0 + 64, cBorder);
end;

procedure DrawTerrainSection;
var
  x0, y0: integer;
  lbl: string;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, SLOT1_Y0 - 2, PANEL_X + PANEL_W, SLOT1_Y1 + 2);
  for var i := 0 to MT_COUNT - 1 do
  begin
    x0 := MSelX(i);
    y0 := TerrCellY(i);
    GraphABC.Brush.Color := MapTileColor(i);
    FillRect(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2);
    if MapBrush = i then FrameRect(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2, cAccent)
    else FrameRect(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2, cBorder);
    // Подпись светлая на тёмной местности и наоборот.
    if (i = MT_WALL) or (i = MT_TREE) or (i = MT_WATER) or (i = MT_DEADWATER) then
      GraphABC.Font.Color := RGB(245, 245, 245)
    else
      GraphABC.Font.Color := RGB(20, 20, 24);
    lbl := '';
    lbl := lbl + MapTileChar(i) + ' ' + MapTileName(i);
    DrawTextCentered(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2, lbl);
  end;
end;

procedure DrawObjectKindSection;
var
  x0, y0: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, SLOT2_Y0 - 2, PANEL_X + PANEL_W, ALPHA_Y1 + 2);
  for var i := 0 to MapKindCount - 1 do
  begin
    x0 := MSelX(i);
    y0 := ObjCellY(i);
    GraphABC.Brush.Color := cBorder;
    FillRoundRect(x0, y0, x0 + MSEL_CW - 2, y0 + MOBJ_CH - 2, 4, 4);
    if MapObjKind = i then GraphABC.Brush.Color := cAccent else GraphABC.Brush.Color := cBack;
    FillRoundRect(x0 + 1, y0 + 1, x0 + MSEL_CW - 3, y0 + MOBJ_CH - 3, 4, 4);
    if MapObjKind = i then GraphABC.Font.Color := RGB(255, 255, 255)
    else GraphABC.Font.Color := cText;
    DrawTextCentered(x0, y0, x0 + MSEL_CW - 2, y0 + MOBJ_CH - 2,
                     MapObjGlyph(MapKindName(i)) + ' ' + MapKindName(i));
  end;
end;

///Перерисовывает картинку обзора. Строится в буфер байтов и выводится одним
///блитом: поклеточная отрисовка обошлась бы в тысячи вызовов на кадр.
procedure BuildOverview;
var
  pw, ph, i, sx, sy: integer;
  c: Color;
begin
  pw := OverPixW;
  ph := OverPixH;
  if (pw < 1) or (ph < 1) or (DocW < 1) or (DocH < 1) then exit;
  if (not OverDirty) and (BmpOver <> nil) and
     (OverCacheW = DocW) and (OverCacheH = DocH) and (OverCacheMode = DocMode) then exit;
  OverDirty := false;
  OverCacheW := DocW;
  OverCacheH := DocH;
  OverCacheMode := DocMode;
  SetLength(RawOver, pw * ph * 4);
  for var y := 0 to ph - 1 do
    for var x := 0 to pw - 1 do
    begin
      sx := ClampI(x * DocW div pw, 0, DocW - 1);
      sy := ClampI(y * DocH div ph, 0, DocH - 1);
      if DocMode = DOC_MAP then c := MapTileColor(MapGrid[sx, sy])
      else c := BlendOver(Tile[sx, sy], CheckerColor(sx, sy));
      i := (y * pw + x) * 4;
      RawOver[i + 0] := c.B;
      RawOver[i + 1] := c.G;
      RawOver[i + 2] := c.R;
      RawOver[i + 3] := 255;
    end;
  if BmpOver <> nil then BmpOver.Dispose;
  BmpOver := BytesToImage(RawOver, pw);
end;

procedure DrawOverviewSection;
var
  ox, oy, pw, ph: integer;
  vx0, vy0, vx1, vy1: integer;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, OVER_Y0 - 2, PANEL_X + PANEL_W, OVER_Y1 + 2);
  BuildOverview;
  ox := OverOriginX;
  oy := OverOriginY;
  pw := OverPixW;
  ph := OverPixH;
  if BmpOver <> nil then GraphBufferGraphics.DrawImage(BmpOver, ox, oy);
  FrameRect(ox - 1, oy - 1, ox + pw + 1, oy + ph + 1, cBorder);

  // Рамка видимой области. Она же показывает, что панорама холста и обзор
  // всегда смотрят на одно и то же место документа.
  if (DocW > 0) and (DocH > 0) and (PixelSize > 0) then
  begin
    vx0 := ox + Round((-ViewX / PixelSize) * pw / DocW);
    vy0 := oy + Round((-ViewY / PixelSize) * ph / DocH);
    vx1 := ox + Round(((-ViewX + W) / PixelSize) * pw / DocW);
    vy1 := oy + Round(((-ViewY + H) / PixelSize) * ph / DocH);
    vx0 := ClampI(vx0, ox, ox + pw);
    vy0 := ClampI(vy0, oy, oy + ph);
    vx1 := ClampI(vx1, ox, ox + pw);
    vy1 := ClampI(vy1, oy, oy + ph);
    if (vx1 > vx0) and (vy1 > vy0) then
      FrameRect(vx0, vy0, vx1, vy1, RGB(250, 240, 80));
  end;
end;

///Рисует сетку цветов, вписанную в квадрат. Перебор идёт по пикселям
///приёмника, поэтому стоимость не зависит от размера исходника.
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

procedure DrawInfoSection;
var
  tx, ty: integer;
  s: string;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, INFO_Y0 - 2, PANEL_X + PANEL_W, INFO_Y1 + 2);

  if DocMode = DOC_TILE then
  begin
    GraphABC.Brush.Color := cFace;
    FillRect(PANEL_X + 2, INFO_Y0, PANEL_X + 42, INFO_Y0 + 40);
    DrawGridThumb(Buffer, PANEL_X + 2, INFO_Y0, 40);
    FrameRect(PANEL_X + 2, INFO_Y0, PANEL_X + 42, INFO_Y0 + 40, cBorder);
  end;

  tx := ScreenToTileX(MouseX);
  ty := ScreenToTileY(MouseY);
  if (MouseX < W) and InDoc(tx, ty) then
  begin
    s := IntToStr(tx) + ' : ' + IntToStr(ty);
    if DocMode = DOC_MAP then s := s + '  ' + MapTileName(MapGrid[tx, ty]);
  end
  else
    s := '—';
  s := s + NewLine + IntToStr(DocW) + 'x' + IntToStr(DocH) + '  ' + IntToStr(PixelSize) + 'x';
  if DocMode = DOC_MAP then
    s := s + '  объектов ' + IntToStr(Length(MapObjects));
  UILabel(PANEL_X + 46, INFO_Y0, PANEL_W - 48, INFO_Y1 - INFO_Y0, s, cText);
end;

procedure DrawMenuButtonSection;
var
  caption: string;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, MENU_Y0 - 2, PANEL_X + PANEL_W, H);
  if DocMode = DOC_MAP then caption := 'Меню — карта' else caption := 'Меню — тайл';
  if IsModified then caption := caption + ' *';
  UIButton(PANEL_X + 4, MENU_Y0, PANEL_W - 8, MENU_Y1 - MENU_Y0, caption, true);
end;

procedure DrawPanel;
begin
  GraphABC.Brush.Color := cPanel;
  FillRect(PANEL_X, 0, PANEL_X + PANEL_W, H);
  DrawToolsSection;
  if DocMode = DOC_MAP then
  begin
    DrawTerrainSection;
    DrawObjectKindSection;
  end
  else
  begin
    DrawPaletteSection;
    DrawColorSection;
  end;
  DrawOverviewSection;
  DrawInfoSection;
  DrawMenuButtonSection;
end;

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

///Перерисовка одного холста. Нужна при протяжке: панель при этом не меняется,
///а её обзор и миниатюры — самая дорогая часть кадра.
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

///Перерисовка холста вместе с обзором: рамка видимой области должна
///следовать за панорамой без задержки.
procedure RepaintCanvasAndOverview;
begin
  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    DrawCanvas;
    DrawOverviewSection;
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  Redraw;
end;

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

///Вопрос с двумя ответами. Escape равнозначен отказу.
function AskYesNo(title, text, yes, no_: string): boolean;
var
  done, res: boolean;
begin
  res := false;
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

    if UIButton(WIN_W div 2 - 180, H div 2 + 30, 170, 34, yes, true) then
    begin
      res := true;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, H div 2 + 30, 170, 34, no_, true) then
    begin
      res := false;
      done := true;
    end;

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then
    begin
      res := false;
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
  SX1 = WIN_W - 70;
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
    FillRect(70, 296, WIN_W - 68, 366);
    // Шахматка кладётся блоками по 8 пикселей, а не по одному:
    // экран перерисовывается на каждом кадре, пока тащат ползунок.
    for var bx := 0 to (WIN_W - 140) div 8 do
      for var by := 0 to 8 do
        if (bx + by) mod 2 = 0 then
        begin
          GraphABC.Brush.Color := cCheckA;
          FillRect(71 + bx * 8, 297 + by * 8,
                   Min(WIN_W - 69, 79 + bx * 8), Min(365, 305 + by * 8));
        end;
    GraphABC.Brush.Color := ARGB(ch[0], ch[1], ch[2], ch[3]);
    FillRect(71, 297, WIN_W - 69, 365);

    if UIButton(WIN_W div 2 - 180, H - 80, 170, 38, 'Применить', true) then
    begin
      FirstColor := ARGB(ch[0], ch[1], ch[2], ch[3]);
      SyncHSVFromColor;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, H - 80, 170, 38, 'Отмена', true) then done := true;

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
// Ввод строки. Символы собираются в OnKeyPress, управляющие клавиши приходят
// обычным путём через TakeKey.
// ---------------------------------------------------------------------------

function AskText(title, prompt, initial: string; var value: string): boolean;
var
  done, ok: boolean;
  fx, fy, fw, tick: integer;
  shown: string;
begin
  TextInputBuf := initial;
  TextInputActive := true;
  done := false;
  ok := false;
  tick := 0;
  ClickPending := false;
  KeyPending := false;
  fx := 60;
  fy := H div 2 - 20;
  fw := WIN_W - 120;

  while not done do
  begin
    tick := tick + 1;
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, fy - 110, WIN_W, fy - 74, title);
    UILabel(0, fy - 70, WIN_W, 24, prompt, cTextDim);

    GraphABC.Brush.Color := cBorder;
    FillRoundRect(fx - 2, fy - 2, fx + fw + 2, fy + 36, 5, 5);
    GraphABC.Brush.Color := RGB(252, 252, 253);
    FillRoundRect(fx, fy, fx + fw, fy + 34, 5, 5);
    // Строка обрезается слева: при наборе интересен конец, а не начало.
    shown := FitText(TextInputBuf, fw - 22);
    if (tick div 60) mod 2 = 0 then shown := shown + '|';
    GraphABC.Font.Color := cText;
    TextOut(fx + 8, fy + 8, shown);

    if UIButton(WIN_W div 2 - 180, fy + 60, 170, 36, 'Готово', true) then
    begin
      ok := true;
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, fy + 60, 170, 36, 'Отмена', true) then done := true;

    Redraw;
    ClickPending := false;

    var key := TakeKey;
    if key = KEY_ENTER then
    begin
      ok := true;
      done := true;
    end
    else if key = KEY_ESC then
      done := true
    else if key = KEY_BACK then
      if Length(TextInputBuf) > 0 then
        TextInputBuf := TextInputBuf.Substring(0, Length(TextInputBuf) - 1);

    Idle;
  end;

  TextInputActive := false;
  WaitMouseRelease;
  NeedRepaint := true;
  if ok then value := TextInputBuf;
  Result := ok;
end;

// ---------------------------------------------------------------------------
// Операции над цветом всего тайла
// ---------------------------------------------------------------------------

procedure ShowColorOpsScreen;
var
  done: boolean;
  y, bx, bw, bh, stp, n: integer;
  caption: string;
begin
  done := false;
  ClickPending := false;
  KeyPending := false;
  bw := 420;
  bh := 30;
  stp := 34;
  bx := (WIN_W - bw) div 2;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 24, WIN_W, 60, 'Операции с цветом');
    UILabel(0, 62, WIN_W, 22,
            'Первый и второй цвет берутся из панели справа', cTextDim);

    // Образцы обоих рабочих цветов, чтобы не гадать, что с чем меняется.
    GraphABC.Brush.Color := BlendOver(FirstColor, cCheckA);
    FillRect(bx, 92, bx + 60, 122);
    FrameRect(bx, 92, bx + 60, 122, cBorder);
    UILabel(bx + 66, 92, 120, 30, 'первый', cText);
    GraphABC.Brush.Color := BlendOver(SecondColor, cCheckA);
    FillRect(bx + 200, 92, bx + 260, 122);
    FrameRect(bx + 200, 92, bx + 260, 122, cBorder);
    UILabel(bx + 266, 92, 120, 30, 'второй', cText);

    y := 140;
    if UIButton(bx, y, bw, bh, 'Заменить второй цвет первым по всему тайлу', true) then
    begin
      PushUndo;
      n := ReplaceColorEverywhere(SecondColor, FirstColor);
      NeedRepaint := true;
      done := true;
      ShowMessage('Замена цвета', 'Заменено пикселей: ' + IntToStr(n) + '.');
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Свести тайл к цветам палитры', true) then
    begin
      PushUndo;
      QuantizeTileToPalette;
      NeedRepaint := true;
      done := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Отразить тайл по горизонтали', true) then
    begin
      PushUndo;
      FlipTileH;
      NeedRepaint := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Отразить тайл по вертикали', true) then
    begin
      PushUndo;
      FlipTileV;
      NeedRepaint := true;
    end;

    ///Отступ такой, чтобы подпись раздела не наезжала на кнопку над ней:
    ///кнопка кончается через bh, подпись занимает ещё двадцать точек.
    y := y + stp + 20;
    UILabel(0, y - 22, WIN_W, 20, 'Индексная палитра', cTextDim);
    if IndexedOn then caption := 'Индексный режим: включён'
    else caption := 'Индексный режим: выключен';
    if UIButton(bx, y, bw, bh, caption, true) then
    begin
      IndexedOn := not IndexedOn;
      if IndexedOn then RebindTileToPalette;
      NeedRepaint := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Привязать тайл к палитре', true) then
    begin
      n := RebindTileToPalette;
      NeedRepaint := true;
      done := true;
      ShowMessage('Привязка к палитре',
                  'Связано пикселей: ' + IntToStr(n) + ' из ' +
                  IntToStr(TW * TH) + '. Остальные не совпали ни с одной ' +
                  'ячейкой палитры и правкой ячеек не изменятся. Свести тайл ' +
                  'к цветам палитры можно кнопкой выше — тогда свяжутся все.');
    end;

    y := y + stp + 20;
    UILabel(0, y - 22, WIN_W, 20, 'Буфер обмена', cTextDim);
    if UIButton(bx, y, bw, bh, 'Отразить буфер по горизонтали', BufferFilled) then
    begin
      BufferFlipH;
      ApplyBufferToSelection;
      NeedRepaint := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Отразить буфер по вертикали', BufferFilled) then
    begin
      BufferFlipV;
      ApplyBufferToSelection;
      NeedRepaint := true;
    end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Повернуть буфер на четверть оборота', BufferFilled) then
    begin
      BufferRotate90;
      NeedRepaint := true;
    end;

    if UIButton(WIN_W div 2 - 85, H - 56, 170, 36, 'Закрыть', true) then done := true;

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then done := true;
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
  if remember then RememberFile(path, false);
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
    TileModified := false;
    RememberFile(path, false);
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

// ---------------------------------------------------------------------------
// Карты локаций
// ---------------------------------------------------------------------------

const
  MAP_FILTER = 'Карты локаций (*.map)|*.map|Все файлы (*.*)|*.*';

///Папка карт для диалогов: сначала настройка, потом папка последней карты.
function MapsDir: string;
begin
  Result := ResolvePath(CfgMapsFolder);
  if (Result <> '') and System.IO.Directory.Exists(Result) then exit;
  Result := '';
  if CfgLastMapFile <> '' then
    try
      Result := System.IO.Path.GetDirectoryName(ResolvePath(CfgLastMapFile));
    except
      Result := '';
    end;
end;

function AskOpenMap(title: string): string;
begin
  Result := '';
  try
    var dlg := System.Windows.Forms.OpenFileDialog.Create;
    dlg.Title := title;
    dlg.Filter := MAP_FILTER;
    var dir := MapsDir;
    if (dir <> '') and System.IO.Directory.Exists(dir) then dlg.InitialDirectory := dir;
    if dlg.ShowDialog = System.Windows.Forms.DialogResult.OK then Result := dlg.FileName;
  except
    Result := '';
  end;
  WaitMouseRelease;
  NeedRepaint := true;
end;

function AskSaveMap(title, suggested: string): string;
begin
  Result := '';
  try
    var dlg := System.Windows.Forms.SaveFileDialog.Create;
    dlg.Title := title;
    dlg.Filter := MAP_FILTER;
    dlg.DefaultExt := 'map';
    dlg.AddExtension := true;
    if suggested <> '' then dlg.FileName := suggested;
    var dir := MapsDir;
    if (dir <> '') and System.IO.Directory.Exists(dir) then dlg.InitialDirectory := dir;
    if dlg.ShowDialog = System.Windows.Forms.DialogResult.OK then Result := dlg.FileName;
  except
    Result := '';
  end;
  WaitMouseRelease;
  NeedRepaint := true;
end;

///Переключение режима. Документы живут параллельно, поэтому переход
///туда и обратно ничего не теряет.
procedure SwitchMode(mode: integer);
begin
  if DocMode = mode then exit;
  DocMode := mode;
  ResetSelection;
  FitScale;
  OverDirty := true;
  NeedRepaint := true;
end;

procedure NewMapOfSize(w, h: integer);
begin
  MapNew(w, h);
  CurrentMapFile := '';
  MapModified := false;
  DocMode := DOC_MAP;
  ResetSelection;
  ClearMapHistory;
  OverDirty := true;
  FitScale;
  NeedRepaint := true;
end;

function LoadMapFrom(path: string; quiet, remember: boolean): boolean;
begin
  Result := false;
  if not FileThere(path) then
  begin
    if not quiet then ShowMessage('Файл не найден', path);
    exit;
  end;
  if not MapLoad(path) then
  begin
    if not quiet then ShowMessage('Карта не прочитана', MapError);
    exit;
  end;
  CurrentMapFile := path;
  MapModified := false;
  DocMode := DOC_MAP;
  ResetSelection;
  ClearMapHistory;
  OverDirty := true;
  FitScale;
  NeedRepaint := true;
  if remember then RememberFile(path, true);
  if (MapWarning <> '') and (not quiet) then
    ShowMessage('Карта открыта с замечанием', MapWarning);
  Result := true;
end;

function SaveMapTo(path: string): boolean;
begin
  Result := false;
  if path = '' then exit;
  if not MapSave(path) then
  begin
    ShowMessage('Карта не сохранена',
                'Не удалось записать файл ' + path + '. Проверьте права на запись.');
    exit;
  end;
  CurrentMapFile := path;
  MapModified := false;
  RememberFile(path, true);
  Result := true;
end;

///Правка строки объекта. Редактор не разбирает содержимое сундуков и условия
///перехода, поэтому даёт править хвост строки как есть — так ничего
///не теряется и доступны все возможности формата.
procedure EditObject(idx: integer);
var
  rest: string;
begin
  if (idx < 0) or (idx >= Length(MapObjects)) then exit;
  rest := MapObjects[idx].Rest;
  if AskText('Объект ' + MapObjects[idx].Kind,
             'Строка целиком: ' + MapObjectLine(MapObjects[idx]),
             rest, rest) then
  begin
    PushUndo;
    MapObjects[idx].Rest := rest.Trim;
    NeedRepaint := true;
  end;
end;

///Разбивает текст на строки по переводам строки.
function SplitLines(s: string): array of string;
var
  cur: string;
  c: char;
begin
  SetLength(Result, 0);
  cur := '';
  for var i := 1 to Length(s) do
  begin
    c := s[i];
    if (c = Chr(10)) or (c = Chr(13)) then
    begin
      if cur <> '' then
      begin
        SetLength(Result, Length(Result) + 1);
        Result[Length(Result) - 1] := cur;
        cur := '';
      end;
    end
    else
      cur := cur + c;
  end;
  if cur <> '' then
  begin
    SetLength(Result, Length(Result) + 1);
    Result[Length(Result) - 1] := cur;
  end;
end;

procedure ShowMapReport;
var
  report: string;
  p: array of string;

  procedure Add(s: string);
  begin
    SetLength(p, Length(p) + 1);
    p[Length(p) - 1] := s;
  end;

begin
  report := MapValidate;
  SetLength(p, 0);
  Add('Локация: ' + MapName);
  Add('Размер: ' + IntToStr(MapW) + ' на ' + IntToStr(MapH) +
      ', объектов: ' + IntToStr(Length(MapObjects)));
  Add('');
  if report = '' then
    Add('Замечаний нет: все объекты стоят на проходимых клетках, ' +
        'проходимая часть локации связна.')
  else
  begin
    Add('Замечания:');
    Add('');
    var lines := SplitLines(report);
    for var i := 0 to Length(lines) - 1 do
      Add(lines[i]);
  end;
  ShowTextScreen('Проверка локации', p);
end;

// ---------------------------------------------------------------------------
// Свойства карты
// ---------------------------------------------------------------------------

procedure ShowMapPropertiesScreen;
var
  done: boolean;
  y, bx, nw, nh: integer;
  nm: string;
begin
  done := false;
  ClickPending := false;
  KeyPending := false;
  bx := 60;
  nw := MapW;
  nh := MapH;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 24, WIN_W, 60, 'Свойства локации');
    UILabel(0, 62, WIN_W, 22,
            'Название и размер попадут в файл карты как есть', cTextDim);

    y := 110;
    UILabelLeft(bx, y, 30, 'Название', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(bx + 160, y, WIN_W - 220, y + 30, 4, 4);
    UILabelLeft(bx + 168, y, 30, FitText(MapName, WIN_W - 400), cText);
    if UIButton(WIN_W - 210, y, 150, 30, 'Изменить', true) then
    begin
      nm := MapName;
      if AskText('Название локации',
                 'Строка name в файле карты', nm, nm) then
      begin
        PushUndo;
        MapName := nm.Trim;
      end;
    end;

    y := y + 56;
    UILabelLeft(bx, y, 30, 'Ширина', cText);
    if UIButton(bx + 160, y, 44, 30, '−', nw > MAP_MIN_SIDE) then nw := nw - 1;
    UILabel(bx + 208, y, 70, 30, IntToStr(nw), cText);
    if UIButton(bx + 282, y, 44, 30, '+', nw < MAP_MAX_SIDE) then nw := nw + 1;
    if UIButton(bx + 334, y, 60, 30, '−8', nw > MAP_MIN_SIDE + 7) then nw := nw - 8;
    if UIButton(bx + 400, y, 60, 30, '+8', nw < MAP_MAX_SIDE - 7) then nw := nw + 8;

    y := y + 42;
    UILabelLeft(bx, y, 30, 'Высота', cText);
    if UIButton(bx + 160, y, 44, 30, '−', nh > MAP_MIN_SIDE) then nh := nh - 1;
    UILabel(bx + 208, y, 70, 30, IntToStr(nh), cText);
    if UIButton(bx + 282, y, 44, 30, '+', nh < MAP_MAX_SIDE) then nh := nh + 1;
    if UIButton(bx + 334, y, 60, 30, '−8', nh > MAP_MIN_SIDE + 7) then nh := nh - 8;
    if UIButton(bx + 400, y, 60, 30, '+8', nh < MAP_MAX_SIDE - 7) then nh := nh + 8;

    nw := ClampI(nw, MAP_MIN_SIDE, MAP_MAX_SIDE);
    nh := ClampI(nh, MAP_MIN_SIDE, MAP_MAX_SIDE);

    y := y + 50;
    if (nw <> MapW) or (nh <> MapH) then
    begin
      UILabel(0, y, WIN_W, 24,
              'Обрезка уберёт объекты, оказавшиеся за новой границей', cWarn);
      if UIButton(WIN_W div 2 - 110, y + 28, 220, 34,
                  'Применить размер ' + IntToStr(nw) + 'x' + IntToStr(nh), true) then
      begin
        PushUndo;
        MapResize(nw, nh);
        ResetSelection;
        FitScale;
        NeedRepaint := true;
      end;
    end;

    if UIButton(WIN_W div 2 - 240, H - 60, 220, 36, 'Проверить локацию', true) then
      ShowMapReport;
    if UIButton(WIN_W div 2 + 20, H - 60, 220, 36, 'Закрыть', true) then done := true;

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then done := true;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Команды
// ---------------------------------------------------------------------------

function CommandSaveAs: boolean;
var
  path, suggest: string;
begin
  if DocMode = DOC_MAP then
  begin
    suggest := FileNameOnly(CurrentMapFile);
    if suggest = '' then suggest := 'location.map';
    path := AskSaveMap('Сохранить карту', suggest);
    if path = '' then Result := false else Result := SaveMapTo(path);
  end
  else
  begin
    suggest := FileNameOnly(CurrentFile);
    if suggest = '' then suggest := 'tile.png';
    path := AskSaveFile('Сохранить тайл', suggest);
    if path = '' then Result := false else Result := SaveTileTo(path);
  end;
end;

function CommandSave: boolean;
begin
  if DocMode = DOC_MAP then
  begin
    if CurrentMapFile = '' then Result := CommandSaveAs
    else Result := SaveMapTo(CurrentMapFile);
  end
  else
  begin
    if CurrentFile = '' then Result := CommandSaveAs
    else Result := SaveTileTo(CurrentFile);
  end;
end;

///Спрашивает про несохранённые изменения. false — операцию нужно отменить.
function ConfirmDiscard(what: string): boolean;
var
  answer: integer;
begin
  Result := true;
  if not IsModified then exit;
  answer := Ask3('Несохранённые изменения',
                 'В документе есть изменения, которых нет в файле. ' + what,
                 'Сохранить', 'Не сохранять', 'Отмена');
  if answer = 1 then Result := CommandSave
  else if answer = 0 then Result := true
  else Result := false;
end;

procedure CommandNew;
var
  nw, nh: integer;
begin
  if DocMode = DOC_MAP then
  begin
    if not ConfirmDiscard('Создать новую карту?') then exit;
    nw := CfgDefaultMapWidth;
    nh := CfgDefaultMapHeight;
    if AskNewTileSize(nw, nh) then NewMapOfSize(nw, nh);
  end
  else
  begin
    if not ConfirmDiscard('Создать новый тайл?') then exit;
    nw := CfgDefaultWidth;
    nh := CfgDefaultHeight;
    if AskNewTileSize(nw, nh) then NewTileOfSize(nw, nh);
  end;
end;

procedure CommandOpen;
var
  path: string;
begin
  if DocMode = DOC_MAP then
  begin
    if not ConfirmDiscard('Открыть другую карту?') then exit;
    path := AskOpenMap('Открыть карту локации');
    if path <> '' then LoadMapFrom(path, false, true);
  end
  else
  begin
    if not ConfirmDiscard('Открыть другой файл?') then exit;
    path := AskOpenFile('Открыть тайл');
    if path <> '' then LoadTileFrom(path, false, true);
  end;
end;

procedure CommandOpenLast;
var
  path: string;
begin
  if DocMode = DOC_MAP then path := ResolvePath(CfgLastMapFile)
  else path := ResolvePath(CfgLastFile);
  if not FileThere(path) then
  begin
    ShowMessage('Файл недоступен', 'Последний файл не найден: ' + path);
    exit;
  end;
  if not ConfirmDiscard('Открыть последний файл?') then exit;
  if DocMode = DOC_MAP then LoadMapFrom(path, false, true)
  else LoadTileFrom(path, false, true);
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
  // Второй документ мог остаться несохранённым — спрашиваем и о нём.
  if DocMode = DOC_MAP then
  begin
    if TileModified then
    begin
      DocMode := DOC_TILE;
      if not ConfirmDiscard('Выйти из программы?') then
      begin
        DocMode := DOC_MAP;
        exit;
      end;
    end;
  end
  else
    if MapModified then
    begin
      DocMode := DOC_MAP;
      if not ConfirmDiscard('Выйти из программы?') then
      begin
        DocMode := DOC_TILE;
        exit;
      end;
    end;
  QuitApp;
end;

// ---------------------------------------------------------------------------
// Справка, настройки, главное меню
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Набор тайлов Andors-Love.
//
// Игра ищет картинки по имени файла: data/tiles/wall.png и так далее, по файлу
// на слот. Имена и их состав повторяют src/gfx/tiles.cpp игры — это такой же
// контракт между двумя проектами, как коды клеток в TJTMMap, и разъезжаться
// им нельзя: слот, названный иначе, игра просто не найдёт.
//
// Ненарисованный слот — не дырка: игра рисует его прежним способом, заливкой
// и знаком шрифта. Поэтому набор можно делать по одному тайлу и в любом
// порядке, а пустые слоты в этом экране — нормальное состояние, не ошибка.
// ---------------------------------------------------------------------------

const
  TSET_COUNT = 17;
  TSET_COLS = 6;
  TSET_CELL_W = 112;
  TSET_CELL_H = 104;
  TSET_THUMB = 56;
  TSET_X0 = (WIN_W - TSET_COLS * TSET_CELL_W) div 2;
  TSET_Y0 = 96;

var
  ///Тайлы набора и их миниатюры. Кэш живёт, пока открыт экран: перечитывать
  ///семнадцать файлов на каждый кадр отрисовки незачем.
  TsetGrid: array of array[,] of Color;
  TsetBmp: array of Bitmap;

///Имя файла слота без расширения. Порядок задан игрой.
function TsetFile(i: integer): string;
begin
  case i of
    0: Result := 'floor';
    1: Result := 'wall';
    2: Result := 'water';
    3: Result := 'tree';
    4: Result := 'grass';
    5: Result := 'road';
    6: Result := 'deadwater';
    7: Result := 'player';
    8: Result := 'npc';
    9: Result := 'mob';
    10: Result := 'exit';
    11: Result := 'sign';
    12: Result := 'item';
    13: Result := 'bed';
    14: Result := 'chest';
    15: Result := 'portal';
    16: Result := 'note';
    else Result := '';
  end;
end;

///Название слота по-русски. Взято из тех же мест игры, что и имена файлов.
function TsetName(i: integer): string;
begin
  case i of
    0: Result := 'пол';
    1: Result := 'стена';
    2: Result := 'вода';
    3: Result := 'дерево';
    4: Result := 'трава';
    5: Result := 'дорога';
    6: Result := 'стоячая вода';
    7: Result := 'герой';
    8: Result := 'житель';
    9: Result := 'враг';
    10: Result := 'переход';
    11: Result := 'табличка';
    12: Result := 'предмет';
    13: Result := 'лежанка';
    14: Result := 'сундук';
    15: Result := 'портал';
    16: Result := 'записка';
    else Result := '';
  end;
end;

///Местность рисуется непрозрачной, объекты — с прозрачным фоном.
///Разделение показывается на экране, чтобы не гадать, где нужен фон.
function TsetIsTerrain(i: integer): boolean;
begin
  Result := i <= 6;
end;

///Папка набора. Пустая строка означает, что папка не задана или не найдена.
function TilesDir: string;
begin
  Result := ResolvePath(CfgTilesFolder);
  if (Result <> '') and System.IO.Directory.Exists(Result) then exit;
  Result := '';
end;

function TsetPath(i: integer): string;
var
  dir: string;
begin
  Result := '';
  if (i < 0) or (i >= TSET_COUNT) then exit;
  dir := TilesDir;
  if dir = '' then exit;
  try
    Result := System.IO.Path.Combine(dir, TsetFile(i) + '.png');
  except
    Result := '';
  end;
end;

///Собирает миниатюру сетки цветов. Прозрачные пиксели кладутся на шахматку:
///иначе тайлы объектов, у которых фон прозрачный, выглядели бы пустыми.
function MakeThumb(g: array[,] of Color; size: integer): Bitmap;
var
  raw: array of byte;
  gw, gh, sx, sy, k: integer;
  c: Color;
begin
  Result := nil;
  if g = nil then exit;
  gw := Length(g, 0);
  gh := Length(g, 1);
  if (gw < 1) or (gh < 1) or (size < 1) then exit;
  SetLength(raw, size * size * 4);
  for var py := 0 to size - 1 do
    for var px := 0 to size - 1 do
    begin
      sx := ClampI(px * gw div size, 0, gw - 1);
      sy := ClampI(py * gh div size, 0, gh - 1);
      c := BlendOver(g[sx, sy], CheckerColor(px, py));
      k := (py * size + px) * 4;
      raw[k + 0] := c.B;
      raw[k + 1] := c.G;
      raw[k + 2] := c.R;
      raw[k + 3] := 255;
    end;
  Result := BytesToImage(raw, size);
end;

///Перечитывает один слот с диска. Отсутствующий файл — не ошибка:
///слот просто остаётся пустым.
procedure TsetLoadSlot(i: integer);
var
  path: string;
  g: array[,] of Color;
begin
  if (i < 0) or (i >= TSET_COUNT) then exit;
  if Length(TsetBmp) <> TSET_COUNT then exit;
  if TsetBmp[i] <> nil then
  begin
    TsetBmp[i].Dispose;
    TsetBmp[i] := nil;
  end;
  SetLength(TsetGrid[i], 0, 0);
  path := TsetPath(i);
  if path = '' then exit;
  if not FileThere(path) then exit;
  if not ReadImageGrid(path, g, true) then exit;
  TsetGrid[i] := g;
  TsetBmp[i] := MakeThumb(g, TSET_THUMB);
end;

procedure TsetLoadAll;
begin
  SetLength(TsetGrid, TSET_COUNT);
  SetLength(TsetBmp, TSET_COUNT);
  for var i := 0 to TSET_COUNT - 1 do TsetLoadSlot(i);
end;

///Освобождает миниатюры. Без этого каждое открытие экрана оставляло бы
///за собой семнадцать картинок.
procedure TsetFree;
begin
  for var i := 0 to Length(TsetBmp) - 1 do
    if TsetBmp[i] <> nil then
    begin
      TsetBmp[i].Dispose;
      TsetBmp[i] := nil;
    end;
  SetLength(TsetBmp, 0);
  SetLength(TsetGrid, 0);
end;

function TsetCellX(slot: integer): integer;
begin
  Result := TSET_X0 + (slot mod TSET_COLS) * TSET_CELL_W;
end;

function TsetCellY(slot: integer): integer;
begin
  Result := TSET_Y0 + (slot div TSET_COLS) * TSET_CELL_H;
end;

function TsetHit(px, py: integer; var slot: integer): boolean;
var
  col, row: integer;
begin
  Result := false;
  slot := -1;
  if (px < TSET_X0) or (py < TSET_Y0) then exit;
  col := (px - TSET_X0) div TSET_CELL_W;
  row := (py - TSET_Y0) div TSET_CELL_H;
  if (col < 0) or (col >= TSET_COLS) or (row < 0) then exit;
  slot := row * TSET_COLS + col;
  if (slot < 0) or (slot >= TSET_COUNT) then
  begin
    slot := -1;
    exit;
  end;
  Result := true;
end;

///Экран набора. Возвращает true, если тайл слота открыт в редакторе:
///вызывающему тогда стоит уйти из меню в редактор, как после обычной загрузки.
function ShowTilesetScreen: boolean;
var
  done: boolean;
  slot, cx, cy, tx: integer;
  dir, path, note: string;
begin
  Result := false;
  TsetLoadAll;
  done := false;
  note := '';
  ClickPending := false;
  KeyPending := false;

  while not done do
  begin
    dir := TilesDir;
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 10, WIN_W, 44, 'Набор тайлов Andors-Love');
    if dir = '' then
      UILabel(0, 46, WIN_W, 20,
              'Папка набора не задана — укажите data/tiles игры', cWarn)
    else
      UILabel(0, 46, WIN_W, 20, FitText(dir, WIN_W - 40), cTextDim);
    UILabel(0, 66, WIN_W, 20,
            'Правая кнопка открывает тайл слота, левая пишет в слот текущий',
            cTextDim);

    for var i := 0 to TSET_COUNT - 1 do
    begin
      cx := TsetCellX(i);
      cy := TsetCellY(i);
      tx := cx + (TSET_CELL_W - TSET_THUMB) div 2;
      GraphABC.Brush.Color := cFace;
      FillRect(tx, cy + 2, tx + TSET_THUMB, cy + 2 + TSET_THUMB);
      if TsetBmp[i] <> nil then
        GraphBufferGraphics.DrawImage(TsetBmp[i], tx, cy + 2)
      else
        UILabel(cx, cy + 22, TSET_CELL_W, 18, 'нет файла', cTextDim);
      ///Слот под курсором обведён цветом выделения: попасть в нужный из
      ///семнадцати мелких клеток иначе трудно.
      if (MouseX >= cx) and (MouseX < cx + TSET_CELL_W) and
         (MouseY >= cy) and (MouseY < cy + TSET_CELL_H) then
        FrameRect(tx - 1, cy + 1, tx + TSET_THUMB + 1, cy + 3 + TSET_THUMB, cAccent)
      else
        FrameRect(tx - 1, cy + 1, tx + TSET_THUMB + 1, cy + 3 + TSET_THUMB, cBorder);
      if TsetIsTerrain(i) then GraphABC.Font.Color := cText
      else GraphABC.Font.Color := cAccent;
      UILabel(cx, cy + 60, TSET_CELL_W, 18,
              FitText(TsetName(i), TSET_CELL_W - 4), GraphABC.Font.Color);
      UILabel(cx, cy + 76, TSET_CELL_W, 18, TsetFile(i) + '.png', cTextDim);
    end;

    UILabel(0, 414, WIN_W, 18,
            'Чёрным — местность, она рисуется непрозрачной; синим — объекты, ' +
            'у них фон прозрачный', cTextDim);
    if note <> '' then UILabel(0, 432, WIN_W, 18, note, cAccent);

    if UIButton(WIN_W div 2 - 230, 458, 220, 34, 'Папка набора…', true) then
    begin
      path := AskOpenFile('Выберите любой тайл в папке набора');
      if path <> '' then
      begin
        try
          CfgTilesFolder := System.IO.Path.GetDirectoryName(path);
        except
          note := 'Не удалось разобрать путь';
        end;
        ConfigSave(ConfigPath);
        TsetLoadAll;
        note := '';
      end;
    end;
    if UIButton(WIN_W div 2 + 10, 458, 220, 34, 'Закрыть', true) then done := true;

    UILabel(0, H - 26, WIN_W, 20,
            'Пустой слот игра рисует по-старому — набор можно делать по одному тайлу',
            cTextDim);

    // Клик по слоту разбирается после кнопок: области не пересекаются,
    // поэтому порядок роли не играет, но так ближе к остальным экранам.
    if ClickPending and TsetHit(ClickX, ClickY, slot) then
    begin
      path := TsetPath(slot);
      if path = '' then
        note := 'Сначала укажите папку набора'
      else if ClickButton = MB_RIGHT then
      begin
        if not FileThere(path) then
          note := 'В слоте ' + TsetFile(slot) + ' файла ещё нет'
        else if ConfirmDiscard('Открыть тайл слота ' + TsetFile(slot) + '?') then
        begin
          if LoadTileFrom(path, false, true) then
          begin
            Result := true;
            done := true;
          end;
        end;
      end
      else if ClickButton = MB_LEFT then
      begin
        if DocMode <> DOC_TILE then
          note := 'Записать в слот можно только тайл, а сейчас открыта карта'
        else if FileThere(path) and
                (not AskYesNo('Заменить тайл в наборе',
                              'В слоте ' + TsetFile(slot) + ' уже есть файл. ' +
                              'Заменить его текущим тайлом ' + IntToStr(TW) +
                              ' на ' + IntToStr(TH) + '?',
                              'Заменить', 'Отмена')) then
          note := ''
        else
        begin
          if SaveTileTo(path) then
          begin
            TsetLoadSlot(slot);
            note := 'Записано в слот ' + TsetFile(slot);
          end;
        end;
      end;
    end;

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then done := true;
    Idle;
  end;

  TsetFree;
  WaitMouseRelease;
  NeedRepaint := true;
end;

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
  Add('Редактор пиксельной графики, тайлсетов и карт локаций.');
  Add('');
  Add('ДВА РЕЖИМА');
  Add('Тайл — рисование картинки. Карта — локация игры Andors-Love в формате');
  Add('data/maps/*.map. Переключение через меню; оба документа живут');
  Add('одновременно, переход туда и обратно ничего не теряет.');
  Add('');
  Add('ОБЩЕЕ ПРАВИЛО МЫШИ');
  Add('Левая кнопка применяет: рисует, заливает, кладёт цвет, ставит объект.');
  Add('Правая кнопка берёт: пипетка цвета или местности, убирает объект.');
  Add('Ролик как кнопка: протяжка двигает изображение, щелчок без протяжки');
  Add('меняет местами рабочие цвета.');
  Add('Вращение ролика меняет масштаб вокруг курсора.');
  Add('');
  Add('ОБЗОРНАЯ ПАНЕЛЬ');
  Add('Внизу панели справа документ показан целиком, жёлтая рамка отмечает');
  Add('видимую на холсте часть. Протяжка средней кнопкой по обзору двигает');
  Add('холст, а панорама холста двигает рамку: обзор и редактор всегда');
  Add('смотрят на одно место документа, только в разных масштабах.');
  Add('');
  Add('ИНСТРУМЕНТЫ РЕЖИМА ТАЙЛА');
  Add('Карандаш рисует первым цветом с учётом прозрачности.');
  Add('Ластик стирает в прозрачность.');
  Add('Заливка заполняет связную область одного цвета.');
  Add('Пипетка берёт цвет пикселя.');
  Add('Линия, прямоугольник и овал строятся протяжкой: пока кнопка нажата,');
  Add('виден предпросмотр. Ctrl во время протяжки делает фигуру заполненной,');
  Add('Shift приводит её к квадрату или кругу.');
  Add('Дизеринг кладёт шахматку из первого и второго цветов.');
  Add('Выделение: протяните рамку, чтобы выделить область и скопировать её');
  Add('в буфер. Щелчок без протяжки вставляет буфер. Протяжка за выделенную');
  Add('область копирует её на новое место.');
  Add('');
  Add('ИНСТРУМЕНТЫ РЕЖИМА КАРТЫ');
  Add('Местность рисует выбранным видом клетки, правая кнопка берёт вид');
  Add('из-под курсора. Заливка и прямоугольник работают тем же видом.');
  Add('Объект: левая кнопка ставит объект выбранного вида или перетаскивает');
  Add('уже стоящий, правая убирает, средняя открывает правку строки.');
  Add('Выделение копирует и вставляет куски местности.');
  Add('');
  Add('НАБОР ТАЙЛОВ ANDORS-LOVE');
  Add('Клавиша T открывает набор игры: семнадцать слотов, по картинке на слот.');
  Add('Игра ищет их по имени файла — floor.png, wall.png и так далее, — поэтому');
  Add('слоты здесь названы и расположены так же, как в самой игре.');
  Add('Правая кнопка открывает тайл слота в редакторе, левая записывает в слот');
  Add('текущий тайл. Папка набора задаётся в настройках или прямо на экране.');
  Add('Пустой слот — не ошибка: игра рисует его прежним способом, заливкой');
  Add('и знаком шрифта, поэтому набор можно делать по одному тайлу.');
  Add('');
  Add('ИНДЕКСНАЯ ПАЛИТРА');
  Add('Клавиша I связывает пиксели тайла с ячейками палитры того же цвета.');
  Add('После этого правка ячейки левой кнопкой перекрашивает все связанные');
  Add('с ней пиксели: набор тайлов перекрашивается сменой палитры, а не');
  Add('перерисовкой. Включённый режим виден по рамке палитры цветом выделения,');
  Add('а ячейка, из которой взят первый цвет, обведена ею же.');
  Add('Правка ячейки и перекраска тайла отменяются вместе, одним Z.');
  Add('Пиксель, не совпавший ни с одной ячейкой, остаётся свободным: чтобы');
  Add('связать весь тайл, сведите его к цветам палитры в операциях с цветом.');
  Add('Цвет, набранный ползунками, связи с палитрой не имеет, пока не совпадёт');
  Add('с какой-нибудь ячейкой. Пипетка берёт цвет вместе с его связью.');
  Add('');
  Add('БЕСШОВНОСТЬ И СИММЕТРИЯ');
  Add('Клавиша B включает бесшовный предпросмотр: тайл рисуется повторяющимся,');
  Add('и швы видно прямо во время работы. Клавиша W включает заворот через');
  Add('край: мазок, уходящий за правую границу, продолжается от левой.');
  Add('Клавиши F и D включают вертикальную и горизонтальную оси симметрии.');
  Add('');
  Add('КЛАВИАТУРА');
  Add('Escape — главное меню. H — эта справка.');
  Add('N создать, L загрузить, S сохранить, A сохранить выделение.');
  Add('Z отменить, Y повторить. V вставить буфер в позицию курсора.');
  Add('X поменять местами цвета. P точный выбор цвета по каналам ARGB.');
  Add('G сетка, C шахматка, B бесшовность, W заворот, F и D оси симметрии.');
  Add('E режим замены вместо смешивания. R случайные цвета.');
  Add('I индексная палитра. T набор тайлов Andors-Love.');
  Add('Delete очистить тайл. Tab переключить режим тайла и карты.');
  Add('Цифры от 1 выбирают инструмент. Плюс и минус — масштаб.');
  Add('Стрелки двигают изображение, пробел возвращает его в центр.');
  Add('Alt, зажатый над холстом, временно включает пипетку.');
  Add('');
  Add('ФОРМАТ КАРТЫ');
  Add('Сетка хранит только геометрию: пол, стена, вода, дерево, трава,');
  Add('дорога, стоячая вода. Всё остальное описано объектами: npc, item,');
  Add('exit, spawn, chest, note, bed, sign. Редактор разбирает у объекта');
  Add('только вид и координаты, а хвост строки хранит как есть и пишет');
  Add('обратно без изменений, поэтому условия перехода и содержимое сундуков');
  Add('не теряются. Проверка локации в свойствах карты находит объекты на');
  Add('непроходимых клетках и участки, до которых нельзя дойти.');
  Add('');
  Add('НАСТРОЙКИ');
  Add('Все параметры хранятся в файле Config.txt рядом с программой в');
  Add('кодировке UTF-8, по строке на параметр в виде «ключ = значение».');
  Add('Файл можно править вручную или через пункт «Настройки» в меню.');
  Add('При запуске программа пробует открыть стартовое изображение, и размер');
  Add('тайла берётся из него. Размер из настроек нужен только тогда, когда');
  Add('открыть картинку не удалось: тогда создаётся пустой тайл.');
  Add('');
  Add('БЛАГОДАРНОСТИ');
  Add('Автор программы — DeadPixel, vk.com/deadpixel_programmer.');
  Add('Команде PascalABC.NET за язык, среду разработки и библиотеку GraphABC,');
  Add('на которой построен весь интерфейс редактора.');
  Add('Игре Andors-Love за формат карт, ради которого сделан режим локаций.');
  Add('Всем, кто пробовал редактор, присылал замечания и находил ошибки.');
  Add('');
  Add('Программа распространяется по лицензии GNU GPL версии 3.');

  ShowTextScreen('Справка', p);
end;

///Экран настроек. Правки применяются к копиям и попадают в Config.txt
///только по кнопке «Сохранить».
procedure ShowSettingsScreen;
var
  done: boolean;
  vHelp, vMenu, vGrid, vChecker, vSeam, vAutoLast, vMapStart: boolean;
  vW, vH, vMW, vMH: integer;
  vStartup, vPalette, vMaps, vTiles: string;
  y, rowH, path1X, ctlX: integer;
  path: string;

  ///Строка с логическим переключателем.
  function ToggleRow(x, ry: integer; caption: string; value: boolean): boolean;
  begin
    UILabelLeft(x, ry, 30, caption, cText);
    Result := UIButton(x + 250, ry, 90, 30, RuBool(value), true);
  end;

  ///Строка с числом и шагом.
  function StepRow(ry: integer; caption: string; var value: integer;
                   lo, hi: integer): boolean;
  begin
    Result := false;
    UILabelLeft(60, ry, 30, caption, cText);
    if UIButton(300, ry, 44, 30, '−', value > lo) then
    begin
      value := ClampI(value - 1, lo, hi);
      Result := true;
    end;
    UILabel(348, ry, 70, 30, IntToStr(value), cText);
    if UIButton(422, ry, 44, 30, '+', value < hi) then
    begin
      value := ClampI(value + 1, lo, hi);
      Result := true;
    end;
    if UIButton(474, ry, 56, 30, '−8', value > lo + 7) then
    begin
      value := ClampI(value - 8, lo, hi);
      Result := true;
    end;
    if UIButton(536, ry, 56, 30, '+8', value < hi - 7) then
    begin
      value := ClampI(value + 8, lo, hi);
      Result := true;
    end;
  end;

begin
  vHelp := CfgShowHelpOnStart;
  vMenu := CfgShowMenuOnStart;
  vGrid := CfgShowGrid;
  vChecker := CfgShowChecker;
  vSeam := CfgShowSeamless;
  vAutoLast := CfgAutoLoadLast;
  vMapStart := CfgStartInMapMode;
  vW := CfgDefaultWidth;
  vH := CfgDefaultHeight;
  vMW := CfgDefaultMapWidth;
  vMH := CfgDefaultMapHeight;
  vStartup := CfgStartupImage;
  vPalette := CfgPaletteFile;
  vMaps := CfgMapsFolder;
  vTiles := CfgTilesFolder;

  rowH := 30;
  path1X := 200;
  ctlX := 400;
  done := false;
  ClickPending := false;
  KeyPending := false;

  while not done do
  begin
    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 14, WIN_W, 46, 'Настройки');
    UILabel(0, 46, WIN_W, 20, 'Сохраняются в Config.txt рядом с программой', cTextDim);

    y := 76;
    if ToggleRow(30, y, 'Справка при запуске', vHelp) then vHelp := not vHelp;
    if ToggleRow(410, y, 'Меню при запуске', vMenu) then vMenu := not vMenu;
    y := y + 36;
    if ToggleRow(30, y, 'Сетка между пикселями', vGrid) then vGrid := not vGrid;
    if ToggleRow(410, y, 'Шахматка прозрачности', vChecker) then vChecker := not vChecker;
    y := y + 36;
    if ToggleRow(30, y, 'Бесшовный предпросмотр', vSeam) then vSeam := not vSeam;
    if ToggleRow(410, y, 'Открывать последний файл', vAutoLast) then vAutoLast := not vAutoLast;
    y := y + 36;
    if ToggleRow(30, y, 'Запускаться в режиме карты', vMapStart) then vMapStart := not vMapStart;

    y := y + 40;
    StepRow(y, 'Ширина нового тайла', vW, MIN_TILE, MAX_TILE);
    y := y + 36;
    StepRow(y, 'Высота нового тайла', vH, MIN_TILE, MAX_TILE);
    y := y + 36;
    StepRow(y, 'Ширина новой карты', vMW, MAP_MIN_SIDE, MAP_MAX_SIDE);
    y := y + 36;
    StepRow(y, 'Высота новой карты', vMH, MAP_MIN_SIDE, MAP_MAX_SIDE);

    y := y + 38;
    UILabelLeft(30, y, rowH, 'Стартовое изображение', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(path1X, y, ctlX + 120, y + rowH, 4, 4);
    UILabelLeft(path1X + 6, y, rowH, FitText(vStartup, ctlX + 108 - path1X), cText);
    if UIButton(ctlX + 130, y, 100, rowH, 'Выбрать', true) then
    begin
      path := AskOpenFile('Стартовое изображение');
      if path <> '' then vStartup := path;
    end;
    if UIButton(ctlX + 236, y, 100, rowH, 'Очистить', vStartup <> '') then vStartup := '';

    y := y + 32;
    UILabelLeft(30, y, rowH, 'Файл палитры', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(path1X, y, ctlX + 120, y + rowH, 4, 4);
    UILabelLeft(path1X + 6, y, rowH, FitText(vPalette, ctlX + 108 - path1X), cText);
    if UIButton(ctlX + 130, y, 100, rowH, 'Выбрать', true) then
    begin
      path := AskOpenFile('Файл палитры');
      if path <> '' then vPalette := path;
    end;
    if UIButton(ctlX + 236, y, 100, rowH, 'Очистить', vPalette <> '') then vPalette := '';

    y := y + 32;
    UILabelLeft(30, y, rowH, 'Папка карт локаций', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(path1X, y, ctlX + 120, y + rowH, 4, 4);
    UILabelLeft(path1X + 6, y, rowH, FitText(vMaps, ctlX + 108 - path1X), cText);
    if UIButton(ctlX + 130, y, 100, rowH, 'Выбрать', true) then
    begin
      // Папку выбираем через любой файл в ней: отдельного диалога папки
      // в GraphABC нет, а этот способ не требует лишних зависимостей.
      path := AskOpenMap('Выберите любую карту в нужной папке');
      if path <> '' then
        try
          vMaps := System.IO.Path.GetDirectoryName(path);
        except
          vMaps := vMaps;
        end;
    end;
    if UIButton(ctlX + 236, y, 100, rowH, 'Очистить', vMaps <> '') then vMaps := '';

    y := y + 32;
    UILabelLeft(30, y, rowH, 'Папка набора тайлов', cText);
    GraphABC.Brush.Color := cFace;
    FillRoundRect(path1X, y, ctlX + 120, y + rowH, 4, 4);
    UILabelLeft(path1X + 6, y, rowH, FitText(vTiles, ctlX + 108 - path1X), cText);
    if UIButton(ctlX + 130, y, 100, rowH, 'Выбрать', true) then
    begin
      path := AskOpenFile('Выберите любой тайл в нужной папке');
      if path <> '' then
        try
          vTiles := System.IO.Path.GetDirectoryName(path);
        except
          vTiles := vTiles;
        end;
    end;
    if UIButton(ctlX + 236, y, 100, rowH, 'Очистить', vTiles <> '') then vTiles := '';

    if UIButton(WIN_W div 2 - 190, H - 52, 180, 36, 'Сохранить', true) then
    begin
      CfgShowHelpOnStart := vHelp;
      CfgShowMenuOnStart := vMenu;
      CfgShowGrid := vGrid;
      CfgShowChecker := vChecker;
      CfgShowSeamless := vSeam;
      CfgAutoLoadLast := vAutoLast;
      CfgStartInMapMode := vMapStart;
      CfgDefaultWidth := vW;
      CfgDefaultHeight := vH;
      CfgDefaultMapWidth := vMW;
      CfgDefaultMapHeight := vMH;
      CfgStartupImage := vStartup;
      CfgPaletteFile := vPalette;
      CfgMapsFolder := vMaps;
      CfgTilesFolder := vTiles;
      GridOn := vGrid;
      CheckerOn := vChecker;
      SeamlessOn := vSeam;
      if not ConfigSave(ConfigPath) then
        ShowMessage('Настройки не сохранены',
                    'Не удалось записать файл ' + ConfigPath +
                    '. Проверьте права на запись в папку программы.');
      done := true;
    end;
    if UIButton(WIN_W div 2 + 10, H - 52, 180, 36, 'Отмена', true) then done := true;

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then done := true;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

procedure ShowMainMenu;
var
  done: boolean;
  y, bx, bw, bh, stp: integer;
  lastPath, lastCaption, info, curName: string;
  lastOk, isMap: boolean;
begin
  done := false;
  ClickPending := false;
  KeyPending := false;
  bw := 420;
  ///Кнопок в режиме тайла двенадцать, и они должны уместиться над подписью
  ///внизу окна: отсюда шаг мельче обычного.
  bh := 32;
  stp := 36;
  bx := (WIN_W - bw) div 2;

  while not done do
  begin
    isMap := DocMode = DOC_MAP;
    if isMap then
    begin
      lastPath := ResolvePath(CfgLastMapFile);
      curName := FileNameOnly(CurrentMapFile);
      if curName = '' then curName := 'новая карта';
      if CfgLastMapFile = '' then lastCaption := 'Открыть последнюю карту — нет'
      else lastCaption := 'Открыть последнюю карту — ' + FileNameOnly(CfgLastMapFile);
    end
    else
    begin
      lastPath := ResolvePath(CfgLastFile);
      curName := FileNameOnly(CurrentFile);
      if curName = '' then curName := 'новый тайл';
      if CfgLastFile = '' then lastCaption := 'Открыть последний файл — нет'
      else lastCaption := 'Открыть последний файл — ' + FileNameOnly(CfgLastFile);
    end;
    lastOk := FileThere(lastPath);

    info := curName + '   ' + IntToStr(DocW) + ' на ' + IntToStr(DocH);
    if isMap then info := info + '   объектов ' + IntToStr(Length(MapObjects));
    if IsModified then info := info + '   есть несохранённые изменения';

    ClearWindow(cBack);
    GraphABC.Font.Color := cText;
    DrawTextCentered(0, 16, WIN_W, 52, APP_TITLE);
    if isMap then
      UILabel(0, 52, WIN_W, 22, 'режим карты локации', cAccent)
    else
      UILabel(0, 52, WIN_W, 22, 'режим тайла', cAccent);
    UILabel(0, 74, WIN_W, 22, info, cTextDim);

    y := 100;
    if UIButton(bx, y, bw, bh, 'Продолжить редактирование', true) then done := true;

    y := y + stp;
    if UIButton(bx, y, bw, bh, FitText(lastCaption, bw - 20), lastOk) then
    begin
      CommandOpenLast;
      done := true;
    end;

    y := y + stp;
    if isMap then
    begin
      if UIButton(bx, y, bw, bh, 'Создать карту', true) then
      begin
        CommandNew;
        done := true;
      end;
    end
    else
      if UIButton(bx, y, bw, bh, 'Создать файл', true) then
      begin
        CommandNew;
        done := true;
      end;

    y := y + stp;
    if isMap then
    begin
      if UIButton(bx, y, bw, bh, 'Загрузить карту', true) then
      begin
        CommandOpen;
        done := true;
      end;
    end
    else
      if UIButton(bx, y, bw, bh, 'Загрузить файл', true) then
      begin
        CommandOpen;
        done := true;
      end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Сохранить', true) then
      if CommandSave then done := true;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Сохранить как', true) then
      if CommandSaveAs then done := true;

    y := y + stp;
    if isMap then
    begin
      if UIButton(bx, y, bw, bh, 'Свойства и проверка локации', true) then
        ShowMapPropertiesScreen;
    end
    else
      if UIButton(bx, y, bw, bh, 'Набор тайлов Andors-Love', true) then
        if ShowTilesetScreen then done := true;

    if not isMap then
    begin
      y := y + stp;
      if UIButton(bx, y, bw, bh, 'Операции с цветом', true) then
        ShowColorOpsScreen;
    end;

    y := y + stp;
    if isMap then
    begin
      if UIButton(bx, y, bw, bh, 'Перейти к редактору тайлов', true) then
      begin
        SwitchMode(DOC_TILE);
        done := true;
      end;
    end
    else
      if UIButton(bx, y, bw, bh, 'Перейти к редактору карт', true) then
      begin
        SwitchMode(DOC_MAP);
        done := true;
      end;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Настройки', true) then ShowSettingsScreen;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Справка и благодарности', true) then ShowHelpScreen;

    y := y + stp;
    if UIButton(bx, y, bw, bh, 'Выход', true) then CommandExit;

    UILabel(0, H - 26, WIN_W, 22,
            'версия ' + APP_VERSION + '   ·   настройки в Config.txt   ·   Escape закрывает меню',
            cTextDim);

    Redraw;
    ClickPending := false;
    if TakeKey = KEY_ESC then done := true;
    Idle;
  end;

  WaitMouseRelease;
  NeedRepaint := true;
end;

// ---------------------------------------------------------------------------
// Инструменты холста
// ---------------------------------------------------------------------------

///Мазок кистью. Промежуточные точки достраиваются отрезком, поэтому
///при быстром движении мыши в линии не остаётся пропусков.
procedure DoPaintStroke(startX, startY: integer);
var
  lastX, lastY, mx, my: integer;
begin
  PushUndo;
  SetLength(StrokeMask, DocW, DocH);
  LiveDraw := DocMode = DOC_TILE;
  lastX := startX;
  lastY := startY;

  System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
  try
    EmitPixel(startX, startY);
  finally
    System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
  end;
  if DocMode = DOC_MAP then RepaintCanvasOnly else Redraw;

  while MousePressed do
  begin
    mx := ScreenToTileX(MouseX);
    my := ScreenToTileY(MouseY);
    if (mx <> lastX) or (my <> lastY) then
    begin
      System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
      try
        ShapeLine(lastX, lastY, mx, my);
      finally
        System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
      end;
      lastX := mx;
      lastY := my;
      if DocMode = DOC_MAP then RepaintCanvasOnly else Redraw;
    end;
    Idle;
  end;

  LiveDraw := false;
  SetLength(StrokeMask, 0, 0);
  NeedRepaint := true;
end;

///Раскладывает фигуру текущего инструмента. Один и тот же вызов служит
///и предпросмотром, и применением: разницу задаёт флаг ShapePreview.
procedure EmitShape(ax, ay, bx, by: integer);
var
  filled: boolean;
begin
  filled := CtrlDown;
  if DocMode = DOC_MAP then
  begin
    ShapeRect(ax, ay, bx, by, filled);
    exit;
  end;
  case ToolTile of
    T_LINE: ShapeLine(ax, ay, bx, by);
    T_RECT: ShapeRect(ax, ay, bx, by, filled);
    T_ELLIPSE: ShapeEllipse(ax, ay, bx, by, filled);
  end;
end;

///Линия, прямоугольник или овал: протяжка с предпросмотром,
///запись в документ по отпусканию кнопки.
procedure DoShapeTool(ax, ay: integer);
var
  bx, by, nx, ny: integer;
begin
  bx := ax;
  by := ay;

  while MousePressed do
  begin
    nx := ScreenToTileX(MouseX);
    ny := ScreenToTileY(MouseY);
    if ShiftDown then SquarifyTo(ax, ay, nx, ny);
    if (nx <> bx) or (ny <> by) then
    begin
      bx := nx;
      by := ny;
      System.Threading.Monitor.Enter(GraphABC.GraphABCControl);
      try
        DrawCanvas;
        ShapePreview := true;
        EmitShape(ax, ay, bx, by);
        ShapePreview := false;
      finally
        System.Threading.Monitor.Exit(GraphABC.GraphABCControl);
      end;
      Redraw;
    end;
    Idle;
  end;

  PushUndo;
  SetLength(StrokeMask, DocW, DocH);
  EmitShape(ax, ay, bx, by);
  SetLength(StrokeMask, 0, 0);
  NeedRepaint := true;
end;

///Протяжка рамки выделения. Обе граничные точки заведомо внутри документа,
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
    nx := ClampI(ScreenToTileX(MouseX), 0, DocW - 1);
    ny := ClampI(ScreenToTileY(MouseY), 0, DocH - 1);
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
    if BufferFilled then
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
  SelW := Min(SelW, DocW);
  SelH := Min(SelH, DocH);
  SelX := ClampI(SelX, 0, DocW - SelW);
  SelY := ClampI(SelY, 0, DocH - SelH);
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

///Перетаскивание объекта карты на новую клетку.
procedure DragObject(idx: integer);
var
  nx, ny: integer;
  moved: boolean;
begin
  moved := false;
  while MousePressed do
  begin
    nx := ClampI(ScreenToTileX(MouseX), 0, MapW - 1);
    ny := ClampI(ScreenToTileY(MouseY), 0, MapH - 1);
    if (nx <> MapObjects[idx].X) or (ny <> MapObjects[idx].Y) then
    begin
      if not moved then
      begin
        PushUndo;
        moved := true;
      end;
      MapObjects[idx].X := nx;
      MapObjects[idx].Y := ny;
      RepaintCanvasOnly;
    end;
    Idle;
  end;
  NeedRepaint := true;
end;

procedure DoObjectTool(mx, my: integer);
var
  idx: integer;
begin
  idx := MapObjectAt(mx, my);
  if ClickButton = MB_RIGHT then
  begin
    if idx >= 0 then
    begin
      PushUndo;
      MapDeleteObject(idx);
      NeedRepaint := true;
    end;
    exit;
  end;
  if idx >= 0 then
    DragObject(idx)
  else
  begin
    PushUndo;
    MapAddObject(MapKindName(MapObjKind), mx, my, MapKindTemplate(MapObjKind));
    NeedRepaint := true;
  end;
end;

///Протяжка средней кнопкой двигает изображение. Щелчок без протяжки
///меняет местами рабочие цвета: обе привычки сохранены и не мешают друг другу.
procedure DoMiddleDrag;
var
  sx, sy, vx, vy: integer;
  moved: boolean;
begin
  sx := ClickX;
  sy := ClickY;
  vx := ViewX;
  vy := ViewY;
  moved := false;
  while MousePressed do
  begin
    if (Abs(MouseX - sx) > 2) or (Abs(MouseY - sy) > 2) then moved := true;
    if moved then
    begin
      ViewX := vx + (MouseX - sx);
      ViewY := vy + (MouseY - sy);
      ClampView;
      RepaintCanvasAndOverview;
    end;
    Idle;
  end;
  if moved then NeedRepaint := true else SwapColors;
end;

procedure HandleMapCanvas(mx, my: integer);
begin
  // Правая кнопка и Alt берут вид местности из-под курсора.
  // Инструмент объектов обрабатывает правую кнопку сам.
  if ((ClickButton = MB_RIGHT) or AltDown) and (ToolMap <> M_OBJ) then
  begin
    MapBrush := MapGrid[mx, my];
    NeedRepaint := true;
    exit;
  end;
  case ToolMap of
    M_PAINT: DoPaintStroke(mx, my);
    M_FILL:
      begin
        PushUndo;
        FloodFillMap(mx, my, MapBrush);
        NeedRepaint := true;
      end;
    M_RECT: DoShapeTool(mx, my);
    M_OBJ: DoObjectTool(mx, my);
    M_SELECT: DoSelectTool(mx, my);
  end;
end;

procedure HandleCanvasClick;
var
  mx, my, idx: integer;
begin
  mx := ScreenToTileX(ClickX);
  my := ScreenToTileY(ClickY);

  if IsMiddle(ClickButton) then
  begin
    // Над объектом средняя кнопка открывает правку его строки.
    if (DocMode = DOC_MAP) and (ToolMap = M_OBJ) and InDoc(mx, my) then
    begin
      idx := MapObjectAt(mx, my);
      if idx >= 0 then
      begin
        EditObject(idx);
        exit;
      end;
    end;
    DoMiddleDrag;
    exit;
  end;

  if not InDoc(mx, my) then
  begin
    if HasSelection then
    begin
      ResetSelection;
      NeedRepaint := true;
    end;
    exit;
  end;

  if DocMode = DOC_MAP then
  begin
    HandleMapCanvas(mx, my);
    exit;
  end;

  if (ClickButton = MB_RIGHT) or AltDown or (ToolTile = T_PICK) then
  begin
    PickColorFromTile(mx, my);
    exit;
  end;

  case ToolTile of
    T_PEN, T_ERASER, T_DITHER: DoPaintStroke(mx, my);
    T_FILL:
      begin
        PushUndo;
        FloodFillTile(mx, my, FirstColor);
        NeedRepaint := true;
      end;
    T_LINE, T_RECT, T_ELLIPSE: DoShapeTool(mx, my);
    T_SELECT: DoSelectTool(mx, my);
  end;
end;

// ---------------------------------------------------------------------------
// Панель инструментов
// ---------------------------------------------------------------------------

procedure DragHSVSquare;
begin
  while MousePressed do
  begin
    Sat := AxisToPercent(MouseX - HSV_X0, BAR_SIZE);
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
    Hue := AxisToHue(MouseX - HSV_X0, BAR_SIZE);
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
    FirstColor := ARGB(AxisToByte(MouseX - HSV_X0, BAR_SIZE),
                       FirstColor.R, FirstColor.G, FirstColor.B);
    RepaintColorOnly;
    Idle;
  end;
  NeedRepaint := true;
end;

procedure HandlePaletteClick;
var
  cj, ci: integer;
  idx: byte;
  oldC: Color;
begin
  if IsMiddle(ClickButton) then
  begin
    PaletteIO;
    exit;
  end;
  if not PalHit(ClickX, ClickY, cj, ci) then exit;
  idx := PalIndexAt(cj, ci);

  if ClickButton = MB_RIGHT then
  begin
    PickColor(Palette[cj, ci]);
    ///Цвет взят из конкретной ячейки, поэтому привязка ставится точная,
    ///а не выведенная по цвету: когда один цвет стоит в палитре дважды,
    ///важно, из какой именно ячейки его взяли.
    FirstIdx := idx;
    exit;
  end;

  oldC := Palette[cj, ci];
  if SameColor(oldC, FirstColor) then exit;

  if IndexedOn and (DocMode = DOC_TILE) then
  begin
    ///Пиксели и сама ячейка меняются одной операцией и одной же отменяются.
    PushUndo;
    Palette[cj, ci] := FirstColor;
    RecolorPaletteEntry(idx, oldC, FirstColor);
    ///Второй рабочий цвет, взятый из этой ячейки, едет за ней следом.
    if SecondIdx = idx then
      SecondColor := ARGB(SecondColor.A, FirstColor.R, FirstColor.G, FirstColor.B);
    OverDirty := true;
  end
  else
    Palette[cj, ci] := FirstColor;

  NeedRepaint := true;
end;

///Протяжка по обзору двигает холст. Рамка видимой области при этом идёт
///за курсором, а обратно панорама холста двигает саму рамку.
procedure DoOverviewDrag;
begin
  while MousePressed do
  begin
    OverviewCenterOn(MouseX, MouseY);
    RepaintCanvasAndOverview;
    Idle;
  end;
  NeedRepaint := true;
end;

///Разбор клика по панели. Ветки взаимоисключающие, поэтому клик
///обрабатывается ровно одной секцией.
procedure HandlePanelClick;
var
  py, idx: integer;
begin
  py := ClickY;

  if (py >= TOOLS_Y0) and (py < TOOLS_Y1) then
  begin
    if ToolHit(ClickX, ClickY, idx) then SetTool(idx);
  end

  else if (py >= OVER_Y0) and (py < OVER_Y1) then
  begin
    if IsMiddle(ClickButton) then DoOverviewDrag
    else OverviewCenterOn(ClickX, ClickY);
  end

  else if (py >= MENU_Y0) and (py < MENU_Y1) then
    ShowMainMenu

  else if DocMode = DOC_MAP then
  begin
    if (py >= SLOT1_Y0) and (py < SLOT1_Y1) then
    begin
      if TerrHit(ClickX, ClickY, idx) then
      begin
        MapBrush := idx;
        NeedRepaint := true;
      end;
    end
    else if (py >= SLOT2_Y0) and (py < ALPHA_Y1) then
      if ObjHit(ClickX, ClickY, idx) then
      begin
        MapObjKind := idx;
        NeedRepaint := true;
      end;
  end

  else
  begin
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
    end;
  end;
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
    KEY_ESC, KEY_M: ShowMainMenu;
    KEY_H: ShowHelpScreen;
    KEY_N: CommandNew;
    KEY_L: CommandOpen;
    KEY_S: CommandSave;
    KEY_A: CommandSaveSelection;
    KEY_P: if DocMode = DOC_TILE then ShowColorScreen;
    KEY_Z: DoUndo;
    KEY_Y: DoRedo;
    KEY_X: SwapColors;
    KEY_R: if DocMode = DOC_TILE then RandomizeTile;
    KEY_DELETE: if DocMode = DOC_TILE then ClearTile;
    KEY_TAB:
      begin
        if DocMode = DOC_MAP then SwitchMode(DOC_TILE) else SwitchMode(DOC_MAP);
      end;
    KEY_V:
      if BufferFilled then
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
    KEY_B:
      begin
        SeamlessOn := not SeamlessOn;
        CfgShowSeamless := SeamlessOn;
        ConfigSave(ConfigPath);
        NeedRepaint := true;
      end;
    KEY_W: begin WrapOn := not WrapOn; NeedRepaint := true end;
    KEY_F: begin SymX := not SymX; NeedRepaint := true end;
    KEY_D: begin SymY := not SymY; NeedRepaint := true end;
    KEY_E: begin BrushReplace := not BrushReplace; NeedRepaint := true end;
    KEY_T: if DocMode = DOC_TILE then ShowTilesetScreen;
    KEY_I:
      if DocMode = DOC_TILE then
      begin
        IndexedOn := not IndexedOn;
        ///Включая режим, привязываем то, что уже нарисовано: иначе он
        ///подействовал бы только на пиксели, положенные после включения.
        if IndexedOn then RebindTileToPalette;
        NeedRepaint := true;
      end;
    KEY_1: SetTool(0);
    KEY_2: SetTool(1);
    KEY_3: SetTool(2);
    KEY_4: SetTool(3);
    KEY_5: SetTool(4);
    KEY_6: SetTool(5);
    KEY_7: SetTool(6);
    KEY_8: SetTool(7);
    KEY_9: SetTool(8);
    KEY_PLUS, KEY_NUMPLUS: ZoomAt(1, W div 2, H div 2);
    KEY_MINUS, KEY_NUMMINUS: ZoomAt(-1, W div 2, H div 2);
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

procedure InitCollections;
begin
  SetLength(Palette, PAL_COLS, PAL_ROWS);
  for var i := 0 to PAL_ROWS - 1 do
    for var j := 0 to PAL_COLS - 1 do
      Palette[j, i] := HSVtoRGB(j * 45, 30 + i * 10, 100 - i * 8);
  SetLength(Buffer, 0, 0);
  SetLength(MapBuffer, 0, 0);
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

procedure SyncTitle;
var
  t: string;
begin
  t := APP_TITLE + ' ' + APP_VERSION + ' — ';
  if DocMode = DOC_MAP then
  begin
    if CurrentMapFile = '' then t := t + 'новая карта'
    else t := t + FileNameOnly(CurrentMapFile);
  end
  else
  begin
    if CurrentFile = '' then t := t + 'новый тайл'
    else t := t + FileNameOnly(CurrentFile);
  end;
  if IsModified then t := t + ' *';
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
  OnKeyPress := OnKeyPressHandler;
  OnMouseDown := OnMouseDownHandler;
  OnMouseMove := OnMouseMoveHandler;
  OnMouseUp := OnMouseUpHandler;
  GraphABC.GraphABCControl.MouseWheel += OnWheelHandler;

  // Отсутствующий файл настроек не ошибка: берутся значения по умолчанию
  // и файл создаётся, чтобы его было легко найти и поправить.
  if not ConfigLoad(ConfigPath) then ConfigSave(ConfigPath);
  GridOn := CfgShowGrid;
  CheckerOn := CfgShowChecker;
  SeamlessOn := CfgShowSeamless;

  FirstColor := ARGB(255, 0, 0, 0);
  SecondColor := ARGB(255, 255, 255, 255);
  SyncHSVFromColor;

  InitCollections;
  InitColorBars;
  if CfgPaletteFile <> '' then LoadPaletteFrom(ResolvePath(CfgPaletteFile), true);

  // Порядок открытия документа при запуске. Размер из настроек нужен
  // только тогда, когда открыть картинку не удалось.
  loaded := false;
  if CfgAutoLoadLast and (CfgLastFile <> '') then
    loaded := LoadTileFrom(ResolvePath(CfgLastFile), true, false);
  if (not loaded) and (CfgStartupImage <> '') then
    loaded := LoadTileFrom(ResolvePath(CfgStartupImage), true, false);
  if not loaded then
    NewTileOfSize(CfgDefaultWidth, CfgDefaultHeight);

  // Карта готовится сразу, чтобы переключение режима ничего не спрашивало.
  MapNew(CfgDefaultMapWidth, CfgDefaultMapHeight);
  MapModified := false;
  if CfgStartInMapMode then
  begin
    if (CfgLastMapFile <> '') and FileThere(ResolvePath(CfgLastMapFile)) then
      LoadMapFrom(ResolvePath(CfgLastMapFile), true, false)
    else
      DocMode := DOC_MAP;
    FitScale;
  end;

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
    ZoomAt(1, Min(MouseX, W - 1), MouseY);
    wheel := wheel - 1;
  end;
  while wheel < 0 do
  begin
    ZoomAt(-1, Min(MouseX, W - 1), MouseY);
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
        DrawInfoSection;
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
