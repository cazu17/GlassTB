# Estabilidad y recuperación

| Situación | Comportamiento |
| --- | --- |
| Reinicio de `explorer.exe` | Heredado de TranslucentTB: se detecta `TaskbarCreated`, se reinyecta y se reaplica. Si Explorer se reinicia de nuevo en < 30 s se asume un fallo provocado por la inyección y se detiene (`disable_explorer_crash_detection` lo desactiva). |
| Suspensión/reanudación, DPI, resolución, pantallas | El worker recalcula las barras con los eventos de Windows; las reglas se reevalúan con `WM_POWERBROADCAST` y `WM_DISPLAYCHANGE`. |
| `config.json` dañado | Valores por defecto en memoria y copia del archivo en `backups\config-invalid-*.json`. |
| `rules.json` dañado | Se mantienen las últimas reglas válidas y la sección Automatización muestra el error. |
| Configuración de una versión más nueva | Se lee, pero nunca se sobrescribe. |
| Cierres inesperados repetidos | Un marcador `running.marker` registra los arranques sin salida limpia; con 3 en 10 minutos el siguiente arranque usa **modo seguro**. |
| Modo seguro manual | `GlassTB.exe --safe-mode` o mantener **Shift** al abrirlo. No se inyecta nada en Explorer y la barra conserva el estilo de Windows; la configuración y los perfiles se pueden revisar. |
| Pausar efectos | Menú de la bandeja → *Pausar efectos*: vuelve al estilo de Windows sin salir. |
| Salida | Desde la bandeja: restaura las barras (`ReturnToStock`) y borra el marcador. |

Pendiente de verificar en Windows real: todas las filas (ver `compatibilidad.md`).
