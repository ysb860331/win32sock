@echo off

for /l %%i in (1,1,1000) do (
	call ..\..\Debug\UDPClient.exe
)

pause