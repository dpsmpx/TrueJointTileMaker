// Харнесс внутренностей TJTM.pas. tools/golden.sh отрезает от программы
// главный блок и подставляет этот: процедуры оригинала — кисть, фигуры,
// заливка, буферы, палитра, панорама, обзор — вызываются напрямую.
//
// Сценарии порождает простой генератор с известным зерном; тест переноса
// (tests/test_model.cpp) порождает те же и сравнивает хэши состояния.

var
  gOut: List<string> := new List<string>;
  gH: int64;
  gSeed: int64;

procedure HReset; begin gH := 2166136261; end;
procedure HByte(b: integer); begin gH := ((gH xor (b and 255)) * 16777619) and $FFFFFFFF; end;
procedure HInt(v: integer);
begin
  HByte(v); HByte(v shr 8); HByte(v shr 16); HByte(v shr 24);
end;
function HHex: string; begin Result := gH.ToString('x8'); end;

function Next: integer;
begin
  gSeed := (gSeed * 1103515245 + 12345) and $7FFFFFFF;
  Result := integer(gSeed shr 4);
end;

function NextIn(lo, hi: integer): integer;
begin
  Result := lo + Next mod (hi - lo + 1);
end;

function RandColor: Color;
begin
  var k := NextIn(0, 3);
  case k of
    0: Result := Palette[NextIn(0, 7), NextIn(0, 7)];
    1: Result := RGB(NextIn(0, 255), NextIn(0, 255), NextIn(0, 255));
    2: Result := ARGB(NextIn(1, 254), NextIn(0, 255), NextIn(0, 255), NextIn(0, 255));
    else Result := ARGB(0, 0, 0, 0);
  end;
end;

procedure HashTile;
begin
  for var y := 0 to TH - 1 do
    for var x := 0 to TW - 1 do
    begin
      HInt(Tile[x, y].ToArgb);
      if (Length(TileIdx, 0) = TW) and (Length(TileIdx, 1) = TH) then HInt(TileIdx[x, y]) else HInt(-1);
    end;
end;

procedure HashMap;
begin
  for var y := 0 to MapH - 1 do
    for var x := 0 to MapW - 1 do
      HByte(MapGrid[x, y]);
end;

/// Случайный тайл: цвета палитры, свободные, полупрозрачные и прозрачные.
procedure RandomTile(w, h: integer);
var
  t: array[,] of Color;
begin
  SetLength(t, w, h);
  for var y := 0 to h - 1 do
    for var x := 0 to w - 1 do
      t[x, y] := RandColor;
  Tile := t;
  TW := w;
  TH := h;
  SetLength(TileIdx, 0, 0);
  RebindTileToPalette;
end;

/// Тайл без содержимого: для сценариев, где важен только размер.
procedure EmptyTile(w, h: integer);
var
  t: array[,] of Color;
begin
  SetLength(t, w, h);
  Tile := t;
  TW := w;
  TH := h;
  SetLength(TileIdx, 0, 0);
end;

procedure RandomMap(w, h: integer);
begin
  MapNew(w, h);
  for var y := 0 to MapH - 1 do
    for var x := 0 to MapW - 1 do
      MapGrid[x, y] := NextIn(0, MT_COUNT - 1);
end;

procedure ResetBrushState;
begin
  DocMode := DOC_TILE;
  ShapePreview := false;
  LiveDraw := false;
  WrapOn := false;
  SymX := false;
  SymY := false;
  BrushReplace := false;
  ToolTile := T_PEN;
end;

// --- сценарии ---------------------------------------------------------------

procedure TestDefaultPalette;
begin
  InitCollections;
  for var i := 0 to PAL_ROWS - 1 do
  begin
    var s := 'palette';
    for var j := 0 to PAL_COLS - 1 do s := s + ' ' + IntToStr(Palette[j, i].ToArgb);
    gOut.Add(s);
  end;
end;

