mkdir dist
copy DumpResources\server\ShooterGame\Binaries\Win64\version.dll DumpResources\DumpResources\configs\
copy DumpResources\server\ShooterGame\Binaries\Win64\msdia140.dll DumpResources\DumpResources\configs\
copy DumpResources\server\ShooterGame\Binaries\Win64\config.json DumpResources\DumpResources\configs\
copy DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins\DumpResources\PdbConfig.json DumpResources\DumpResources\configs\

xcopy DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins dist\ /E
