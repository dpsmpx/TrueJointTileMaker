// Эталон для настроек: каждый файл из tests/golden/config_in читается
// TJTMConfig оригинала, значения выписываются; затем пишутся настройки
// по умолчанию и прочитанные из смешанного файла.
uses TJTMConfig;

var
  outl: List<string> := new List<string>;

procedure Dump(name: string; known: boolean);
begin
  outl.Add('file ' + name);
  outl.Add('known ' + IntToStr(Ord(known)));
  outl.Add('showHelpOnStart ' + IntToStr(Ord(CfgShowHelpOnStart)));
  outl.Add('showMenuOnStart ' + IntToStr(Ord(CfgShowMenuOnStart)));
  outl.Add('defaultWidth ' + IntToStr(CfgDefaultWidth));
  outl.Add('defaultHeight ' + IntToStr(CfgDefaultHeight));
  outl.Add('showGrid ' + IntToStr(Ord(CfgShowGrid)));
  outl.Add('showChecker ' + IntToStr(Ord(CfgShowChecker)));
  outl.Add('startupImage [' + CfgStartupImage + ']');
  outl.Add('autoLoadLast ' + IntToStr(Ord(CfgAutoLoadLast)));
  outl.Add('lastFile [' + CfgLastFile + ']');
  outl.Add('lastFolder [' + CfgLastFolder + ']');
  outl.Add('paletteFile [' + CfgPaletteFile + ']');
  outl.Add('showSeamless ' + IntToStr(Ord(CfgShowSeamless)));
  outl.Add('startInMapMode ' + IntToStr(Ord(CfgStartInMapMode)));
  outl.Add('defaultMapWidth ' + IntToStr(CfgDefaultMapWidth));
  outl.Add('defaultMapHeight ' + IntToStr(CfgDefaultMapHeight));
  outl.Add('mapsFolder [' + CfgMapsFolder + ']');
  outl.Add('lastMapFile [' + CfgLastMapFile + ']');
  outl.Add('tilesFolder [' + CfgTilesFolder + ']');
  outl.Add('iconsFolder [' + CfgIconsFolder + ']');
end;

begin
  var inDir := ParamStr(1);
  var outDir := ParamStr(2);
  var files := System.IO.Directory.GetFiles(inDir, '*.txt');
  System.Array.Sort(files, System.StringComparer.Ordinal);
  foreach var f in files do
  begin
    var known := ConfigLoad(f);
    Dump(System.IO.Path.GetFileName(f), known);
  end;
  var missing := ConfigLoad(System.IO.Path.Combine(inDir, 'нет-такого-файла.txt'));
  Dump('missing', missing);

  ConfigSetDefaults;
  ConfigSave(System.IO.Path.Combine(outDir, 'config_saved_default.txt'));
  ConfigLoad(System.IO.Path.Combine(inDir, '02_mixed.txt'));
  ConfigSave(System.IO.Path.Combine(outDir, 'config_saved_mixed.txt'));

  System.IO.File.WriteAllLines(System.IO.Path.Combine(outDir, 'config.txt'), outl.ToArray,
                               new System.Text.UTF8Encoding(false));
end.
