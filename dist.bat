mkdir dist
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\version.dll DumpResources\DumpResources\configs\
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\msdia140.dll DumpResources\DumpResources\configs\
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\config.json DumpResources\DumpResources\configs\
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins\DumpResources\PdbConfig.json DumpResources\DumpResources\configs\

xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\AtlasApi\Plugins dist\ /E
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\version.dll dist\
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\msdia140.dll dist\
xcopy /y DumpResources\server\ShooterGame\Binaries\Win64\config.json dist\
