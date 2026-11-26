# Fase 2B — Motor de compatibilidad (instrucciones para ChatGPT)

> Lee primero `AGENTS.md`. Crea la rama `fase-2b/compatibilidad` **desde `fase-2a/identidad`**
> y abre el PR contra `fase-2a/identidad` (PR apilado; se rebasará cuando 2A se integre).

## Objetivo

Crear una capa que describa el sistema y decida qué mecanismo usar, **extrayendo la lógica que ya
existe** en `TaskbarAttributeWorker`, sin cambiar el comportamiento visible. Es la base para que las
fases siguientes (efectos, perfiles, diagnóstico) consulten capacidades en vez de repetir
comprobaciones de versión dispersas.

## Código existente que debes reutilizar (no reescribir desde cero)

- `enum class TaskbarType { Unknown, Classic, Mixed, XAML }` y `TaskbarAttributeWorker::GetTaskbarType(Window)`
  en `TranslucentTB/taskbar/taskbarattributeworker.{hpp,cpp}` (detecta `Taskbar.dll` en el proceso de Explorer).
- `m_IsWindows11` y `m_IsBlurAccentStateSupported` (constructor del worker, comprueba revisión ≥ 282 vía `win32::GetWindowsBuild()`).
- `win32::GetWindowsBuild()` e `win32::IsAtLeastBuild()` en `Common/win32.hpp`.
- La decisión de inyectar `ExplorerTAP` cuando `m_TaskbarType == TaskbarType::XAML` (en `ResetState`).

## Alcance

1. **Módulo `TranslucentTB/compat/`** (añádelo a `TranslucentTB.vcxproj` y `.filters`):
   - `systeminfo.hpp/.cpp`: struct `SystemInfo` con versión (major, minor, build, revisión),
     arquitectura nativa del SO y del proceso (`IsWow64Process2`), e indicador Windows 11.
     Obtención con las funciones de `win32.hpp` ya existentes.
   - `capabilities.hpp/.cpp`: struct `Capabilities` (p. ej. `blurAccentState`, `xamlTaskbarService`,
     `classicAccentPolicy`) calculado por una **función pura** `ComputeCapabilities(const SystemInfo&, TaskbarType)`
     que reproduzca exactamente las reglas actuales (incluida la revisión 282).
   - `backend.hpp/.cpp`: `enum class Backend { None, AccentPolicy, XamlTap, Mixed }` y función pura
     `SelectBackend(TaskbarType, const Capabilities&)` que replique la decisión actual del worker.
2. **Conectar sin cambiar comportamiento**: `TaskbarAttributeWorker` usa `ComputeCapabilities` y
   `SelectBackend` en lugar de su lógica en línea. `GetTaskbarType` puede moverse a `compat/`
   manteniendo su firma o dejándose donde está; justifica la elección en `docs/glasstb/decisiones.md` (ADR-003).
   La UI (`mainappwindow.cpp`, `IsBlurSupported`) debe seguir recibiendo los mismos valores.
3. **Diagnóstico**: al arrancar y tras cada reinicio de Explorer, registra con el logger existente
   (`ProgramLog`, spdlog) una línea `info` con versión, arquitecturas, tipo de barra, capacidades y backend elegido.
4. **Pruebas unitarias** en `Tests/compat/` (Google Test, añádelas a `Tests.vcxproj`): casos para
   `ComputeCapabilities` y `SelectBackend` con builds representativos — 19045 (Win10), 22000 rev 281 y 282,
   22621, 26100 — y con `TaskbarType::Unknown`. Las funciones deben ser puras para poder probarlas sin Windows real.
5. **Documentación**: crea `docs/glasstb/compatibilidad.md` con la tabla de reglas
   (build/revisión → capacidades → backend) y la **matriz de compatibilidad** con estas columnas de estado:
   Probado · Parcial · Experimental · No compatible · No verificado. Marca todo como **No verificado**
   salvo lo que realmente se haya probado; CI solo prueba compilación y tests unitarios.

## Fuera de alcance

- Nuevos efectos, nueva UI, perfiles, cambio de ruta de configuración, cambios en `ExplorerHooks`/`ExplorerTAP`.
- Cambiar umbrales o reglas de compatibilidad: si encuentras una regla dudosa, documéntala, no la cambies.

## Criterios de aceptación

- Workflow `Build` en verde (x64 y ARM64), `Tests.exe` en verde con las pruebas nuevas ejecutándose
  (cita en el PR el número de tests que corrieron).
- Diff del worker revisable: solo sustituye lógica por llamadas a `compat/`, sin cambiar decisiones.
- El PR lista pruebas manuales en Windows 10 y 11: comprobar en el log la línea de diagnóstico y que
  la apariencia de la barra es idéntica a la de 2A.
