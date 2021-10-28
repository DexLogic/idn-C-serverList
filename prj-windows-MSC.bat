if not exist "bin-windows" mkdir bin-windows
cl src/main.c src/idnServerList.c src/plt-windows.c Ws2_32.lib /Fobin-windows\ /Febin-windows/serverList.exe
