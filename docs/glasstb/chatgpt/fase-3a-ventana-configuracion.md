# Fase 3A — Ventana de configuración: Inicio y Apariencia (instrucciones para ChatGPT)

> Lee primero `AGENTS.md`. Crea la rama `fase-3a/ventana-configuracion` desde `origin/develop`
> y abre el PR contra `develop`.

## Objetivo

Primera entrega de la nueva interfaz: una ventana **GlassTB** con navegación lateral, abierta desde
el icono de la bandeja, con dos secciones **completamente funcionales**: *Inicio* y *Apariencia*.
Todo debe actuar sobre la configuración y la barra reales. No añadas secciones vacías ni botones
decorativos: las demás secciones (Perfiles, Automatización, Pantallas…) llegarán en fases siguientes.

## Decisión de tecnología (obligatoria antes de programar)

La UI actual usa **XAML Islands + WinUI 2.8** dentro del proceso (`Xaml/`, hospedada con
`Application::CreateXamlWindow<T>` y `BaseXamlPageHost`). Migrar a WinUI 3 implica otro runtime
(Windows App SDK), otro modelo de ventanas y un cambio de empaquetado.
**En esta fase se usa la infraestructura existente (WinUI 2.8)**. Registra en `docs/glasstb/decisiones.md`
el ADR-004 con la evaluación WinUI 2.8 frente a WinUI 3 (consumo de memoria, compatibilidad con
Windows 10, empaquetado portable, esfuerzo) y la recomendación para fases futuras. No migres nada.

## Código existente que debes reutilizar

- Hospedaje XAML: `TranslucentTB/application.hpp` (`CreateXamlWindow`), `uwp/basexamlpagehost.*`,
  `uwp/xamlpagehost.hpp`, y como ejemplos `Xaml/Pages/WelcomePage.*` y `ColorPickerPage.*`.
- Selector de color existente: `Xaml/Controls/ColorPicker.*` y el flujo de vista previa en
  `MainAppWindow` (`ApplyColorPreview`, `TaskbarAttributeWorker::ApplyColorPreview`).
- Modelo de configuración: `Common/config/config.hpp` (`DesktopAppearance`, `VisibleWindowAppearance`,
  `MaximisedWindowAppearance`, `StartOpenedAppearance`, `SearchOpenedAppearance`,
  `TaskViewOpenedAppearance`, `BatterySaverAppearance`), `TaskbarAppearance` (accent, color,
  showPeek, showLine, blurRadius), `ConfigManager::SaveConfig`, `ConfigurationChanged`.
- Modelos XAML de apariencia ya existentes: `Xaml/Models/Primitives/TaskbarAppearance.*` y
  `OptionalTaskbarAppearance.*`.
- Estado del motor (Fase 2B): `compat::SystemInfo`, `compat::Capabilities`, `compat::SelectBackend`,
  `TaskbarAttributeWorker::GetType()`.

## Alcance

1. **Página `SettingsPage`** (nuevo `Xaml/Pages/SettingsPage.{idl,h,cpp,xaml}`) con `muxc:NavigationView`
   lateral, tema claro/oscuro automático según Windows, tipografía del sistema, accesible por teclado
   (orden de tabulación, `AutomationProperties.Name` en todos los controles).
2. **Apertura**: nueva entrada **"Abrir GlassTB"** al principio del menú de la bandeja (menú XAML y menú
   clásico si ambos existen). Si la ventana ya está abierta, se trae al frente (una sola instancia).
   Cerrar la ventana **no** detiene el motor.
3. **Sección Inicio** (solo datos reales):
   - Estado del motor (activo / sin barra detectada) y backend seleccionado.
   - Versión de Windows (build y revisión), arquitectura nativa y del proceso, tipo de barra.
   - Capacidades detectadas (desenfoque AccentPolicy, servicio XAML).
   - Número de pantallas con barra de tareas detectadas por el worker.
   - Botón "Copiar diagnóstico" que copia ese resumen al portapapeles.
   Los datos se pasan del proceso principal a la página con un modelo/propiedades definidos en IDL
   (como hace `TrayFlyoutPage` con `SetTaskbarType`); actualízalos tras reinicios de Explorer.
4. **Sección Apariencia**: un editor por cada estado de `Config` (Escritorio, Ventana visible,
   Ventana maximizada, Inicio, Búsqueda, Vista de tareas, Ahorro de batería) con:
   - Activado (solo estados opcionales), material (los `ACCENT_STATE` que ya expone el menú actual),
     color con el `ColorPicker` existente, radio de desenfoque cuando aplique, mostrar botón Peek
     (Win10) y línea de la barra (Win11), deshabilitando lo que `Capabilities` indique como no soportado
     con un texto que explique por qué.
   - **Vista previa en tiempo real** sobre la barra real usando el mecanismo existente.
   - Botones **Aplicar** (guarda con `SaveConfig`), **Cancelar** (descarta y restaura la apariencia
     guardada) y **Restablecer** (valores por defecto de `Config`, con confirmación).
   - Las reglas por ventana (`RuledTaskbarAppearance` rules) no se editan aquí: muestra cuántas hay
     y deja la edición para la fase de Automatización.
   - Coherencia con el menú de la bandeja: un cambio hecho en cualquiera de los dos se refleja en el otro.
5. **Textos**: todos los textos nuevos en `Xaml/Strings/es-US/Resources.resw` (español, idioma principal)
   y `en-US`; para los demás idiomas usa la cadena en inglés como valor provisional. Nada de textos fijos en XAML/C++.
6. **Pruebas**: añade tests unitarios para cualquier lógica pura nueva (p. ej. conversión entre modelo
   XAML y `TaskbarAppearance`, formato del diagnóstico). Documenta en el PR las pruebas manuales.

## Fuera de alcance

Perfiles, reglas, multimonitor, efectos nuevos, cambio de ruta de configuración, WinUI 3, Mica en la ventana
si requiere APIs no disponibles en XAML Islands (documenta si es viable).

## Criterios de aceptación

- Workflow `Build` en verde en x64 y ARM64, `Tests.exe` en verde.
- Ningún control sin efecto real; todo control deshabilitado explica el motivo.
- El menú de la bandeja actual sigue funcionando igual.
- El PR incluye la lista de pruebas manuales en Windows 11 (abrir desde la bandeja, editar cada estado,
  vista previa, aplicar/cancelar/restablecer, reinicio de Explorer con la ventana abierta, navegación con teclado,
  Narrador) y capturas **solo si se tomaron realmente** (este Mac no puede ejecutar la app: dilo explícitamente).
