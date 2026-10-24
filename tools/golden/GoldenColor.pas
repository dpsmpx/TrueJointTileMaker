// Эталон для цвета: прогоняет функции TJTMColor оригинала по полным
// перебором и пишет хэши FNV-1a с выборкой значений. Тесты переноса
// считают те же хэши и сравнивают.
uses TJTMColor, GraphABC;

var
  h: int64;
  outf: Text;

procedure HReset; begin h := 2166136261; end;
procedure HByte(b: integer); begin h := ((h xor (b and 255)) * 16777619) and $FFFFFFFF; end;
procedure HInt(v: integer);
begin
  HByte(v); HByte(v shr 8); HByte(v shr 16); HByte(v shr 24);
end;
function HHex: string; begin Result := h.ToString('x8'); end;

begin
  Assign(outf, ParamStr(1));
  Rewrite(outf);

  // HSV -> RGB, с выходом за диапазоны: h приводится по модулю, s и v
  // ограничиваются.
  HReset;
  for var hh := -360 to 719 do
    for var s := -5 to 105 do
      for var v := -5 to 105 do
      begin
        var c := HSVtoRGB(hh, s, v);
        HByte(c.R); HByte(c.G); HByte(c.B); HByte(c.A);
        if (hh * 7 + s * 13 + v * 31) mod 997 = 0 then
          Writeln(outf, 'hsv ', hh, ' ', s, ' ', v, ' ', c.R, ' ', c.G, ' ', c.B);
      end;
  Writeln(outf, 'hash hsvToRgb ', HHex);

  HReset;
  for var r := 0 to 255 do
    for var g := 0 to 255 do
      for var b := 0 to 255 do
      begin
        var x := RGBtoHSV(r, g, b);
        HInt(x.H); HByte(x.S); HByte(x.V);
        if (r * 3 + g * 5 + b * 7) mod 4999 = 0 then
          Writeln(outf, 'rgb ', r, ' ', g, ' ', b, ' ', x.H, ' ', x.S, ' ', x.V);
      end;
  Writeln(outf, 'hash rgbToHsv ', HHex);

  // Смешивание: все пары альф, каналы — из простого генератора, чтобы
  // покрыть и тёмное, и светлое.
  HReset;
  var seed: int64 := 12345;
  for var sa := 0 to 255 do
    for var da := 0 to 255 do
    begin
      seed := (seed * 1103515245 + 12345) and $7FFFFFFF;
      var src := ARGB(sa, seed and 255, (seed shr 8) and 255, (seed shr 16) and 255);
      seed := (seed * 1103515245 + 12345) and $7FFFFFFF;
      var dst := ARGB(da, seed and 255, (seed shr 8) and 255, (seed shr 16) and 255);
      var c := BlendOver(src, dst);
      HByte(c.A); HByte(c.R); HByte(c.G); HByte(c.B);
      if (sa * 256 + da) mod 251 = 0 then
        Writeln(outf, 'blend ', src.ToArgb, ' ', dst.ToArgb, ' ', c.ToArgb);
    end;
  Writeln(outf, 'hash blendOver ', HHex);

  HReset;
  for var size := -1 to 300 do
    for var pos := -10 to 310 do
    begin
      HInt(AxisToPercent(pos, size));
      HInt(PercentToAxis(pos, size));
      HInt(AxisToValue(pos, size));
      HInt(ValueToAxis(pos, size));
      HInt(AxisToHue(pos, size));
      HInt(HueToAxis(pos, size));
      HInt(AxisToByte(pos, size));
      HInt(ByteToAxis(pos, size));
    end;
  Writeln(outf, 'hash axis ', HHex);

  // Полосы выбора цвета в размерах панели: квадрат 100x100 для каждого
  // тона, полосы тона и прозрачности 100x12.
  HReset;
  var raw: array of byte;
  SetLength(raw, 100 * 100 * 4);
  for var tone := 0 to 359 do
  begin
    RenderHSVPlane(raw, 100, tone);
    for var i := 0 to raw.Length - 1 do HByte(raw[i]);
  end;
  SetLength(raw, 100 * 12 * 4);
  RenderHuePlane(raw, 100);
  for var i := 0 to raw.Length - 1 do HByte(raw[i]);
  RenderAlphaPlane(raw, 100);
  for var i := 0 to raw.Length - 1 do HByte(raw[i]);
  Writeln(outf, 'hash planes ', HHex);

  Close(outf);
  Halt(0);
end.
