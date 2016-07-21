@echo off

::touch user/user_main.c
set USRPATH=user\.output\eagle\debug
::rm -f %USRPATH%\lib\libuser.a %USRPATH%\obj\user\user_main.*
echo.
echo start...
echo.
::echo BOOT=new APP=1 SPI_SPEED=40 SPI_MODE=QIO SPI_SIZE=6
set boot=new
set spi_speed=40
set spi_mode=QIO
set spi_size_map=6
set app=1
rm -f ../bin/upgrade/*
make clean
make BOOT=%boot% APP=%app% SPI_SPEED=%spi_speed% SPI_MODE=%spi_mode% SPI_SIZE=%spi_size_map%
make clean
::rm -f ../bin/upgrade/*.dump ../bin/upgrade/*.S
set app=2
make BOOT=%boot% APP=%app% SPI_SPEED=%spi_speed% SPI_MODE=%spi_mode% SPI_SIZE=%spi_size_map%
rename ..\bin\upgrade\user1.4096.new.%spi_size_map%.bin user1.bin
rename ..\bin\upgrade\user2.4096.new.%spi_size_map%.bin user2.bin
rm -f ../bin/inter/*
::mkdir -p ../bin/inter
mv ../bin/upgrade/*.dump ../bin/upgrade/*.S ../bin/inter
::make BOOT=%boot% APP=%app% SPI_SPEED=%spi_speed% SPI_MODE=%spi_mode% SPI_SIZE=%spi_size_map%
