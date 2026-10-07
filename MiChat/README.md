# Mi Chat

Chat personal (mensajes a vos mismo) de escritorio hecho en C++ / Qt 6.

- Todo se guarda localmente en SQLite: `%APPDATA%\MiChat\MiChat\chat.db`
- Pegá imágenes con Ctrl+V (capturas, copiadas del navegador) o archivos copiados del explorador; también arrastrar y soltar o el botón 📎
- Click en una imagen para ampliarla; clic derecho: copiar imagen / guardar como
- Los links se muestran como tarjeta con miniatura y título (YouTube y cualquier página con OpenGraph)
- Buscador (Ctrl+F): texto, nombres de archivo, títulos/URLs de links; filtro por tipo y por rango de fechas; ignora acentos y mayúsculas
- Clic derecho en un mensaje: copiar texto / eliminar

## Generar el instalador (MiChat-Setup.exe)
**Opción A – sin instalar nada:** subí esta carpeta a un repositorio de GitHub. En la pestaña *Actions* se ejecuta
"Instalador Windows" y al terminar descargás `MiChat-Setup` (artifact).

**Opción B – local:** instalá Qt 6 (MSVC), CMake, Ninja e Inno Setup 6 y ejecutá `build_windows.bat`.
