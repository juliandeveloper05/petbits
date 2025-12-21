# 🎮 PetBits - Virtual Pet Simulator

<div align="center">

![PetBits Logo](https://img.shields.io/badge/PetBits-Virtual_Pet-9bbc0f?style=for-the-badge&logo=gamepad)
[![JavaScript](https://img.shields.io/badge/JavaScript-Vanilla-F7DF1E?style=for-the-badge&logo=javascript)](https://developer.mozilla.org/en-US/docs/Web/JavaScript)
[![PocketBase](https://img.shields.io/badge/PocketBase-Backend-B8DBE4?style=for-the-badge&logo=pocketbase)](https://pocketbase.io)
[![License](https://img.shields.io/badge/License-MIT-green?style=for-the-badge)](LICENSE)

*Un simulador de mascotas virtuales con estética retro y efectos CRT*

[Demo](https://tu-deploy-url.com) • [Documentación](#-documentación) • [Contribuir](#-contribuir)

</div>

---

## ✨ Características

- 🐾 **Múltiples mascotas**: Perros, gatos, conejos, hamsters ¡y más!
- 🎨 **Estética retro**: Efectos CRT, escanlines y pixel art
- 📈 **Sistema de progresión**: Tus mascotas crecen de bebé a adulto
- 🍎 **Sistema de alimentación**: 5 tipos de comida con diferentes efectos
- 💰 **Economía de juego**: Gana monedas cuidando a tus mascotas
- 🎮 **Responsive**: Funciona en móvil, tablet y desktop
- 🔐 **Autenticación**: Sistema de usuarios con PocketBase
- ☁️ **Sincronización**: Tus datos se guardan en la nube

## 🎯 Gameplay

1. **Compra** una mascota en la tienda
2. **Aliméntala** para mantener su energía
3. **Juega** con ella para ganar XP
4. **Observa** cómo crece a través de 3 etapas
5. **Gana monedas** pasivamente mientras la cuidas
6. **Colecciona** todas las mascotas

## 🛠️ Tecnologías

- **Frontend**: HTML5, CSS3, JavaScript (Vanilla)
- **Backend**: [PocketBase](https://pocketbase.io) (Go)
- **Fonts**: Press Start 2P, VT323
- **Hosting**: GitHub Pages + Fly.io

## 📦 Instalación Local

### Requisitos previos

- Git
- PowerShell (Windows) o Bash (Linux/Mac)
- Navegador web moderno

### Frontend

```bash
# Clonar el repositorio
git clone https://github.com/tu-usuario/petbits.git
cd petbits

# Abrir index.html en tu navegador
# O usar un servidor local:
npx http-server
```

### Backend (PocketBase)

#### Windows (PowerShell)

```powershell
cd backend
.\setup.ps1
```

#### Manual

```bash
cd backend

# Descargar PocketBase
wget https://github.com/pocketbase/pocketbase/releases/download/v0.22.0/pocketbase_0.22.0_linux_amd64.zip
unzip pocketbase_0.22.0_linux_amd64.zip
chmod +x pocketbase

# Ejecutar
./pocketbase serve
```

#### Configurar las colecciones

1. Abre `http://127.0.0.1:8090/_/`
2. Crea una cuenta de admin
3. Sigue las instrucciones en [SETUP_BACKEND.md](SETUP_BACKEND.md)

## 🚀 Deploy

### Frontend (GitHub Pages)

```bash
# Asegúrate de que el código esté en la rama main
git push origin main

# Habilita GitHub Pages en Settings > Pages
# Source: main branch, / (root)
```

### Backend (Fly.io)

```bash
cd backend

# Instalar Fly CLI
curl -L https://fly.io/install.sh | sh

# Login
flyctl auth login

# Deploy
flyctl launch
flyctl deploy

# Actualizar URL en pb-client.js con tu URL de Fly.io
```

## 📁 Estructura del Proyecto

```
petbits/
├── index.html          # Página principal
├── style.css           # Estilos con efectos CRT
├── main.js             # Lógica del juego
├── pb-client.js        # Cliente de PocketBase
├── README.md           # Este archivo
├── SETUP_BACKEND.md    # Guía detallada del backend
└── backend/
    ├── README.md       # Documentación del backend
    ├── setup.ps1       # Script de setup para Windows
    ├── fly.toml        # Configuración de Fly.io
    └── pb_data/        # Datos de PocketBase (no en git)
```

## 🎮 Controles

### Móvil / Desktop
- **Click en mascota**: Interactuar (+1 XP)
- **Botón Alimentar**: Abrir menú de comida
- **Botón Jugar**: Jugar con la mascota (-10 hambre, +15 XP)
- **Botón Dormir**: Recuperar hambre (+6 por 3 segundos)

### Consola (decorativos)
- **Botón A**: Alimentar
- **Botón B**: Volver a tienda

## 🧪 Testing

```bash
# Frontend (puedes usar cualquier servidor)
npx http-server -p 8080

# Backend
cd backend
./pocketbase serve
```

## 🤝 Contribuir

¡Las contribuciones son bienvenidas! Por favor:

1. Haz fork del proyecto
2. Crea una rama para tu feature (`git checkout -b feature/NuevaFeature`)
3. Commit tus cambios (`git commit -m '✨ Add: Nueva feature'`)
4. Push a la rama (`git push origin feature/NuevaFeature`)
5. Abre un Pull Request

## 📝 TODO

- [ ] Sistema de logros
- [ ] Mini-juegos interactivos
- [ ] Más tipos de mascotas
- [ ] Sistema de breeding
- [ ] Leaderboard global
- [ ] Sonidos y música
- [ ] Modo oscuro/claro
- [ ] PWA (Progressive Web App)
- [ ] i18n (Inglés, Portugués)

## 🐛 Bugs Conocidos

- [ ] En algunos móviles antiguos, las animaciones pueden verse entrecortadas
- [ ] El inventario no se actualiza en tiempo real entre tabs

## 📄 Licencia

Este proyecto está bajo la Licencia MIT - ver el archivo [LICENSE](LICENSE) para más detalles.

## 👤 Autor

**Tu Nombre**
- GitHub: [@tu-usuario](https://github.com/tu-usuario)
- Email: tu@email.com

## 🙏 Agradecimientos

- Pixel art fonts de Google Fonts
- PocketBase por el increíble backend
- Inspirado en los clásicos Tamagotchi y Neopets

---

<div align="center">

**¿Te gustó el proyecto? ¡Dale una ⭐!**

Hecho con ❤️ y mucho ☕

</div>
