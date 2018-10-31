rd /s/q Release
md .\Release
copy .\Objects\*.hex .\Release\*.hex
copy .\Objects\*.bin .\Release\*.bin
copy .\Rev.txt .\Release\Rev.txt
del /s .\Objects\*.bin
start .\Tools\CheckFileSize.exe .\Release\CY2311_R2.bin 8192