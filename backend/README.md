# PocketBase Backend para PetBits

Este directorio contiene el backend de PocketBase para el juego.

## Setup Local

1. Descarga PocketBase para Windows:
```bash
# Visita: https://pocketbase.io/docs/
# O copia este comando:
Invoke-WebRequest -Uri "https://github.com/pocketbase/pocketbase/releases/download/v0.22.0/pocketbase_0.22.0_windows_amd64.zip" -OutFile "pocketbase.zip"
Expand-Archive -Path "pocketbase.zip" -DestinationPath "." -Force
Remove-Item "pocketbase.zip"
```

2. Ejecuta PocketBase:
```bash
.\pocketbase.exe serve
```

3. Accede al admin panel:
```
http://127.0.0.1:8090/_/
```

## Esquema de Datos

### Colección: `users` (built-in)
- email
- username
- emailVisibility
- verified

### Colección: `profiles`
- user (relation to users)
- coins (number, default: 100)
- totalPetsOwned (number, default: 0)

### Colección: `pets`
- user (relation to users)
- type (text: puppy, kitten, etc.)
- name (text)
- hunger (number, 0-100)
- xp (number)
- stage (number, 0-2)
- isActive (bool)
- createdAt (auto)
- lastFed (datetime)

### Colección: `inventory`
- user (relation to users)
- apple (number, default: 3)
- carrot (number, default: 2)
- meat (number, default: 1)
- cake (number, default: 0)
- star (number, default: 0)

## Deploy a Fly.io

```bash
# 1. Instalar Fly CLI
powershell -Command "iwr https://fly.io/install.ps1 -useb | iex"

# 2. Login
flyctl auth login

# 3. Lanzar app
flyctl launch

# 4. Deploy
flyctl deploy
```
