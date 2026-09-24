# tools/exportar.ps1
#
# Arma el .zip que se le pasa a un tester.
#
#   pwsh tools/exportar.ps1
#   -> godot/exports/PetBits-prueba.zip
#
# ---
#
# EXPORTA EN MODO DEBUG, A PROPOSITO.
#
# Dos razones. Una: la DLL de la GDExtension que ya existe es la de debug, y la
# de release obliga a recompilar godot-cpp entero, que es largo. Dos: en un build
# de prueba conviene que los errores queden a la vista — el .zip trae un
# PetBits.console.exe que los muestra.
#
# Y de paso es el candado de la tecla de tester: F8 solo hace algo cuando
# OS.is_debug_build() es verdadero, asi que en un export de release no existe.
#
# ---
#
# EL .PCK VA AL LADO DEL .EXE, NO ADENTRO.
#
# Embebido es lo que mas dispara falsos positivos de antivirus en los juegos de
# Godot. Y la DLL tiene que estar al lado del .exe si o si: por eso el LEEME
# insiste en descomprimir antes de abrir.

$ErrorActionPreference = "Stop"

$raiz = Split-Path -Parent $PSScriptRoot
$proyecto = Join-Path $raiz "godot"
$exports = Join-Path $proyecto "exports"
$carpeta = Join-Path $exports "PetBits"
$zip = Join-Path $exports "PetBits-prueba.zip"

# Godot no tiene que escanear la carpeta de salida como parte del proyecto: sin
# esto, el export siguiente veria el .pck anterior como un recurso mas.
New-Item -ItemType Directory -Force $exports | Out-Null
Set-Content -Path (Join-Path $exports ".gdignore") -Value "" -NoNewline

if (Test-Path $carpeta) { Remove-Item -Recurse -Force $carpeta }
New-Item -ItemType Directory -Force $carpeta | Out-Null
if (Test-Path $zip) { Remove-Item -Force $zip }

Write-Host "`n--- 1. Importando recursos ---`n"
& godot --headless --path $proyecto --import 2>&1 | Out-Null

Write-Host "--- 2. Exportando ---`n"
& godot --headless --path $proyecto --export-debug "Windows Desktop" (Join-Path $carpeta "PetBits.exe")

# Lo que TIENE que estar para que ande en otra maquina.
$necesarios = @(
    "PetBits.exe",
    "PetBits.pck",
    "libpetbits_core.windows.template_debug.x86_64.dll"
)
foreach ($n in $necesarios) {
    if (-not (Test-Path (Join-Path $carpeta $n))) {
        throw "Falta $n en la exportacion. El .zip no andaria."
    }
}

# Los .exp y .lib que deja MSVC al lado de la DLL no son la biblioteca: son las
# tablas de exportacion. Al tester no le sirven.
Get-ChildItem $carpeta -Include *.exp, *.lib -Recurse | Remove-Item -Force

Copy-Item (Join-Path $PSScriptRoot "LEEME-prueba.txt") (Join-Path $carpeta "LEEME.txt")

Write-Host "--- 3. Armando el .zip ---`n"
Compress-Archive -Path (Join-Path $carpeta "*") -DestinationPath $zip -CompressionLevel Optimal

Get-ChildItem $carpeta | ForEach-Object { "  {0,-52} {1,10:N0} KB" -f $_.Name, ($_.Length / 1KB) }
""
"  {0}" -f $zip
"  {0:N1} MB" -f ((Get-Item $zip).Length / 1MB)
