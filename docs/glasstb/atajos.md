# Atajos globales

Configuración → **Atajos** (o `config.json` → `hotkeys`). Sin asignar por defecto para no quitar atajos a otras aplicaciones.

| Clave | Acción |
| --- | --- |
| `open_settings` | Abrir la ventana de GlassTB |
| `toggle_effects` | Pausar/reanudar efectos (vuelve al estilo de Windows sin salir) |
| `next_profile` | Activar el siguiente perfil de la lista |
| `toggle_automation` | Pausar/reanudar las reglas de automatización |

Formato: modificadores `Ctrl`, `Alt`, `Shift`, `Win` + una tecla (`A`–`Z`, `0`–`9`, `F1`–`F24`, `Space`, `Enter`, `Tab`, `Esc`,
`Home`, `End`, `PageUp`, `PageDown`, flechas, `Insert`, `Delete`, `Pause`, `PrintScreen`). Se registran con `RegisterHotKey`
y `MOD_NOREPEAT`; si Windows lo rechaza (atajo ocupado) se anota un aviso en el registro.
