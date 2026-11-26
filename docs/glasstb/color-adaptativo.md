# Color adaptativo

Configuración → **Color adaptativo**. Se guarda en `config.json` bajo `adaptive_color`:

```json
"adaptive_color": { "enabled": true, "use_secondary": false, "intensity": 160, "saturation": 100, "contrast": 50, "excluded_wallpapers": ["foto.jpg"] }
```

## Cómo funciona

1. Lee la ruta del fondo con `SystemParametersInfo(SPI_GETDESKWALLPAPER)`. Si es un color sólido usa `COLOR_BACKGROUND`.
2. Decodifica la imagen una vez con WIC y la reduce a 64×64 píxeles (coste acotado, sin capturas de pantalla).
3. Cuantiza a 4096 grupos (4 bits por canal) ponderando la saturación y penalizando negros, blancos y grises,
   y elige el **dominante** y un **secundario** con al menos 40° de diferencia de tono (o 30 % de luminosidad).
4. Ajusta el color al tema: oscuro → luminosidad ≤ 0,45 (texto blanco legible); claro → ≥ 0,60. *Contraste* acerca
   el color al extremo del tema, *saturación* lo multiplica e *intensidad* es la opacidad del tinte.
5. Sustituye el color de los siete estados **en memoria** (los materiales se conservan). Las reglas de automatización se
   aplican encima.

## Cuándo se recalcula

Solo con `WM_SETTINGCHANGE` de fondo de pantalla (`SPI_SETDESKWALLPAPER`), cambio de tema (`ImmersiveColorSet`) o al cambiar
estos ajustes. No hay temporizadores ni sondeo. La presentación de diapositivas de Windows emite el mismo aviso al cambiar de imagen.

## Limitaciones conocidas

- Se usa el fondo principal; los fondos distintos por pantalla (`IDesktopWallpaper`) no se distinguen todavía.
- Las transiciones animadas entre colores no están implementadas (requerirían actualizar la barra varias veces por segundo).
- Formatos: los que WIC sepa decodificar en el equipo (JPEG, PNG, BMP, GIF, TIFF, JPEG XR; HEIF/WebP si los códecs están instalados).
