@echo off
md %1
set binfiles=asl.exe plist.exe pbind.exe p2hex.exe p2bin.exe
for %%i in (%binfiles%) do tdstrip %%i
for %%i in (%binfiles%) do copy %%i %1
ren %1\asl.exe as.exe
set binfiles=
copy *.msg %1 

md %2
for %%i in (include\*.inc) do copy %%i %2
for %%i in (%2\*.inc) do unumlaut %%i

md %3
for %%i in (*.1) do copy %%i %3

md %4
for %%i in (*.msg) do copy %%i %1

md %5
set docdirs=DE EN
for %%i in (%docdirs%) do unumlaut doc_%%i\as.doc %5\as_%%i.doc
for %%i in (%docdirs%) do unumlaut doc_%%i\as.tex %5\as_%%i.tex
