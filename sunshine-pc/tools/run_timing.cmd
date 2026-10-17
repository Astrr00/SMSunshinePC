@echo off
rem Timing test: runs the game with the GPU; SUNSHINE_DEV_SKIP_MOVIES=1 skips all movies (logos, cutscenes) for convenience. Play/idle in Delfino ~2 min, then close the game.
cd /d "%~dp0"
set SUNSHINE_ALLOW_CPU_ADAPTER=
set AURORA_SCREENSHOT_EVERY=
set SUNSHINE_DEV_SKIP_MOVIES=1
sunshine.exe --dvd "%~dp0game.rvz" > timing_gpu.log 2>&1
echo.
echo === Result (send this to Claude) ===
findstr /C:"Device:" /C:"Backend:" timing_gpu.log
findstr /C:"[framerate]" timing_gpu.log > timing_framerate.txt
powershell -NoProfile -Command "$l=Get-Content timing_framerate.txt | Select -Skip 5; $n=0;$a=0;$p=0; foreach($x in $l){ if($x -match 'logic (\d+)/s, window presents (\d+)/s'){ $n++;$a+=[int]$Matches[1];$p+=[int]$Matches[2] } }; if($n){ 'samples',$n,'avg logic',[math]::Round($a/$n,1),'avg presents',[math]::Round($p/$n,1) -join ' ' } else { 'no framerate lines' }"
wmic cpu get name | findstr /V "Name"
wmic path win32_videocontroller get name | findstr /V "Name"
pause