procedure TestShapes;
begin
  gSeed := 4242;
  for var k := 0 to 2999 do
  begin
    InitCollections;
    ResetBrushState;
    RandomTile(NextIn(1, 26), NextIn(1, 22));
    var tools: array of integer := (T_PEN, T_ERASER, T_DITHER);
    ToolTile := tools[NextIn(0, 2)];
    BrushReplace := NextIn(0, 1) = 1;
    WrapOn := NextIn(0, 1) = 1;
    SymX := NextIn(0, 2) = 0;
    SymY := NextIn(0, 2) = 0;
    FirstColor := RandColor;
    SecondColor := RandColor;
    FirstIdx := IndexOfColor(FirstColor);
    SecondIdx := IndexOfColor(SecondColor);
    if NextIn(0, 4) = 0 then FirstIdx := NextIn(0, 63);
    var useMask := NextIn(0, 3) <> 0;
    SetLength(StrokeMask, 0, 0);
    if useMask then SetLength(StrokeMask, TW, TH);
    var kind := NextIn(0, 5);
    var x0 := NextIn(-6, TW + 5);
    var y0 := NextIn(-6, TH + 5);
    var x1 := NextIn(-6, TW + 5);
    var y1 := NextIn(-6, TH + 5);
    case kind of
      0: ShapeLine(x0, y0, x1, y1);
      1: ShapeRect(x0, y0, x1, y1, false);
      2: ShapeRect(x0, y0, x1, y1, true);
      3: ShapeEllipse(x0, y0, x1, y1, false);
      4: ShapeEllipse(x0, y0, x1, y1, true);
      else
      begin
        // Мазок: первая точка, затем отрезки к следующим, как DoPaintStroke.
        EmitPixel(x0, y0);
        var lx := x0;
        var ly := y0;
        var steps := NextIn(1, 6);
        for var s := 1 to steps do
        begin
          var nx := NextIn(-6, TW + 5);
          var ny := NextIn(-6, TH + 5);
          ShapeLine(lx, ly, nx, ny);
          lx := nx;
          ly := ny;
        end;
      end;
    end;
    HReset;
    HashTile;
    gOut.Add('shape ' + IntToStr(k) + ' ' + HHex);
  end;
end;

procedure TestMapShapes;
begin
  gSeed := 777;
  for var k := 0 to 999 do
  begin
    ResetBrushState;
    DocMode := DOC_MAP;
    RandomMap(NextIn(4, 30), NextIn(4, 20));
    MapBrush := NextIn(0, MT_COUNT - 1);
    SymX := NextIn(0, 2) = 0;
    SymY := NextIn(0, 2) = 0;
    SetLength(StrokeMask, 0, 0);
    if NextIn(0, 3) <> 0 then SetLength(StrokeMask, MapW, MapH);
    var kind := NextIn(0, 3);
    var x0 := NextIn(-3, MapW + 2);
    var y0 := NextIn(-3, MapH + 2);
    var x1 := NextIn(-3, MapW + 2);
    var y1 := NextIn(-3, MapH + 2);
    case kind of
      0: ShapeLine(x0, y0, x1, y1);
      1: ShapeRect(x0, y0, x1, y1, false);
      2: ShapeRect(x0, y0, x1, y1, true);
      else FloodFillMap(x0, y0, MapBrush);
    end;
    HReset;
    HashMap;
    gOut.Add('mapshape ' + IntToStr(k) + ' ' + HHex);
  end;
  DocMode := DOC_TILE;
end;

procedure TestFlood;
begin
  gSeed := 99;
  for var k := 0 to 999 do
  begin
    InitCollections;
    ResetBrushState;
    var w := NextIn(1, 30);
    var h := NextIn(1, 30);
    // Мало разных цветов — чтобы области были крупными.
    var cols: array of Color;
    SetLength(cols, NextIn(1, 4));
    for var i := 0 to cols.Length - 1 do cols[i] := RandColor;
    var t: array[,] of Color;
    SetLength(t, w, h);
    for var y := 0 to h - 1 do
      for var x := 0 to w - 1 do
        t[x, y] := cols[NextIn(0, cols.Length - 1)];
    Tile := t;
    TW := w;
    TH := h;
    SetLength(TileIdx, 0, 0);
    RebindTileToPalette;
    var c := RandColor;
    FloodFillTile(NextIn(-1, w), NextIn(-1, h), c);
    HReset;
    HashTile;
    gOut.Add('flood ' + IntToStr(k) + ' ' + HHex);
  end;
