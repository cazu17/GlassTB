# Fase 2A — Identidad GlassTB (instrucciones para ChatGPT)

> Copia todo este archivo en ChatGPT (Codex) junto con el acceso al repositorio `cazu17/GlassTB`.
> Lee primero `AGENTS.md`. Trabaja en una rama nueva `fase-2a/identidad` creada desde `develop`
> y abre un pull request contra `develop`.

## Objetivo

Convertir el fork en GlassTB **sin cambiar ningún comportamiento**: la app debe hacer exactamente
lo mismo que TranslucentTB, pero con nombre, identificadores y textos propios, de modo que pueda
instalarse sin chocar con una instalación de TranslucentTB.

## Alcance (hacer)

1. **Nombre de la aplicación**: en `Common/appinfo.hpp`, `UTF8_APP_NAME` pasa a `GlassTB`,
   `GlassTB (Canary)` y `GlassTB (Dev)`.
2. **Ejecutable**: el binario de salida pasa a `GlassTB.exe` (ajusta `TargetName` en
   `TranslucentTB/TranslucentTB.vcxproj` y todas las referencias en `AppPackage/Package.appxmanifest`,
   `AppPackage/AppPackage.wapproj` y `.azp/scripts/*.ps1`). **No renombres carpetas ni proyectos**.
3. **Identificadores únicos**: genera GUID nuevos para `MUTEX_GUID`, `TRAY_GUID` y `EXPLORER_PAYLOAD`
   en `Common/constants.hpp`, y cambia el prefijo de los mensajes registrados y eventos
   (`TTB_`, `TTBHook_`, `TTBTAP_`) a `GTB_`, `GTBHook_`, `GTBTAP_`. Busca **todas** las apariciones
   (también en `ExplorerHooks/` y `ExplorerTAP/`), porque el proceso principal y las DLL deben coincidir.
   Revisa también los CLSID/IID de `ExplorerTAP/*.idl` y decide si deben cambiar para no colisionar
   con TranslucentTB registrado en el mismo equipo; documenta la decisión.
4. **Paquete MSIX**: `Identity Name="GlassTB"`, publisher `CN=GlassTB Open Source Developers`,
   `DisplayName`, `StartupTask TaskId="GlassTB"` y el alias de ejecución `GlassTB.exe`.
5. **Recursos de versión y textos**: en `TranslucentTB/resources/language/*.rc2` y `Xaml/Strings/*`,
   los textos que muestran el nombre deben decir GlassTB. No traduzcas ni reescribas otros textos.
6. **Coexistencia**: al iniciar, si detectas una instancia de TranslucentTB en ejecución (ventana
   de bandeja con clase y título de TranslucentTB), muestra un aviso en español indicando que ambas
   aplicaciones modifican la barra de tareas y que debe cerrarse una. No cierres TranslucentTB.
7. **Licencia y créditos**: mantén `LICENSE.md`. Reescribe `README.md` en español presentando
   GlassTB como fork de TranslucentTB con enlace y créditos a sus autores. Añade `NOTICE.md` con
   el aviso de derivación (GPLv3, sección 5).
8. **Documentación inicial**: crea `docs/glasstb/decisiones.md` (ADR-001: base del fork y por qué
   se conserva la arquitectura; ADR-002: cambio de identificadores) y `docs/glasstb/riesgos.md`
   (describe Detours en Explorer, XAML Diagnostics y `SetWindowCompositionAttribute`, y qué pasa
   si una actualización de Windows los rompe).

## Fuera de alcance (no hacer todavía)

- Nueva interfaz, perfiles, reglas, efectos nuevos, cambio de la ruta de configuración
  (se hará en la fase 4), cambio de iconos (se entregarán aparte).
- Migrar a WinUI 3 o cambiar el sistema de build.

## Criterios de aceptación

- El workflow `Build` pasa en x64 y ARM64, y `Tests.exe` pasa en x64.
- `git grep -n "TTB_\|TTBHook_\|TTBTAP_"` no devuelve identificadores de runtime antiguos
  (sí puede quedar el nombre TranslucentTB en créditos, licencia y nombres de proyecto).
- El artefacto portable contiene `GlassTB.exe`.
- El PR lista las pruebas manuales pendientes en Windows: arranque, icono de bandeja, cambio de
  apariencia en escritorio y ventana maximizada, reinicio de `explorer.exe`, salida limpia.
