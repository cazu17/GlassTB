# Riesgos de integración con Windows

| Mecanismo heredado | Encapsulación | Impacto de una actualización incompatible |
| --- | --- | --- |
| Detours en Explorer | `ExplorerHooks/` concentra el enganche de composición y la integración con Vista de tareas. Los módulos de Explorer comparten el GUID del payload declarado en `Common/constants.hpp`. | Un cambio en los puntos de enganche o en interfaces internas puede impedir la inicialización, interrumpir los refrescos o causar reinicios de Explorer. El trabajador limita los reintentos ante reinicios repetidos. |
| XAML Diagnostics | `ExplorerTAP/tapsite.cpp` establece la conexión con `InitializeXamlDiagnosticsEx`. `ExplorerTAP/taskbarappearanceservice.cpp` aplica los efectos vía el servicio interno. | Si falla la conexión o cambia el árbol XAML de Windows 11, el servicio puede dejar de detectar la barra o de aplicar los efectos. Una inicialización exitosa puede mantener el módulo cargado hasta que Explorer termine. |
| `SetWindowCompositionAttribute` | Las declaraciones están en `Common/undoc/user32.hpp`; la resolución dinámica se realiza en `TranslucentTB/dynamicloader.hpp` y el enganche de Explorer queda en `ExplorerHooks/swcadetour.cpp`. | Si desaparece la exportación o cambia el contrato de los atributos, puede fallar la inicialización o la aplicación del color y la transparencia en las barras que dependen de esa vía. |
| Otras interfaces no documentadas | `Common/undoc/` agrupa las declaraciones internas de Explorer, User32, UxTheme y NT usadas por los adaptadores existentes. | Cambios de contrato pueden afectar la detección de estados de la shell, el tema o la identificación de ventanas. |

Después de actualizar Windows, comprobar arranque, icono de bandeja, apariencia en escritorio y ventana maximizada, recuperación tras reiniciar `explorer.exe` y salida limpia. Comprobar la coexistencia instalando ambos paquetes y abriendo GlassTB con TranslucentTB ya iniciado.