end;

procedure TestPaletteOps;
begin
  gSeed := 31337;
  for var k := 0 to 499 do
  begin
    InitCollections;
    ResetBrushState;
    // Палитра с повторами — проверка того, какая из одинаковых ячеек берётся.
    for var i := 0 to 7 do
      for var j := 0 to 7 do
        if NextIn(0, 5) = 0 then Palette[j, i] := Palette[NextIn(0, 7), NextIn(0, 7)];
    RandomTile(NextIn(1, 20), NextIn(1, 20));
    var op := NextIn(0, 4);
    var n := 0;
    case op of
      0: QuantizeTileToPalette;
      1: n := RebindTileToPalette;
      2:
        begin
          var idx: byte := NextIn(0, 63);
          var oldC := PalColorOf(idx);
          var newC := RandColor;
          Palette[idx mod 8, idx div 8] := newC;
          n := RecolorPaletteEntry(idx, oldC, newC);
        end;
      3: n := ReplaceColorEverywhere(Tile[NextIn(0, TW - 1), NextIn(0, TH - 1)], RandColor);
      else
      begin
        if NextIn(0, 1) = 0 then FlipTileH else FlipTileV;
      end;
    end;
    var probe := RandColor;
    HReset;
    HashTile;
    HInt(n);
    HInt(NearestPaletteIndex(probe));
    HInt(NearestPaletteColor(probe).ToArgb);
    HInt(IndexOfColor(probe));
    HInt(BoundPixelCount);
    gOut.Add('palop ' + IntToStr(k) + ' ' + HHex);
  end;
end;

procedure TestBuffers;
begin
  gSeed := 2024;
  for var k := 0 to 699 do
  begin
    InitCollections;
    ResetBrushState;
    RandomTile(NextIn(2, 24), NextIn(2, 24));
    HasSelection := true;
    SelX := NextIn(0, TW - 1);
    SelY := NextIn(0, TH - 1);
    SelW := NextIn(1, TW - SelX);
    SelH := NextIn(1, TH - SelY);
    CopySelectionToBuffer;
    var op := NextIn(0, 4);
    case op of
      0: BufferFlipH;
      1: BufferFlipV;
      2: BufferRotate90;
      else ;
    end;
    if NextIn(0, 1) = 0 then
      ApplyBufferToSelection
    else
      PasteBufferAt(NextIn(-8, TW + 2), NextIn(-8, TH + 2));
    HReset;
    HashTile;
    HInt(Length(Buffer, 0));
    HInt(Length(Buffer, 1));
    for var y := 0 to Length(Buffer, 1) - 1 do
      for var x := 0 to Length(Buffer, 0) - 1 do
        HInt(Buffer[x, y].ToArgb);
    gOut.Add('buffer ' + IntToStr(k) + ' ' + HHex);
  end;
end;

procedure TestView;
begin
  gSeed := 555;
  for var k := 0 to 999 do
  begin
    InitCollections;
    ResetBrushState;
    if NextIn(0, 3) = 0 then
    begin
      DocMode := DOC_MAP;
      MapNew(NextIn(4, 256), NextIn(4, 256));
    end
    else
      EmptyTile(NextIn(1, 512), NextIn(1, 512));
    W := CANVAS_W;
    H := CANVAS_H;
    FitScale;
    HReset;
    HInt(PixelSize); HInt(ViewX); HInt(ViewY);
    var steps := NextIn(1, 8);
    for var s := 1 to steps do
    begin
      var act := NextIn(0, 4);
      case act of
        0: ZoomAt(1, NextIn(0, W - 1), NextIn(0, H - 1));
        1: ZoomAt(-1, NextIn(0, W - 1), NextIn(0, H - 1));
        2: begin ViewX := ViewX + NextIn(-500, 500); ViewY := ViewY + NextIn(-500, 500); ClampView; end;
        3: OverviewCenterOn(NextIn(PANEL_X - 10, PANEL_X + PANEL_W + 10), NextIn(OVER_Y0 - 10, OVER_Y1 + 10));
        else FitScale;
      end;
      HInt(PixelSize); HInt(ViewX); HInt(ViewY);
    end;
    var sx := NextIn(-50, 650);
    var sy := NextIn(-50, 600);
    HInt(ScreenToTileX(sx)); HInt(ScreenToTileY(sy));
    HInt(TileToScreenX(sx)); HInt(TileToScreenY(sy));
    HInt(OverPixW); HInt(OverPixH); HInt(OverOriginX); HInt(OverOriginY);
    HInt(UndoDepth);
    GridOn := NextIn(0, 1) = 1;
    HInt(GridGap);
    // Рамка видимой области на обзоре — как в DrawOverviewSection.
    var ox := OverOriginX;
    var oy := OverOriginY;
    var pw := OverPixW;
    var ph := OverPixH;
    var vx0 := ox + Round((-ViewX / PixelSize) * pw / DocW);
    var vy0 := oy + Round((-ViewY / PixelSize) * ph / DocH);
    var vx1 := ox + Round(((-ViewX + W) / PixelSize) * pw / DocW);
    var vy1 := oy + Round(((-ViewY + H) / PixelSize) * ph / DocH);
    HInt(ClampI(vx0, ox, ox + pw)); HInt(ClampI(vy0, oy, oy + ph));
    HInt(ClampI(vx1, ox, ox + pw)); HInt(ClampI(vy1, oy, oy + ph));
    gOut.Add('view ' + IntToStr(k) + ' ' + HHex);
    DocMode := DOC_TILE;
  end;
