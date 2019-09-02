for /l %%x in (0, 1, 14) do (
	for /l %%y in (0, 1, 14) do (
		ShooterGame\Binaries\Win64\ShooterGameServer.exe Ocean?ServerX=%%x?ServerY=%%y?AltSaveDirectoryName=%%x%%y?ServerAdminPassword=123?QueryPort=5001?Port=5002?MapPlayerLocation=true -NoBattlEye -log -server -NoSeamlessServer 
	)
)
