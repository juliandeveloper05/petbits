# tools/armar_teaser.ps1
#
# El teaser para historias de Instagram: la intro del juego explicada en ~30 s.
#
#   pwsh tools/armar_teaser.ps1
#   pwsh tools/armar_teaser.ps1 -Musica C:\ruta\al\tema.mp3
#   -> godot/teaser/teaser_historias.mp4   (1080x1920, 30 fps)
#
# ---
#
# TRES PASOS, Y NINGUNO A MANO.
#
# 1. Los subtitulos, dibujados con la tipografia del juego (teaser_textos.gd).
# 2. El juego, jugado por una coreografia y grabado con --write-movie
#    (teaser.gd). Anota en tiempos.json cuando empieza cada tramo.
# 3. ffmpeg los junta: el juego al DOBLE exacto con vecino mas cercano, y cada
#    subtitulo en el tramo que le toca segun tiempos.json. Si la intro cambia de
#    ritmo, los subtitulos se corren solos.
#
# Los primeros y ultimos 250 px quedan libres: Instagram los tapa con el avatar
# y la barra de responder.

param([string]$Musica = "")

$ErrorActionPreference = "Stop"

$raiz = Split-Path -Parent $PSScriptRoot
$proyecto = Join-Path $raiz "godot"
$carpeta = Join-Path $proyecto "teaser"
$avi = Join-Path $carpeta "teaser.avi"
$mp4 = Join-Path $carpeta "teaser_historias.mp4"

# Godot no tiene que importar esta carpeta: son salidas, no recursos del juego.
New-Item -ItemType Directory -Force $carpeta | Out-Null
Set-Content -Path (Join-Path $carpeta ".gdignore") -Value "" -NoNewline

Write-Host "`n--- 1. Subtitulos ---`n"
& godot --headless --path $proyecto --script res://scripts/teaser_textos.gd

Write-Host "`n--- 2. Grabando el juego ---`n"
if (Test-Path $avi) { Remove-Item $avi }
& godot --path $proyecto res://scenes/Teaser.tscn --write-movie $avi --fixed-fps 30 --disable-vsync
if (-not (Test-Path $avi)) { throw "No se grabo $avi" }

$t = Get-Content (Join-Path $carpeta "tiempos.json") -Raw | ConvertFrom-Json
$orden = @("titulo", "semilla", "cuidala", "pueblo", "mundo", "cierre")
$fin = [double]$t.fin

Write-Host "`n--- 3. Armando el vertical ---`n"

$entradas = @("-i", $avi)
foreach ($n in $orden) {
    $entradas += @("-loop", "1", "-framerate", "30", "-i", (Join-Path $carpeta "tramo_$n.png"))
}

# Los numeros con PUNTO, siempre. Con la configuracion regional en espanol,
# el formato de PowerShell los escribe con coma —4,500— y en un filtro de ffmpeg
# la coma separa filtros: el comando entero se romperia.
$inv = [System.Globalization.CultureInfo]::InvariantCulture
function N([double]$x) { return $x.ToString("0.000", $inv) }

$f = New-Object System.Collections.Generic.List[string]
# 60,700: la esquina de BANDA en teaser_textos.gd. Si se mueve una, la otra.
$f.Add("[0:v]scale=960:540:flags=neighbor,pad=1080:1920:60:700:color=0x0A0E0A[v0]")
for ($i = 0; $i -lt 5; $i++) {
    $a = N ([double]$t.($orden[$i]))
    $b = N ([double]$t.($orden[$i + 1]))
    $f.Add("[v$i][$($i + 1):v]overlay=0:0:enable='between(t,$a,$b)'[v$($i + 1)]")
}
$s = N ([double]$t.cierre)
$f.Add("[6:v]format=rgba,fade=t=in:st=${s}:d=0.6:alpha=1[cf]")
$f.Add("[v5][cf]overlay=0:0:enable='gte(t,$s)'[vout]")
$filtro = $f -join ";"

$salida = @("-map", "[vout]")
if ($Musica -ne "") {
    if (-not (Test-Path $Musica)) { throw "No esta el tema: $Musica" }
    $entradas += @("-i", $Musica)
    $salida_audio = N ($fin - 2)
    $filtro += ";[7:a]afade=t=in:d=1,afade=t=out:st=${salida_audio}:d=2[aout]"
    $salida += @("-map", "[aout]", "-c:a", "aac", "-b:a", "192k")
}

if (Test-Path $mp4) { Remove-Item $mp4 }
& ffmpeg -y -loglevel warning @entradas -filter_complex $filtro @salida -t (N $fin) `
    -c:v libx264 -pix_fmt yuv420p -crf 18 -r 30 $mp4

Write-Host "`n--- listo ---`n"
& ffprobe -v error -show_entries "format=duration:stream=width,height" -of default=nw=1 $mp4
Write-Host "`n  $mp4"