end;

procedure TestRasters;
begin
  gSeed := 8080;
  for var k := 0 to 299 do
  begin
    InitCollections;
    ResetBrushState;
    if NextIn(0, 3) = 0 then
    begin
      DocMode := DOC_MAP;
      RandomMap(NextIn(4, 120), NextIn(4, 120));
    end
    else
      RandomTile(NextIn(1, 200), NextIn(1, 200));
    CheckerOn := NextIn(0, 1) = 1;
    HReset;
    // Обзор.
    OverDirty := true;
    BuildOverview;
    for var i := 0 to RawOver.Length - 1 do HByte(RawOver[i]);
    if DocMode = DOC_TILE then
    begin
      // Картинка холста — те же байты, что собирает EnsureTileBmp.
      for var i := 0 to TH - 1 do
        for var j := 0 to TW - 1 do
        begin
          var c := DisplayColor(j, i);
          HByte(c.B); HByte(c.G); HByte(c.R); HByte(c.A);
        end;
      // Миниатюры набора и значков.
      var size := NextIn(1, 64);
      var bmp := MakeThumb(Tile, size);
      var bmp2 := MakeThumbAlpha(Tile, size);
      // У значка сверяются альфа и цвет непрозрачных точек: libgdiplus
      // хранит картинку с домноженной альфой, и цвет полупрозрачной точки
      // возвращается из неё с потерей — это свойство Mono, а не оригинала.
      for var y := 0 to size - 1 do
        for var x := 0 to size - 1 do
        begin
          HInt(bmp.GetPixel(x, y).ToArgb);
          var c2 := bmp2.GetPixel(x, y);
          HByte(c2.A);
          if c2.A = 255 then HInt(c2.ToArgb);
        end;
      bmp.Dispose;
      bmp2.Dispose;
    end;
    gOut.Add('raster ' + IntToStr(k) + ' ' + HHex);
    DocMode := DOC_TILE;
  end;
end;

procedure TestColorsAndPick;
begin
  gSeed := 1234;
  for var k := 0 to 999 do
  begin
    InitCollections;
    ResetBrushState;
    RandomTile(NextIn(1, 16), NextIn(1, 16));
    FirstColor := RandColor;
    SecondColor := RandColor;
    FirstIdx := IndexOfColor(FirstColor);
    SecondIdx := NextIn(0, 63);
    Hue := NextIn(0, 359);
    var act := NextIn(0, 4);
    case act of
      0: PickColorFromTile(NextIn(-1, TW), NextIn(-1, TH));
      1: SwapColors;
      2: SyncHSVFromColor;
      3: begin Sat := NextIn(0, 100); Val := NextIn(0, 100); Hue := NextIn(0, 359); ApplyHSVToColor; end;
      else PickColor(RandColor);
    end;
    HReset;
    HInt(FirstColor.ToArgb); HInt(SecondColor.ToArgb);
    HInt(FirstIdx); HInt(SecondIdx);
    HInt(Hue); HInt(Sat); HInt(Val);
    gOut.Add('pick ' + IntToStr(k) + ' ' + HHex);
  end;
