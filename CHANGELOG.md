# Registro de cambios

## [Sin publicar]

### Añadido
- Identidad GlassTB (nombre, ejecutable, paquete, GUID e identificadores IPC propios) y aviso de coexistencia con TranslucentTB.
- Motor de compatibilidad (`compat/`): versión, arquitectura, tipo de barra, capacidades y backend, con diagnóstico en el log.
- Ventana de configuración: Inicio (diagnóstico), Apariencia (editor con vista previa, aplicar/cancelar/restablecer),
  Perfiles, Color adaptativo y Automatización.
- Datos en `%LOCALAPPDATA%\GlassTB` con `schema_version`, migración desde `settings.json`/TranslucentTB, copias rotativas y registros.
- Perfiles: 9 predefinidos, guardar/duplicar/eliminar/importar/exportar, selector en la bandeja.
- Automatización por eventos (`rules.json`) con Y/O/NO, prioridades, resolución determinista y debounce; editor visual básico.
- Color adaptativo desde el fondo de pantalla (WIC, 64×64, sin sondeo).
- Modo seguro, detección de bucles de cierre y pausa de efectos/automatización desde la bandeja.
- CI en GitHub Actions (x64/ARM64 + tests) y workflow de release con SHA-256.

### Cambiado
- La configuración pasa de `settings.json` a `config.json`; ya no se escribe el comentario ni el `$schema` de TranslucentTB.

### Basado en
- TranslucentTB (GPLv3) — ver `NOTICE.md`.
