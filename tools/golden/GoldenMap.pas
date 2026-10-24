// Эталон для карт: каждый файл из tests/golden/map_in читается TJTMMap
// оригинала; выписываются итог разбора, сетка, объекты, замечания проверки
// и то, что запишет MapSave. Затем — правки размера и объектов.
uses TJTMMap;

var
  outl: List<string> := new List<string>;
  tmpFile: string;

procedure DumpMap;
begin
  outl.Add('name [' + MapName + ']');
  outl.Add('size ' + IntToStr(MapW) + ' ' + IntToStr(MapH));
  if (Length(MapGrid, 0) <> MapW) or (Length(MapGrid, 1) <> MapH) then
  begin
    // Оригинал оставляет сетку и заявленный размер разными: дальше
    // программа упала бы на первом обращении к клетке.
    outl.Add('inconsistent ' + IntToStr(Length(MapGrid, 0)) + ' ' + IntToStr(Length(MapGrid, 1)));
    exit;
  end;
  for var y := 0 to MapH - 1 do
  begin
    var row := '';
    for var x := 0 to MapW - 1 do row := row + MapTileChar(MapGrid[x, y]);
    outl.Add('row ' + row);
  end;
  for var k := 0 to Length(MapObjects) - 1 do
    outl.Add('obj [' + MapObjectLine(MapObjects[k]) + '] kind=[' + MapObjects[k].Kind + '] rest=[' + MapObjects[k].Rest + ']');
  var v := MapValidate;
  foreach var line in v.Split(new string[](NewLine), System.StringSplitOptions.RemoveEmptyEntries) do
    outl.Add('valid [' + line + ']');
  MapSave(tmpFile);
  var bytes := System.IO.File.ReadAllBytes(tmpFile);
  var sb := new System.Text.StringBuilder;
  foreach var b in bytes do sb.Append(b.ToString('x2'));
  outl.Add('saved ' + sb.ToString);
end;

begin
  var inDir := ParamStr(1);
  var outDir := ParamStr(2);
  tmpFile := System.IO.Path.Combine(outDir, 'tmp.map');
  var files := System.IO.Directory.GetFiles(inDir, '*.map');
  System.Array.Sort(files, System.StringComparer.Ordinal);
  foreach var f in files do
  begin
    outl.Add('file ' + System.IO.Path.GetFileName(f));
    var ok := MapLoad(f);
    outl.Add('ok ' + IntToStr(Ord(ok)));
    outl.Add('error [' + MapError + ']');
    outl.Add('warning [' + MapWarning + ']');
    if ok then DumpMap;
  end;

  // Правки: размер вниз и вверх, добавление и удаление объектов.
  MapLoad(System.IO.Path.Combine(inDir, '11_validate.map'));
  outl.Add('file resize-3x3');
  MapResize(3, 3);
  DumpMap;
  outl.Add('file resize-8x6');
  MapResize(8, 6);
  DumpMap;
  outl.Add('file objects');
  MapAddObject('bed', 2, 2, '');
  MapAddObject('npc', 2, 2, 'elder');
  outl.Add('objectAt ' + IntToStr(MapObjectAt(2, 2)) + ' ' + IntToStr(MapObjectAt(0, 0)) + ' ' + IntToStr(MapObjectAt(7, 5)));
  MapDeleteObject(0);
  MapDeleteObject(-1);
  MapDeleteObject(100);
  DumpMap;
  outl.Add('file new-2x300');
  MapNew(2, 300);
  DumpMap;

  System.IO.File.Delete(tmpFile);
  System.IO.File.WriteAllLines(System.IO.Path.Combine(outDir, 'map.txt'), outl.ToArray,
                               new System.Text.UTF8Encoding(false));
end.
