///Цветовые преобразования и растровые буферы True Joint Tile Maker.
///Модуль не зависит от состояния приложения: здесь только математика цвета.
///Функции пересчёта координат в значения (AxisTo*/[]ToAxis) намеренно живут здесь же,
///чтобы отрисовка полос выбора цвета и обработка кликов по ним пользовались
///одной и той же формулой и не могли разъехаться.
unit TJTMColor;

interface

uses GraphABC, System.Drawing, System.Drawing.Imaging;

type
  ///Цвет в модели HSV. H = 0..359, S = 0..100, V = 0..100.
  HSVColor = record
    H, S, V: integer;
  end;

///Ограничивает значение диапазоном lo..hi.
function ClampI(v, lo, hi: integer): integer;
///Сравнивает цвета по значению ARGB.
///Штатное "=" для Color сравнивает ещё и признак именованного цвета,
///поэтому clWhite и RGB(255, 255, 255) считаются разными. Здесь этого не происходит.
function SameColor(a, b: Color): boolean;
///HSV -> RGB. h приводится к 0..359, s и v ограничиваются 0..100. Альфа = 255.
function HSVtoRGB(h, s, v: integer): Color;
///RGB -> HSV. Возвращаемый H всегда лежит в 0..359.
function RGBtoHSV(r, g, b: integer): HSVColor;
///RGB -> HSV для готового цвета.
function ColorToHSV(c: Color): HSVColor;
///Наложение src поверх dst по правилу source-over с учётом альфы обоих цветов.
function BlendOver(src, dst: Color): Color;
///Создаёт Bitmap из массива байтов формата BGRA (4 байта на пиксель).
function BytesToImage(bytes: array of byte; Width: integer): Bitmap;

///Координата вдоль оси (0..size-1) -> процент 0..100.
function AxisToPercent(pos, size: integer): integer;
///Процент 0..100 -> координата вдоль оси (0..size-1).
function PercentToAxis(percent, size: integer): integer;
///Координата по вертикали -> яркость: вверху ярко, внизу темно.
function AxisToValue(pos, size: integer): integer;
///Яркость -> координата по вертикали.
function ValueToAxis(v, size: integer): integer;
///Координата вдоль полосы тонов -> тон 0..359.
function AxisToHue(pos, size: integer): integer;
///Тон 0..359 -> координата вдоль полосы тонов.
function HueToAxis(h, size: integer): integer;
///Координата вдоль полосы прозрачности -> значение 0..255.
function AxisToByte(pos, size: integer): integer;
///Значение 0..255 -> координата вдоль полосы прозрачности.
function ByteToAxis(v, size: integer): integer;

///Заполняет буфер квадратом "насыщенность x яркость" для заданного тона.
procedure RenderHSVPlane(var bytes: array of byte; Width, Tone: integer);
///Заполняет буфер горизонтальной полосой тонов 0..359.
procedure RenderHuePlane(var bytes: array of byte; Width: integer);
///Заполняет буфер горизонтальным градиентом прозрачности.
procedure RenderAlphaPlane(var bytes: array of byte; Width: integer);

implementation

function ClampI(v, lo, hi: integer): integer;
begin
  if v < lo then Result := lo
  else if v > hi then Result := hi
  else Result := v;
end;

function SameColor(a, b: Color): boolean;
begin
  Result := a.ToArgb = b.ToArgb;
end;

function HSVtoRGB(h, s, v: integer): Color;
var
  r, g, b, vm, vi, vd, a: real;
  sector: integer;
begin
  h := h mod 360;
  if h < 0 then h := h + 360;
  s := ClampI(s, 0, 100);
  v := ClampI(v, 0, 100);

  vm := (100 - s) * v / 100;
  a := (v - vm) * ((h mod 60) / 60);
  vi := vm + a;
  vd := v - a;

  sector := h div 60;
  case sector of
    0: begin r := v;  g := vi; b := vm end;
    1: begin r := vd; g := v;  b := vm end;
    2: begin r := vm; g := v;  b := vi end;
    3: begin r := vm; g := vd; b := v  end;
    4: begin r := vi; g := vm; b := v  end;
    else begin r := v;  g := vm; b := vd end;
  end;

  Result := RGB(ClampI(Round(r * 2.55), 0, 255),
                ClampI(Round(g * 2.55), 0, 255),
                ClampI(Round(b * 2.55), 0, 255));
end;

function RGBtoHSV(r, g, b: integer): HSVColor;
var
  mn, mx, delta, h: integer;
