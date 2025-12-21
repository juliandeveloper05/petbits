# Script de Setup Automático de PocketBase
# Ejecuta este script desde PowerShell en la carpeta backend

Write-Host "🎮 PetBits - Setup de PocketBase" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Verificar si ya existe
if (Test-Path "pocketbase.exe") {
    Write-Host "✅ PocketBase ya está descargado." -ForegroundColor Green
    $response = Read-Host "¿Quieres ejecutarlo ahora? (s/n)"
    if ($response -eq "s") {
        Write-Host "🚀 Iniciando PocketBase..." -ForegroundColor Yellow
        Write-Host "Admin UI estará en: http://127.0.0.1:8090/_/" -ForegroundColor Cyan
        .\pocketbase.exe serve
    }
    exit
}

Write-Host "📥 Descargando PocketBase..." -ForegroundColor Yellow

try {
    $url = "https://github.com/pocketbase/pocketbase/releases/download/v0.22.0/pocketbase_0.22.0_windows_amd64.zip"
    $output = "pocketbase.zip"
    
    Invoke-WebRequest -Uri $url -OutFile $output
    
    Write-Host "📦 Extrayendo archivos..." -ForegroundColor Yellow
    Expand-Archive -Path $output -DestinationPath "." -Force
    Remove-Item $output
    
    Write-Host "✅ PocketBase descargado exitosamente!" -ForegroundColor Green
    Write-Host ""
    Write-Host "📝 Próximos pasos:" -ForegroundColor Cyan
    Write-Host "1. Ejecuta: .\pocketbase.exe serve" -ForegroundColor White
    Write-Host "2. Abre: http://127.0.0.1:8090/_/" -ForegroundColor White
    Write-Host "3. Configura las colecciones (ver SETUP_BACKEND.md)" -ForegroundColor White
    Write-Host ""
    
    $response = Read-Host "¿Quieres ejecutar PocketBase ahora? (s/n)"
    if ($response -eq "s") {
        Write-Host "🚀 Iniciando PocketBase..." -ForegroundColor Yellow
        Write-Host "Admin UI estará en: http://127.0.0.1:8090/_/" -ForegroundColor Cyan
        .\pocketbase.exe serve
    }
    
} catch {
    Write-Host "❌ Error al descargar PocketBase: $_" -ForegroundColor Red
    Write-Host "Por favor descarga manualmente desde:" -ForegroundColor Yellow
    Write-Host "https://pocketbase.io/docs/" -ForegroundColor Cyan
}
