@echo off

::touch user/user_main.c
set USRPATH=user\.output\eagle\debug
rm -f %USRPATH%\lib\libuser.a %USRPATH%\obj\user\user_main.*
echo.
echo start...
echo.
echo BOOT=none APP=0 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE=6
make BOOT=none APP=0 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE=6
::make BOOT=%boot% APP=%app% SPI_SPEED=%spi_speed% SPI_MODE=%spi_mode% SPI_SIZE=%spi_size_map%

