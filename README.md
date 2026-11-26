# GlassTB

GlassTB personaliza la barra de tareas de Windows (transparencia, acrílico, color y más) de forma local,
sin publicidad, sin telemetría y sin cuentas. Es un fork de [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB)
y conserva su motor probado, ampliándolo con perfiles, automatización y una nueva ventana de configuración.

> **Estado:** en desarrollo. Todo compila y pasa las pruebas automáticas en x64 y ARM64, pero todavía **no se ha
> verificado en una barra de tareas real**. Consulta la [matriz de compatibilidad](docs/glasstb/compatibilidad.md).

## Funciones

- **Materiales**: estilo de Windows, opaco, transparente, desenfoque y acrílico, con color y opacidad; apariencias distintas
  para escritorio, ventana visible, ventana maximizada, Inicio, Búsqueda, Vista de tareas y ahorro de batería.
- **Ventana de configuración** (bandeja → *Abrir GlassTB*): Inicio (diagnóstico), Apariencia (vista previa en vivo,
  aplicar/cancelar/restablecer), Perfiles, Pantallas, Color adaptativo, Automatización, Atajos y Rendimiento.
- **Perfiles**: 9 predefinidos (Invisible, Glass, Frosted, Midnight, OLED, Cyberpunk, Gamer, Productividad, Windows clásico),
  propios, importación/exportación JSON y selector en la bandeja.
- **Pantallas**: un perfil distinto por monitor.
- **Automatización** por eventos: aplicación en primer plano o en ejecución, pantalla completa, horario, días, batería,
  tema oscuro, número de pantallas; operadores Y/O/NO y prioridades. [Guía](docs/glasstb/automatizacion.md)
- **Color adaptativo** tomado del fondo de pantalla. [Detalles](docs/glasstb/color-adaptativo.md)
- **Atajos globales** configurables. [Detalles](docs/glasstb/atajos.md)
- **Estabilidad**: modo seguro (`--safe-mode`, Shift o bucle de cierres), pausa de efectos, copias de seguridad y
  recuperación de configuraciones dañadas. [Detalles](docs/glasstb/recuperacion.md)

Efectos que requieren modificar la geometría del árbol XAML de Explorer (degradados, neón, Liquid Glass, barra flotante o
segmentada) **no están implementados**: ver [riesgos](docs/glasstb/riesgos.md) y la matriz de compatibilidad.

## Datos

Todo se guarda en `%LOCALAPPDATA%\GlassTB` (`config.json`, `profiles\`, `rules.json`, `backups\`, `logs\`).
La primera vez se copia la configuración de TranslucentTB si existe. [Persistencia](docs/glasstb/persistencia.md)

## Instalar y compilar

- Descarga el ZIP portable desde *Releases* (o los artefactos de *Actions*) y verifica `SHA256SUMS.txt`.
  [Distribución](docs/glasstb/distribucion.md)
- Compilar desde el código: [instrucciones](docs/glasstb/compilacion.md).

## Documentación técnica

[Decisiones de arquitectura](docs/glasstb/decisiones.md) · [Riesgos de integración con Windows](docs/glasstb/riesgos.md) ·
[Compatibilidad](docs/glasstb/compatibilidad.md) · [Registro de cambios](CHANGELOG.md)

## Créditos y licencia

Gracias a [Ethan Smith (@ethanhs)](https://github.com/ethanhs), [@sylveon](https://github.com/sylveon),
[@MrAksel](https://github.com/MrAksel), [@denosawr](https://github.com/denosawr) y a todas las
[personas que contribuyeron a TranslucentTB](https://github.com/TranslucentTB/TranslucentTB/graphs/contributors).
El icono original es de [@dAKirby309](https://github.com/dAKirby309).

GlassTB se distribuye bajo la [GNU General Public License, versión 3](LICENSE.md). El [aviso de derivación](NOTICE.md)
identifica las modificaciones del fork.
