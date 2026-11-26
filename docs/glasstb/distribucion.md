# Distribución, firma y actualizaciones

| Formato | Estado | Notas |
| --- | --- | --- |
| ZIP portable (x64, ARM64) | Generado por CI y por el workflow de release | Sin firma digital. Verificar con `SHA256SUMS.txt`. |
| MSIX | El proyecto `AppPackage` compila el paquete | Para instalarlo hace falta firmarlo con un certificado cuyo *subject* coincida con `Publisher` (`CN=GlassTB Open Source Developers`). Sin certificado, no es instalable fuera del modo desarrollador. |

## Preparado para firma

`Common/CppProject.props` firma los binarios si se pasa `CertificateThumbprint` (certificado del almacén) o
`ArtifactSigningMetadata` (Azure Artifact Signing). Ejemplo:

```powershell
msbuild TranslucentTB.slnx -p:Configuration=Release -p:Platform=x64 -p:BuildType=Release -p:CertificateThumbprint=<huella> -p:CertificateTimestampServer=http://timestamp.digicert.com
```

## Actualizaciones verificadas

GlassTB **no descarga ni instala actualizaciones por sí mismo** (sin servicios externos ni telemetría). Las versiones se
publican en GitHub Releases con `SHA256SUMS.txt`; el usuario verifica antes de reemplazar el ZIP:

```powershell
Get-FileHash .\GlassTB-portable-x64.zip -Algorithm SHA256
```

La versión MSIX firmada se actualiza instalando el paquete nuevo (Windows valida la firma).

## Desinstalación limpia

- **MSIX**: *Configuración → Aplicaciones* elimina el paquete; la DLL de ExplorerTAP pide a Explorer reiniciarse al desinstalar
  (`KillExplorerWhenPackageUninstalls`), y Windows borra la carpeta de datos virtualizada.
- **Portable**: salir desde la bandeja (restaura la barra), borrar la carpeta del ZIP y, si se desea, `%LOCALAPPDATA%\GlassTB`.
  Si se activó el inicio automático en la versión portable mediante un acceso directo en `shell:startup`, borrarlo.
- GlassTB no escribe en el registro salvo lo que haga el propio Windows para la tarea de inicio del MSIX.
