# 🎮 PetBits con Backend - Guía de Setup

## ✅ Lo que ya está preparado

1. **Frontend actualizado**:
   - ✅ Pantalla de autenticación (login/registro) agregada
   - ✅ PocketBase SDK integrado via CDN
   - ✅ Cliente de PocketBase (`pb-client.js`) creado
   - ✅ Estilos CSS para autenticación

2. **Backend configurado**:
   - ✅ Carpeta `backend/` creada
   - ✅ README con instrucciones
   - ✅ `fly.toml` para deployment

## 📝 Próximos Pasos (en orden)

### Paso 1: Descargar PocketBase

```powershell
# Ve a la carpeta backend
cd backend

# Descarga PocketBase para Windows
Invoke-WebRequest -Uri "https://github.com/pocketbase/pocketbase/releases/download/v0.22.0/pocketbase_0.22.0_windows_amd64.zip" -OutFile "pocketbase.zip"
Expand-Archive -Path "pocketbase.zip" -DestinationPath "." -Force
Remove-Item "pocketbase.zip"
```

### Paso 2: Ejecutar PocketBase localmente

```powershell
# Desde la carpeta backend
.\pocketbase.exe serve
```

Esto iniciará PocketBase en `http://127.0.0.1:8090`

### Paso 3: Configurar la Base de Datos

1. Abre en tu navegador: `http://127.0.0.1:8090/_/`
2. Crea una cuenta de admin (esta es solo para ti, no es una cuenta de juego)
3. Crea las siguientes colecciones:

#### **Colección: `profiles`**
- Campos:
  - `user` (Relation → users, Single)
  - `coins` (Number, default: 100)
  - `totalPetsOwned` (Number, default: 0)
- API Rules:
  - List/View: `@request.auth.id != "" && user = @request.auth.id`
  - Create: `@request.auth.id != ""`
  - Update: `@request.auth.id != "" && user = @request.auth.id`
  - Delete: Ninguno

#### **Colección: `pets`**
- Campos:
  - `user` (Relation → users, Single)
  - `type` (Text)
  - `name` (Text)
  - `hunger` (Number, min: 0, max: 100)
  - `xp` (Number, min: 0)
  - `stage` (Number, min: 0, max: 2)
  - `isActive` (Bool, default: false)
  - `lastFed` (Date)
- API Rules:
  - List/View: `@request.auth.id != "" && user = @request.auth.id`
  - Create: `@request.auth.id != ""`
  - Update: `@request.auth.id != "" && user = @request.auth.id`
  - Delete: `@request.auth.id != "" && user = @request.auth.id`

#### **Colección: `inventory`**
- Campos:
  - `user` (Relation → users, Single)
  - `apple` (Number, default: 3)
  - `carrot` (Number, default: 2)
  - `meat` (Number, default: 1)
  - `cake` (Number, default: 0)
  - `star` (Number, default: 0)
- API Rules:
  - List/View: `@request.auth.id != "" && user = @request.auth.id`
  - Create: `@request.auth.id != ""`
  - Update: `@request.auth.id != "" && user = @request.auth.id`
  - Delete: Ninguno

### Paso 4: Actualizar main.js

Necesitas modificar `main.js` para integrar con PocketBase. Las partes clave a cambiar:

```javascript
// En lugar de:
let gameState = {
    coins: 100,
    // ...
};

// Ahora usarás:
let gameState = {
    profile: null,
    currentPet: null,
    inventory: null
};

// Y cargarás datos del backend:
async function loadGameData() {
    const profile = await PBGame.getProfile();
    const pet = await PBGame.getActivePet();
    const inventory = await PBGame.getInventory();
    
    gameState.profile = profile;
    gameState.currentPet = pet;
    gameState.inventory = inventory;
}
```

### Paso 5: Implementar Autenticación en main.js

Agrega al principio de tu código:

```javascript
// Auth Screen Logic
const authScreen = document.getElementById('auth-screen');
const loginForm = document.getElementById('login-form');
const registerForm = document.getElementById('register-form');
const authTabs = document.querySelectorAll('.auth-tab-btn');

// Tab switching
authTabs.forEach(tab => {
    tab.addEventListener('click', () => {
        const tabName = tab.dataset.tab;
        authTabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        
        document.getElementById('login-form').classList.remove('active');
        document.getElementById('register-form').classList.remove('active');
        document.getElementById(`${tabName}-form`).classList.add('active');
    });
});

// Login
loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const email = document.getElementById('login-email').value;
    const password = document.getElementById('login-password').value;
    
    showAuthLoading(true);
    const result = await PBAuth.login(email, password);
    showAuthLoading(false);
    
    if (result.success) {
        await loadGameData();
        showScreen('title');
    } else {
        showToast(result.error, 'error');
    }
});

// Register
registerForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('register-username').value;
    const email = document.getElementById('register-email').value;
    const password = document.getElementById('register-password').value;
    const passwordConfirm = document.getElementById('register-password-confirm').value;
    
    if (password !== passwordConfirm) {
        showToast('Las contraseñas no coinciden', 'error');
        return;
    }
    
    showAuthLoading(true);
    const result = await PBAuth.register(username, email, password);
    showAuthLoading(false);
    
    if (result.success) {
        await loadGameData();
        showScreen('title');
    } else {
        showToast(result.error, 'error');
    }
});

function showAuthLoading(show) {
    const loading = document.getElementById('auth-loading');
    if (show) {
        loading.classList.add('active');
    } else {
        loading.classList.remove('active');
    }
}
```

### Paso 6: Probar localmente

1. Abre `index.html` en tu navegador
2. Deberías ver la pantalla de login
3. Crea una cuenta
4. ¡Juega!

### Paso 7: Deploy a Fly.io (cuando funcione localmente)

```powershell
# Instalar Fly CLI
powershell -Command "iwr https://fly.io/install.ps1 -useb | iex"

# Login
flyctl auth login

# Ir a carpeta backend
cd backend

# Lanzar app (primera vez)
flyctl launch

# Deploy
flyctl deploy

# Ver URL de tu app
flyctl status
```

Luego actualiza `pb-client.js` línea 10:
```javascript
const PB_URL = 'https://tu-app.fly.dev';
```

## 🐛 Troubleshooting

### "Failed to fetch" al hacer login/register
- Verifica que PocketBase esté corriendo (`.\pocketbase.exe serve`)
- Verifica que la URL en `pb-client.js` sea correcta

### "Unauthorized" en requests
- Verifica las API Rules en cada colección
- Revisa la consola del navegador para más detalles

### No se guardan los datos
- Verifica que las colecciones existan
- Chequea que los tipos de datos sean correctos

## 📚 Recursos

- [PocketBase Docs](https://pocketbase.io/docs/)
- [Fly.io Docs](https://fly.io/docs/)
- [JavaScript SDK](https://github.com/pocketbase/js-sdk)
