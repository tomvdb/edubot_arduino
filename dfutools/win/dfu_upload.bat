@echo on

rem: Note %~dp0 get path of this batch file
rem: Need to change drive if My Documents is on a drive other than C:
set driverLetter=%~dp0
set driverLetter=%driverLetter:~0,2%
%driverLetter%
cd %~dp0
rem: the two line below are needed to fix path issues with incorrect slashes before the bin file name
set str1=%1
set str1=%str1:/=\%

set str2=%2
set str2=%str2:/=\%

dfu_commandline %str1% %str2%
dfusecommand -c -d --fn %str2%