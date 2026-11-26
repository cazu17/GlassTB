# Compatibilidad de la barra de tareas

El worker conserva la detección de la barra principal de Explorer. Sin `Taskbar.dll` en el proceso identifica una barra `Classic`. Con la DLL cargada, dos islas XAML indican `Mixed` y una indica `XAML`. Si falla la enumeración de módulos o la cantidad de islas es distinta, el tipo queda como `Unknown` y se espera al siguiente aviso de la barra.

## Reglas de selección

Los tipos de esta tabla son ejemplos detectados para cada versión. Una actualización puede cambiar el tipo de barra sin cambiar el número de build base. El backend depende del tipo detectado, incluso en el caso de 22621.

| Windows (build y revisión) | Tipo detectado | Desenfoque de AccentPolicy | Servicio XAML | AccentPolicy clásica | Backend |
| --- | --- | --- | --- | --- | --- |
| 10 (19045) | Classic | Sí | No | Sí | AccentPolicy |
| 11 (22000.281 o anterior) | Mixed | No | No | Sí | Mixed |
| 11 (22000.282 o posterior dentro de 22000) | Mixed | Sí | No | Sí | Mixed |
| 11 (22000, revisión desconocida) | Mixed | No | No | Sí | Mixed |
| 11 (22621) | Mixed | No | No | Sí | Mixed |
| 11 (22621) | XAML | No | Sí | No | XamlTap |
| 11 (26100) | XAML | No | Sí | No | XamlTap |
| Cualquier build | Unknown | Según la versión | No | No | None |

La revisión 282 corresponde a la corrección de `AccentPolicy` en Windows 11 build 22000. El valor de desenfoque de AccentPolicy permanece deshabilitado en builds posteriores de Windows 11. La UI conserva su regla independiente que habilita el desenfoque cuando la barra es XAML. `Mixed` usa AccentPolicy y conserva el tratamiento de las ventanas interiores de la barra. `None` indica que la detección de la barra principal quedó pendiente.

## Matriz de verificación en Windows

Los estados describen pruebas con una barra de tareas de Windows real. La compilación de CI y los tests unitarios de selección se registran por separado.

| Escenario | Probado | Parcial | Experimental | No compatible | No verificado |
| --- | --- | --- | --- | --- | --- |
| Windows 10 19045, Classic | | | | | ✓ |
| Windows 11 22000 hasta .281, Mixed | | | | | ✓ |
| Windows 11 22000 desde .282, Mixed | | | | | ✓ |
| Windows 11 22621, Mixed | | | | | ✓ |
| Windows 11 22621, XAML | | | | | ✓ |
| Windows 11 26100, XAML | | | | | ✓ |
| Explorer aún no clasificado, Unknown | | | | | ✓ |

Para inspeccionar el diagnóstico, establece `"verbosity": "info"` en la configuración y vuelve a abrir GlassTB. Busca `Taskbar compatibility:` en el log al arrancar y tras reiniciar `explorer.exe`. La línea muestra versión, arquitecturas nativa y del proceso, tipo de barra, capacidades y backend. Compara escritorio, ventana maximizada y disponibilidad del desenfoque con la Fase 2A, usando la misma configuración.
