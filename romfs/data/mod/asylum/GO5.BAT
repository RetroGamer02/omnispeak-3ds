@echo off
if not exist fixmhead.exe goto just_run_it
fixmhead ck5mhead.obj maphead.ck5

on errorlevel 1 goto error
copy gamemaps.ck5 custmaps.ck5
:just_run_it
ck5patch patch5.pat -nowait
:error