begin
  r := ClampI(r, 0, 255);
  g := ClampI(g, 0, 255);
  b := ClampI(b, 0, 255);

  mn := r;
  if g < mn then mn := g;
  if b < mn then mn := b;
  mx := r;
  if g > mx then mx := g;
  if b > mx then mx := b;
  delta := mx - mn;

  if delta = 0 then
    h := 0
  else if mx = r then
    h := Round(60 * (g - b) / delta)
  else if mx = g then
    h := Round(60 * (b - r) / delta) + 120
  else
    h := Round(60 * (r - g) / delta) + 240;

  h := h mod 360;
  if h < 0 then h := h + 360;

  Result.H := h;
  if mx = 0 then Result.S := 0 else Result.S := ClampI(Round(delta * 100 / mx), 0, 100);
  Result.V := ClampI(Round(mx * 100 / 255), 0, 100);
end;

function ColorToHSV(c: Color): HSVColor;
begin
  Result := RGBtoHSV(c.R, c.G, c.B);
end;

function BlendOver(src, dst: Color): Color;
var
  sa, da, oa: real;
begin
  if src.A >= 255 then
  begin
    Result := src;
    exit;
  end;
  if src.A <= 0 then
  begin
    Result := dst;
    exit;
  end;

  sa := src.A / 255;
  da := dst.A / 255;
  oa := sa + da * (1 - sa);
  if oa <= 0 then
    Result := ARGB(0, 0, 0, 0)
  else
    Result := ARGB(ClampI(Round(oa * 255), 0, 255),
                   ClampI(Round((src.R * sa + dst.R * da * (1 - sa)) / oa), 0, 255),
                   ClampI(Round((src.G * sa + dst.G * da * (1 - sa)) / oa), 0, 255),
                   ClampI(Round((src.B * sa + dst.B * da * (1 - sa)) / oa), 0, 255));
end;

function BytesToImage(bytes: array of byte; Width: integer): Bitmap;
var
  Height: integer;
begin
  Result := nil;
  if (bytes = nil) or (Width <= 0) then exit;
  Height := (bytes.Length div 4) div Width;
  if Height <= 0 then exit;
  Result := new Bitmap(Width, Height);
  var rect := new Rectangle(0, 0, Width, Height);
  var bmData := Result.LockBits(rect, ImageLockMode.WriteOnly, Result.PixelFormat);
  System.Runtime.InteropServices.Marshal.Copy(bytes, 0, bmData.Scan0, Width * Height * 4);
  Result.UnlockBits(bmData);
end;

function AxisToPercent(pos, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(pos * 100 / (size - 1)), 0, 100);
end;

function PercentToAxis(percent, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(ClampI(percent, 0, 100) * (size - 1) / 100), 0, size - 1);
end;

function AxisToValue(pos, size: integer): integer;
begin
  Result := 100 - AxisToPercent(pos, size);
end;

function ValueToAxis(v, size: integer): integer;
begin
  Result := PercentToAxis(100 - ClampI(v, 0, 100), size);
end;

function AxisToHue(pos, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(pos * 359 / (size - 1)), 0, 359);
end;

function HueToAxis(h, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(ClampI(h, 0, 359) * (size - 1) / 359), 0, size - 1);
end;

function AxisToByte(pos, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(pos * 255 / (size - 1)), 0, 255);
end;

function ByteToAxis(v, size: integer): integer;
begin
  if size <= 1 then Result := 0
  else Result := ClampI(Round(ClampI(v, 0, 255) * (size - 1) / 255), 0, size - 1);
end;

///Общий каркас для всех трёх полос: перебирает пиксели буфера и просит цвет у callback.
///Вынесен отдельно, чтобы не дублировать раскладку BGRA три раза.
procedure FillPlane(var bytes: array of byte; Width: integer; kind, Tone: integer);
var
  Height, i: integer;
  c: Color;
begin
  if (bytes = nil) or (Width <= 0) then exit;
  Height := (bytes.Length div 4) div Width;
  for var y := 0 to Height - 1 do
    for var x := 0 to Width - 1 do
    begin
      case kind of
        0: c := HSVtoRGB(Tone, AxisToPercent(x, Width), AxisToValue(y, Height));
        1: c := HSVtoRGB(AxisToHue(x, Width), 100, 100);
        else c := RGB(255 - AxisToByte(x, Width), 255 - AxisToByte(x, Width), 255 - AxisToByte(x, Width));
      end;
      i := (y * Width + x) * 4;
      bytes[i + 0] := c.B;
      bytes[i + 1] := c.G;
      bytes[i + 2] := c.R;
      bytes[i + 3] := 255;
    end;
end;

procedure RenderHSVPlane(var bytes: array of byte; Width, Tone: integer);
begin
  FillPlane(bytes, Width, 0, Tone);
end;

procedure RenderHuePlane(var bytes: array of byte; Width: integer);
begin
  FillPlane(bytes, Width, 1, 0);
end;

procedure RenderAlphaPlane(var bytes: array of byte; Width: integer);
begin
  FillPlane(bytes, Width, 2, 0);
end;

end.