end;

procedure TestUndo;
begin
  // История: глубина, порядок снимков, обрезка сверху.
  InitCollections;
  ResetBrushState;
  gSeed := 4711;
  RandomTile(300, 300);
  ClearTileHistory;
  var limit := UndoDepth;
  gOut.Add('undoDepth ' + IntToStr(limit));
  for var s := 1 to limit + 5 do
  begin
    PushUndo;
    Tile[s mod TW, 0] := RGB(s, s, s);
  end;
  gOut.Add('undoCount ' + IntToStr(Length(UndoStack)));
  DoUndo; DoUndo; DoUndo;
  gOut.Add('afterUndo ' + IntToStr(Length(UndoStack)) + ' ' + IntToStr(Length(RedoStack)) + ' ' + IntToStr(Tile[0, 0].ToArgb) + ' ' + IntToStr(Tile[1, 0].ToArgb));
  DoRedo;
  gOut.Add('afterRedo ' + IntToStr(Length(UndoStack)) + ' ' + IntToStr(Length(RedoStack)) + ' ' + IntToStr(Tile[0, 0].ToArgb));
  PushUndo;
  gOut.Add('afterPush ' + IntToStr(Length(UndoStack)) + ' ' + IntToStr(Length(RedoStack)));
  for var sz := 1 to 600 do
    if sz mod 37 = 0 then
    begin
      TW := sz; TH := sz div 2 + 1;
      gOut.Add('depth ' + IntToStr(sz) + ' ' + IntToStr(UndoDepth));
    end;
end;

procedure TestMapCells;
begin
  MapNew(10, 6);
  MapGrid[3, 3] := MT_WATER;
  MapAddObject('npc', 1, 1, 'elder');
  MapAddObject('npc', 2, 1, '');
  MapAddObject('spawn', 3, 1, ' rat 2 4');
  MapAddObject('spawn', 4, 1, '   ');
  MapAddObject('chest', 5, 1, '0 - bread:1');
  MapAddObject('exit', 6, 1, 'forest 2 8');
  MapAddObject('npc', 6, 1, 'smith  extra');
  for var y := 0 to MapH - 1 do
    for var x := 0 to MapW - 1 do
      gOut.Add('cell ' + IntToStr(x) + ' ' + IntToStr(y) + ' [' + MapCellTileFile(x, y) + ']');
  gOut.Add('cell -1 0 [' + MapCellTileFile(-1, 0) + ']');
end;

procedure TestPaletteFile;
begin
  // LoadPaletteFrom: картинка любого размера растягивается до 8x8.
  InitCollections;
  var pic := new Picture(13, 5);
  for var y := 0 to 4 do
    for var x := 0 to 12 do
      pic.SetPixel(x, y, ARGB(255, x * 19, y * 50, (x * y * 11) mod 256));
  var path := System.IO.Path.Combine(ParamStr(1), '..', '..', 'build', 'golden', 'pal13x5.png');
  pic.Save(path);
  LoadPaletteFrom(path, true);
  for var i := 0 to PAL_ROWS - 1 do
  begin
    var s := 'palfile';
    for var j := 0 to PAL_COLS - 1 do s := s + ' ' + IntToStr(Palette[j, i].ToArgb);
    gOut.Add(s);
  end;
end;

begin
  W := CANVAS_W;
  H := CANVAS_H;
  SetLength(Buffer, 0, 0);
  SetLength(MapBuffer, 0, 0);
  TestDefaultPalette;
  TestShapes;
  TestMapShapes;
  TestFlood;
  TestPaletteOps;
  TestBuffers;
  TestView;
  TestRasters;
  TestColorsAndPick;
  TestUndo;
  TestMapCells;
  TestPaletteFile;
  System.IO.File.WriteAllLines(System.IO.Path.Combine(ParamStr(1), 'editor.txt'), gOut.ToArray,
                               new System.Text.UTF8Encoding(false));
  Halt(0);
end.
