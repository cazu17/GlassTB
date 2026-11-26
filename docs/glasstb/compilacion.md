# Compilar GlassTB

## Requisitos (Windows 10/11, x64 o ARM64)

- Visual Studio 2026 (toolset **v145**) con las cargas de trabajo *Desarrollo para el escritorio con C++* y
  *Desarrollo de aplicaciones para la Plataforma universal de Windows* (ver `.vsconfig`).
- Windows SDK 10.0.28000 (lo instala NuGet: `Microsoft.Windows.SDK.CPP*`).
- vcpkg integrado (`vcpkg integrate install`); las dependencias se declaran en cada `vcpkg.json` (wil, spdlog,
  rapidjson, detours, member-thunk, gtest) con los puertos propios de `vcpkg/ports`.

## Línea de comandos

```powershell
msbuild TranslucentTB.slnx -t:Restore -p:RestorePackagesConfig=true -p:Configuration=Release -p:Platform=x64
msbuild TranslucentTB.slnx -p:Configuration=Release -p:Platform=x64 -p:BuildType=Release -p:SkipSigning=True -p:AppxPackageSigningEnabled=false
x64\Release\Tests.exe
```

No uses `-m` (compilación paralela de proyectos): el empaquetado MSIX consulta el ejecutable mientras se compila y MSBuild
informa `MSB4006`. `BuildType` puede ser `Release`, `Canary` o `Dev` (cambia el nombre visible).

## Versión portable

```powershell
./.azp/scripts/create-portable.ps1 -Platform x64 -Configuration Release -WorkDir $env:TEMP -OutputDir dist -NoConfig -Compress
```

Genera `dist\GlassTB-portable-x64.zip`. Requiere Windows 11 y los paquetes de marco `Microsoft.VCLibs.140.00` y
`Microsoft.UI.Xaml.2.8` (instalados normalmente con Windows o Microsoft Store).

## Integración continua

- `.github/workflows/build.yml`: cada push y PR compila x64 y ARM64 (Release, sin firma), ejecuta `Tests.exe` en x64 y
  publica los portables como artefactos.
- `.github/workflows/release.yml`: al publicar una etiqueta `v*` crea un *release* en borrador con los ZIP y `SHA256SUMS.txt`.
