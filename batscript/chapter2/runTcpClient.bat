@echo off

for /l %%i in (1,1,1) do (
	start cmd /c ..\..\Debug\TCPClient.exe
)