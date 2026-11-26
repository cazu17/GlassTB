param(
    [string] $BuildType
)

$ErrorActionPreference = "Stop"

$appx_file = "$PSScriptRoot\..\..\AppPackage\Package.appxmanifest"
$appx_content = Get-Content $appx_file
if ($BuildType -eq "Release")
{
	$appx_content = $appx_content.Replace("GlassTB (Dev)", "GlassTB")
}
elseif ($BuildType -eq "Canary")
{
	$appx_content = $appx_content.Replace("GlassTB (Dev)", "GlassTB (Canary)")
}

Set-Content $appx_file -Value $appx_content
