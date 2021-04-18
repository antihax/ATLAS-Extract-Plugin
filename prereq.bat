mkdir DumpResources\steamcmd
mkdir json
mkdir DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins\DumpResources
copy DumpResources\x64\Release\DumpResources.dll DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins\DumpResources

copy DumpResources\DumpResources\configs\version.dll DumpResources\server\ShooterGame\Binaries\Win64\
copy DumpResources\DumpResources\configs\msdia140.dll DumpResources\server\ShooterGame\Binaries\Win64\
copy DumpResources\DumpResources\configs\config.json DumpResources\server\ShooterGame\Binaries\Win64\
copy DumpResources\DumpResources\configs\PdbConfig.json DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins\DumpResources

powershell -Command "(New-Object Net.WebClient).DownloadFile('https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip', 'DumpResources\steamcmd/steamcmd.zip')"
tar -xf DumpResources\steamcmd/steamcmd.zip -C DumpResources\steamcmd

DumpResources\steamcmd\steamcmd.exe +login anonymous +force_install_dir ..\server +app_update 1006030 +quit
