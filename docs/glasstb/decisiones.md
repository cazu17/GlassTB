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
