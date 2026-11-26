# Persistencia, perfiles y copias de seguridad

## Ubicación

Todos los datos del usuario se guardan en `%LOCALAPPDATA%\GlassTB`:

| Ruta | Contenido |
| --- | --- |
| `config.json` | Configuración activa (JSON con comentarios permitidos al leer). |
| `profiles\*.json` | Perfiles creados, duplicados o importados por el usuario (UTF-8). |
| `backups\*.json` | Copias de seguridad rotativas (ver abajo). |
| `logs\*.log` | Un registro por ejecución; se conservan los 10 más recientes. |

En la versión MSIX, Windows redirige `%LOCALAPPDATA%` a la carpeta privada del paquete
(`%LOCALAPPDATA%\Packages\<paquete>\LocalCache\Local\GlassTB`); el comportamiento es el mismo.

## Migración inicial

Si `config.json` no existe al arrancar, GlassTB busca, en este orden:

1. `settings.json` heredado: junto al ejecutable (portable) o en `RoamingState` del paquete.
2. Una instalación de TranslucentTB de Microsoft Store: cualquier carpeta de `%LOCALAPPDATA%\Packages`
   cuyo nombre contenga `TranslucentTB` y tenga `RoamingState\settings.json`.

El archivo encontrado se copia (nunca se mueve ni modifica) y además se guarda una copia en `backups\`
con el prefijo `legacy-settings` o `translucenttb-import`. La migración ocurre antes de crear el
observador de archivos, así que no provoca recargas.

## Versionado del esquema

- `config.json` incluye `"schema_version": 1`. Un archivo sin la clave es versión 0 (formato de TranslucentTB,
  mismas claves). Antes de reescribirlo se guarda `backups\config-v0-*.json`.
- Si la versión es mayor que la soportada, GlassTB carga las claves conocidas pero **no sobrescribe** el archivo
  (los botones de guardar/perfiles quedan deshabilitados con un aviso).
- Los perfiles también llevan `schema_version`; uno de versión mayor se rechaza al importar.

## Validación y recuperación

- JSON inválido o con tipos incorrectos: se usan los valores por defecto en memoria y se copia el archivo roto a
  `backups\config-invalid-*.json` para no perder las ediciones del usuario.
- Escritura atómica: se escribe un `.tmp` y se reemplaza con `ReplaceFile`/`MoveFileEx` (write-through).
  Si falla, la configuración en memoria vuelve al estado anterior y la UI informa el error.
- Copia diaria: la primera carga válida de cada sesión guarda `backups\config-*.json`.
- Rotación: se conservan las 10 copias más recientes de cada prefijo.

## Perfiles

- Un perfil contiene solo las siete apariencias (escritorio, ventana visible, maximizada, Inicio, Búsqueda,
  Vista de tareas, ahorro de batería). Aplicarlo **conserva** las reglas por ventana, las ventanas ignoradas y las
  opciones avanzadas.
- Nombres: 1–64 caracteres, sin `< > : " / \ | ? *`, sin espacios al inicio/fin ni punto final. El archivo se llama
  como el nombre en minúsculas. Los nombres de los perfiles predefinidos están reservados.
- Importar valida tamaño (≤ 256 KiB), UTF-8, JSON, tipos, colores y versión; si el nombre existe se añade ` (2)`, ` (3)`…
- Editar la apariencia a mano (editor o menú de la bandeja) borra el indicador de perfil activo.

### Perfiles predefinidos

Solo usan materiales que el motor aplica realmente (AccentPolicy en barras clásicas/mixtas y pinceles XAML en la barra XAML):

| Perfil | Escritorio | Ventana maximizada |
| --- | --- | --- |
| Invisible | Transparente | Transparente |
| Glass | Acrílico claro | Acrílico oscuro |
| Frosted | Acrílico esmerilado claro | Acrílico más opaco |
| Midnight | Acrílico azul noche | Opaco azul noche |
| OLED | Negro opaco | Negro opaco |
| Cyberpunk | Acrílico magenta | Opaco violeta |
| Gamer | Transparente con tinte rojo | Negro opaco |
| Productividad | Transparente | Estilo de Windows |
| Windows clásico | Estilo de Windows | Estilo de Windows |

**Liquid Glass** y **Minimal Dock** no se incluyen: requieren efectos de geometría/iluminación (márgenes, dock flotante,
reflejos) que el motor todavía no puede aplicar sin simularlos. Ver `compatibilidad.md`.
