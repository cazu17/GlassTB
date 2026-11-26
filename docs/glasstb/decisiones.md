# Decisiones de arquitectura

## ADR-001. Base del fork y arquitectura

**Decisión.** Usar la rama `ci/github-actions` del fork como base de la Fase 2A. Mantener el proceso Win32, la bandeja, el trabajador de barra de tareas, WinUI 2, los módulos de Explorer y la solución MSBuild existentes.

**Motivo.** La fase establece una identidad propia mientras conserva los modos de apariencia y permite comprobar ambos binarios con el flujo de compilación existente. El proyecto de empaquetado sigue resolviendo el ejecutable a través de la referencia a `TranslucentTB.vcxproj`, cuyo `TargetName` define `GlassTB.exe`.

## ADR-002. Identificadores propios

**Decisión.** Asignar a GlassTB su propia identidad MSIX, alias, tarea de arranque, clase de bandeja, mutex, GUID del icono y GUID del payload. Usar nombres con prefijos de GlassTB para los mensajes, el evento del TAP y las clases de ventana privadas que comparten el proceso principal y las DLL. Conservar los nombres e identificadores definidos por Windows.

**COM.** Renovar los CLSID de `TaskbarAppearanceService` y `TAPSite`, el LIBID de ExplorerTAP, los IID de `ITaskbarAppearanceService` e `IVersionedApi` y el CLSID del proxy/stub. `RegisterActiveObject` y `GetActiveObject` usan el CLSID del servicio, y `CoRegisterPSClsid` asocia los IID con el proxy. Cada aplicación identifica así su propio servicio y su propio proxy incluso cuando ambos TAP usan el mismo proceso Explorer. Los contratos y la versión de la API interna se conservan.

**Consecuencias.** El aviso busca la clase de bandeja histórica `TrayWindow` con los títulos de TranslucentTB estable, Canary y Dev. La separación de identificadores permite instalar ambos productos, pero el control de la apariencia de la barra sigue siendo compartido. Para instalar un MSIX de GlassTB, la firma debe corresponder al publisher del fork. El flujo de GitHub Actions produce una compilación sin firma.

## ADR-003 (Fase 2A). Orden de compilación del paquete MSIX

**Decisión.** Declarar en la solución que `AppPackage` depende del proyecto del ejecutable, igual que en su referencia de empaquetado.

**Motivo.** En GitHub Actions, el build paralelo de la rama base y el del PR reportaron `MSB4006` en `CppWinRTComputeGetResolvedWinMD`. Ordenar el proyecto del ejecutable antes de Desktop Bridge evita que el empaquetador consulte sus salidas mientras la solución todavía las está generando. Se conserva el workflow y su matriz de arquitecturas.

## ADR-003 (Fase 2B). Sistema, capacidades y selección de backend

**Decisión.** Consultar la versión y arquitectura del sistema en `TranslucentTB/compat/`. Calcular las capacidades y el backend con funciones puras a partir de esa información y del tipo de barra. Conservar `GetTaskbarType(Window)` en el worker.

**Motivo.** El detector ya usa la ventana de la barra y la enumeración del proceso Explorer. Mantenerlo en el worker conserva el momento de la detección durante el arranque y el rearme. La selección extraída se puede probar sin Explorer. Los predicados de versión existentes siguen determinando Windows 11 y el caso exacto de 22000, aun si falla la lectura de la revisión. El tipo detectado sigue siendo el único criterio que habilita XAML Diagnostics.

**Consecuencia.** El worker conserva el manejo de reintentos, el tratamiento especial de ventanas en barras mixtas y el comportamiento de la UI. Al cambiar el PID de Explorer o el tipo detectado emite un diagnóstico con la política de verbosidad existente.

## ADR-004. Ventana de configuración con WinUI 2.8

**Decisión.** Hospedar `SettingsPage` en las XAML Islands existentes con WinUI 2.8. Conservar el modelo de ventanas, los recursos, el selector de color y el empaquetado de GlassTB.

| Criterio | WinUI 2.8 y XAML Islands | WinUI 3 y Windows App SDK |
| --- | --- | --- |
| Memoria | Reutiliza el runtime que GlassTB carga para la bandeja y las ventanas XAML. | Introduce un runtime distinto. El modo autónomo sin paquete puede usar más memoria al compartir menos páginas de código. El impacto en GlassTB requiere medición. |
| Windows 10 | Mantiene la ruta que usa actualmente el MSIX de GlassTB. | Hay soporte en Windows 10 según la versión del SDK. La compatibilidad de la app, la bandeja y el host tendría que revalidarse. |
| Portable | Conserva el arranque y las dependencias actuales del portable en Windows 11. | Hay que distribuir o inicializar el runtime de Windows App SDK y validar el modo empaquetado y el autónomo por arquitectura. |
| Esfuerzo | Usa `Application::CreateXamlWindow` y los modelos WinRT existentes. | Exige adaptar la ventana, el ciclo de vida, los controles y el empaquetado, con pruebas de migración. |

**Decisión sobre Mica.** La ventana usa los recursos de tema del host y el tema claro u oscuro de Windows. Mica queda aplazada hasta validar su integración con el host Win32 y los fallbacks de Windows 10.

**Recomendación.** Seguir con WinUI 2.8 para las siguientes secciones mientras cumpla sus requisitos. Evaluar WinUI 3 en una prueba separada que mida memoria y arranque y verifique Windows 10 y 11, MSIX y portable antes de una migración.

[Implementación de Windows App SDK](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/deploy-overview)
