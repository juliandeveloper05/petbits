# tools/armar_video.ps1
#
# Graba treinta segundos de juego y los arma en un video vertical para Stories.
#
#   pwsh tools/armar_video.ps1
#   → godot/demo.avi          el crudo, 480x270
#   → godot/demo_stories.mp4  1080x1920, listo para subir
#
# ---
#
# POR QUE UN SCRIPT Y NO UN EDITOR.
#
# El mundo cambia. Cuando cambia —como cambio el dia que el mineral paso a
# existir cerca del pueblo— este comando vuelve a producir el video, con la misma
# coreografia y el mismo encuadre. Con un montaje hecho a mano habria que
# rehacerlo, y a la tercera vez ya no se rehace.
#
# ---
#
# LAS DOS DECISIONES QUE IMPORTAN.
#
# `--fixed-fps 30`: el tiempo del juego avanza a pasos fijos sin importar cuanto
# tarde cada cuadro de verdad. La grabacion dura exactamente los cuadros que pide
# demo.gd y sale igual en cualquier maquina.
#
# `scale=960:540:flags=neighbor`: el viewport es 480x270 y se agranda al DOBLE
# exacto. Con vecino mas cercano, cero interpolacion. A 2,25 —lo que haria falta
# para llenar los 1080 de ancho— el pixel art se convierte en un borron.

$ErrorActionPreference = "Stop"

$raiz = Split-Path -Parent $PSScriptRoot
$avi = Join-Path $raiz "godot\demo.avi"
$mp4 = Join-Path $raiz "godot\demo_stories.mp4"

# La tipografia del titulo. Press Start 2P es la de ocho bits de siempre; si no
# esta, se cae a Consolas, que no queda igual pero no rompe el comando.
$fuente = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts\PressStart2P-Regular.ttf"
if (-not (Test-Path $fuente)) { $fuente = "C:\Windows\Fonts\consola.ttf" }
# ffmpeg quiere las barras al reves y los dos puntos escapados.
$f = ($fuente -replace '\\', '/') -replace ':', '\:'

Write-Host "`n--- 1. Grabando el juego ---`n"
if (Test-Path $avi) { Remove-Item $avi }
& godot --path (Join-Path $raiz "godot") res://scenes/Demo.tscn `
    --write-movie $avi --fixed-fps 30 --disable-vsync

if (-not (Test-Path $avi)) { throw "No se genero $avi" }

Write-Host "`n--- 2. Armando el vertical ---`n"

# El juego al doble, centrado en 1080x1920 con el negro del propio juego.
# Los primeros y ultimos 250 px los tapa Instagram con su interfaz, asi que todo
# lo que importa vive entre y=250 e y=1670.
$verde = "0x6BFF7A"   # el verde fosforo con el que el juego dibuja la semilla
$apagado = "0x8FA882" # el mismo, bajado, para que el pie no compita con el titulo

$vf = @(
    "scale=960:540:flags=neighbor"
    "pad=1080:1920:60:690:0x0A0E0A"
    "drawtext=fontfile='${f}':text='PetBits':fontcolor=${verde}:fontsize=72:x=(w-text_w)/2:y=470"
    "drawtext=fontfile='${f}':text='cada criatura es':fontcolor=${apagado}:fontsize=26:x=(w-text_w)/2:y=1330"
    "drawtext=fontfile='${f}':text='una semilla de 64 bits':fontcolor=${apagado}:fontsize=26:x=(w-text_w)/2:y=1380"
) -join ","

if (Test-Path $mp4) { Remove-Item $mp4 }
& ffmpeg -y -loglevel warning -i $avi -vf $vf `
    -c:v libx264 -pix_fmt yuv420p -crf 18 -r 30 $mp4

Write-Host "`n--- listo ---`n"
& ffprobe -v error -show_entries "format=duration:stream=width,height" -of default=nw=1 $mp4
Write-Host "`n  $mp4"
Write-Host "`nPara ponerle musica, cuando tengas el archivo:"
Write-Host "  ffmpeg -i demo_stories.mp4 -i tema.mp3 -filter_complex ""[1:a]afade=t=in:d=1,afade=t=out:st=28:d=2[a]"" -map 0:v -map ""[a]"" -shortest -c:v copy demo_con_musica.mp4"
