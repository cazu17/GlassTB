# Automatización (rules.json)

Las reglas viven en `%LOCALAPPDATA%\GlassTB\rules.json`. Se editan desde **Configuración → Automatización →
Editar reglas** (la primera vez se crea una plantilla) y se recargan automáticamente al guardar el archivo.
Si el archivo tiene un error, se conserva el último conjunto válido y la sección muestra el error.

## Funcionamiento

- Se evalúan las reglas habilitadas; gana la de **mayor prioridad** cuya condición se cumple.
  Con la misma prioridad gana la que aparece **primero** en el archivo (resolución determinista).
- La regla ganadora se aplica **en memoria** sobre la configuración: `config.json` no se reescribe.
  Al dejar de cumplirse, vuelve la configuración guardada.
- Motor por eventos: cambio de ventana en primer plano, alimentación/batería/ahorro de energía, tema de Windows
  y cambios de pantallas disparan una evaluación con *debounce* de 300 ms.
- Un temporizador por minuto existe solo si alguna regla usa horario o días; la lista de procesos se consulta cada
  15 s solo si alguna regla usa `app_running`. Si el ganador no cambia, no se toca la barra (deduplicación).
- **Pausar automatización** desactiva todas las reglas sin borrarlas.

## Condiciones

| `type` | Parámetros | Se cumple cuando |
| --- | --- | --- |
| `all` / `any` | `conditions: [...]` | Todas (Y) / alguna (O) se cumplen |
| `not` | `conditions: [una]` | La condición no se cumple (NO) |
| `foreground_app` | `apps: ["game.exe"]` | La ventana activa pertenece a uno de esos ejecutables |
| `app_running` | `apps: [...]` | Alguno de esos ejecutables está en ejecución |
| `foreground_maximized` | — | La ventana activa está maximizada |
| `fullscreen` | — | Hay una app a pantalla completa, presentación o modo ocupado (`SHQueryUserNotificationState`) |
| `time_range` | `from`, `to` (`"HH:MM"`) | La hora local está en el rango (puede cruzar medianoche) |
| `weekdays` | `days: [0..6]` | Día de la semana (0 = domingo) |
| `on_battery` | — | El equipo funciona con batería |
| `battery_saver` | — | El ahorro de batería está activo |
| `battery_below` | `percent` (1–100) | Batería por debajo del porcentaje |
| `dark_theme` | — | El tema de aplicaciones de Windows es oscuro |
| `monitors_at_least` | `count` (1–16) | Hay al menos ese número de pantallas |

El estado de Inicio, Búsqueda y Vista de tareas ya tiene apariencia propia en **Apariencia**, por eso no se repite como
condición. *Cambio de fondo de pantalla* se usará en el color adaptativo (fase 5).

## Acciones

| `type` | Parámetros | Efecto (en memoria) |
| --- | --- | --- |
| `activate_profile` | `profile` | Usa las apariencias de ese perfil (predefinido o tuyo) |
| `restore_windows` | — | Estilo normal de Windows en todos los estados (modo rendimiento) |
| `set_material` | `material`: `normal`, `opaque`, `clear`, `blur`, `acrylic` | Cambia el material de todos los estados |
| `set_color` | `color`: `#RRGGBB` o `#RRGGBBAA` | Cambia el color de todos los estados |
| `set_opacity` | `opacity` (0–255) | Cambia la opacidad del color de todos los estados |

Las acciones de una regla se aplican en orden (por ejemplo, perfil y después opacidad).

## Límites y validación

200 reglas, 8 niveles de anidamiento, 32 hijos por grupo, archivo ≤ 1 MiB, nombres de 1–100 caracteres,
prioridad entre −1000 y 1000. Cualquier tipo o valor desconocido invalida el archivo completo (con el motivo).

## Ejemplo

```jsonc
{
  "schema_version": 1,
  "rules": [
    {
      "name": "Juegos",
      "priority": 100,
      "condition": { "type": "any", "conditions": [
        { "type": "fullscreen" },
        { "type": "foreground_app", "apps": ["eldenring.exe", "cs2.exe"] }
      ] },
      "actions": [ { "type": "activate_profile", "profile": "OLED" } ]
    },
    {
      "name": "Noche",
      "priority": 10,
      "condition": { "type": "time_range", "from": "21:00", "to": "07:00" },
      "actions": [ { "type": "activate_profile", "profile": "Midnight" } ]
    }
  ]
}
```
