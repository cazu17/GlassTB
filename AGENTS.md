# Instrucciones para agentes de código (ChatGPT / Codex y otros)

GlassTB es un fork de [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB) (GPLv3).
El objetivo es evolucionarlo sin romper lo que ya funciona.

## Reglas obligatorias

1. Inspecciona los archivos reales antes de modificarlos. No inventes APIs, clases, métodos ni dependencias.
2. No sustituyas funcionalidad existente por implementaciones simuladas, maquetas ni datos ficticios.
3. No dejes botones o controles sin efecto real. Si algo no es viable técnicamente, deshabilítalo y explícalo.
4. Cambios pequeños y revisables: un pull request por tarea, sin refactorizaciones masivas.
5. Todo debe compilar en CI (`.github/workflows/build.yml`, x64 y ARM64 Release). No ocultes errores ni desactives warnings para que pase.
6. No declares pruebas como superadas si no se ejecutaron. Indica qué se probó en CI y qué requiere prueba manual en Windows.
7. Conserva la licencia GPLv3, los avisos de copyright y los créditos a TranslucentTB.
8. Prohibido: evasión de antivirus, desactivar Defender u otras protecciones, drivers de kernel, ejecución de código remoto, modificar archivos protegidos de Windows.
9. Cualquier mecanismo no documentado (Detours en `ExplorerHooks`, XAML Diagnostics en `ExplorerTAP`, `Common/undoc`) debe quedar encapsulado y documentado en `docs/glasstb/riesgos.md`.
10. Registra cada decisión de arquitectura en `docs/glasstb/decisiones.md` (formato ADR breve).

## Estructura actual

| Carpeta | Contenido |
|---|---|
| `TranslucentTB/` | Proceso principal Win32: ciclo de vida, bandeja, `taskbar/taskbarattributeworker`, `managers/configmanager` |
| `ExplorerHooks/` | DLL inyectada en Explorer (Detours) — hook de `SetWindowCompositionAttribute` y vista de tareas |
| `ExplorerTAP/` | Integración XAML Diagnostics con la barra de Windows 11 + efectos (`effects/`) |
| `Xaml/` | Interfaz (XAML Islands, WinUI 2.8), recursos `Strings/<idioma>` |
| `Common/` | Configuración (`config/`), constantes, utilidades, APIs no documentadas (`undoc/`) |
| `Tests/` | Pruebas Google Test (`Tests.exe`) |
| `AppPackage/` | Empaquetado MSIX (wapproj) |

Toolchain: Visual Studio 2026 (v145), Windows SDK 10.0.28000, C++20, C++/WinRT, vcpkg (manifiestos), NuGet `packages.config`.

## Entrega de cada tarea

Al terminar, el PR debe incluir: resumen de cambios, archivos tocados, cómo se verificó, qué queda pendiente
y qué debe probarse manualmente en Windows 10/11.
