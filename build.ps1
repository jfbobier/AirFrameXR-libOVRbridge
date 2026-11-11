<#
Builds the bridge DLL (LibOVRRT64_1.dll) and the launcher (libovr_launch.exe) into .\dist.

  powershell -ExecutionPolicy Bypass -File build.ps1            # release flavour (default)
  powershell -ExecutionPolicy Bypass -File build.ps1 -Debug     # keeps the per-frame debug trace

Needs Visual Studio 2022 with the C++ desktop workload (MSBuild, cl) and git. The shim is built inside a checkout of VirtualDesktop-OpenXR
(OVRNull, the null libovr runtime it grows from) at the pinned commit; the checkout, its LibOVR headers submodule and the WIL NuGet package are
fetched into .\build on the first run. -VdxrDir uses a checkout you already have (it must be at the pinned commit).
#>
param([switch]$Debug, [string]$VdxrDir = '')
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$pin = '1a83fec8b5c565b14b06ffa8e1eb7e4768057573'   # mbucchia/VirtualDesktop-OpenXR
$work = "$here\build"; $dist = "$here\dist"
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path $msbuild)) {
  $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
  if ($vs) { $msbuild = $vs } else { throw 'MSBuild (Visual Studio 2022, C++ workload) not found' }
}
New-Item -ItemType Directory -Force $work, $dist | Out-Null

# ---- the VirtualDesktop-OpenXR checkout (only OVRNull and external\LibOVR are used)
if ($VdxrDir) { $vdxr = $VdxrDir } else {
  $vdxr = "$work\VDXR"
  if (-not (Test-Path "$vdxr\OVRNull")) {
    cmd /c "git clone --quiet https://github.com/mbucchia/VirtualDesktop-OpenXR.git `"$vdxr`" 2>&1" | Out-Null
    cmd /c "git -C `"$vdxr`" checkout --quiet $pin 2>&1" | Out-Null
    cmd /c "git -C `"$vdxr`" submodule update --init external/LibOVR 2>&1" | Out-Null
  }
}
if (-not (Test-Path "$vdxr\external\LibOVR\Include\OVR_CAPI.h")) { throw "LibOVR headers missing in $vdxr\external\LibOVR" }

# ---- WIL (NuGet), the one package OVRNull needs
$nuget = (Get-Command nuget.exe -ErrorAction SilentlyContinue).Source
if (-not $nuget) { $nuget = "$work\nuget.exe"; if (-not (Test-Path $nuget)) { Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile $nuget } }
& $nuget restore "$vdxr\OVRNull\packages.config" -PackagesDirectory "$vdxr\packages" -NonInteractive | Out-Null

# ---- OVRNull + our sources (shim\ overrides files of the same name) -> OVRShim project
$dst = "$vdxr\OVRShim"
New-Item -ItemType Directory -Force $dst | Out-Null
robocopy "$vdxr\OVRNull" $dst /E /XD bin obj /XF OVRNull.vcxproj OVRNull.vcxproj.user OVRNull.vcxproj.filters /NFL /NDL /NJH /NJS /NP | Out-Null
Copy-Item "$here\shim\*" $dst -Force -Recurse
# api.cpp implements ovr_SubmitControllerVibration and driver.cpp the audio endpoint strings: drop OVRNull's stubs of those functions
$stubs = Get-Content "$dst\stubs.cpp" -Raw
foreach ($fn in 'ovr_GetAudioDeviceOutGuidStr', 'ovr_GetAudioDeviceInGuidStr', 'ovr_SubmitControllerVibration') {
  $re = 'OVR_PUBLIC_FUNCTION\(ovrResult\)\s*' + $fn + '\(.*?\r?\n\}\r?\n'
  if (-not [regex]::IsMatch($stubs, $re, 'Singleline')) { throw "cannot find $fn in OVRNull\stubs.cpp" }
  $stubs = [regex]::Replace($stubs, $re, '', 'Singleline')
}
Set-Content "$dst\stubs.cpp" $stubs -Encoding UTF8 -NoNewline
$proj = Get-Content "$vdxr\OVRNull\OVRNull.vcxproj" -Raw
$fxVs = [regex]::Match($proj, '<FxCompile Include="ReprojectVS.hlsl">.*?</FxCompile>', 'Singleline').Value
$fxPs = [regex]::Match($proj, '<FxCompile Include="ReprojectPS.hlsl">.*?</FxCompile>', 'Singleline').Value
if (-not $fxVs -or -not $fxPs) { throw 'cannot find the FxCompile items in OVRNull.vcxproj' }
$extra = $fxVs.Replace('ReprojectVS.hlsl', 'LayerVS.hlsl') + "`n    " + $fxPs.Replace('ReprojectPS.hlsl', 'LayerPS.hlsl') + "`n    " + $fxPs.Replace('ReprojectPS.hlsl', 'LayerCubePS.hlsl')
$proj = $proj.Replace($fxPs, $fxPs + "`n    " + $extra)
$proj = $proj.Replace('$(SolutionDir)\bin\', '$(SolutionDir)\bin-shim\').Replace('<ClInclude Include="driver.h" />', '<ClInclude Include="driver.h" /><ClInclude Include="ipc.h" /><ClInclude Include="ipc_win.h" />')
Set-Content "$dst\OVRShim.vcxproj" $proj -Encoding UTF8

$buildStart = Get-Date
Push-Location $vdxr
$env:CL = if ($Debug) { '' } else { '/DAIRFRAMEXR_RELEASE' }   # the release flavour compiles the per-frame telemetry out
(Get-Item "$dst\driver.cpp").LastWriteTime = Get-Date
& $msbuild 'OVRShim\OVRShim.vcxproj' /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=$vdxr\\" /m /v:minimal /nologo 2>&1 | Out-File -Encoding utf8 "$work\shim_build.log"
Pop-Location
$dll = "$vdxr\bin-shim\x64\Release\LibOVRRT64_1.dll"
if (-not (Test-Path $dll) -or (Get-Item $dll).LastWriteTime -lt $buildStart) { Get-Content "$work\shim_build.log" | Select-String 'error' | Select-Object -First 20; throw 'the shim did not build (see build\shim_build.log)' }
New-Item -ItemType Directory -Force "$dist\bin" | Out-Null
Copy-Item $dll "$dist\bin\LibOVRRT64_1.dll" -Force

# ---- the launcher (one file, MSVC)
$vc = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$vcvars = "$vc\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw 'vcvars64.bat not found' }
cmd /c "`"$vcvars`" >nul && cl /nologo /O2 /MT /EHsc /W3 /Fo:`"$work\\`" /Fe:`"$dist\libovr_launch.exe`" `"$here\launcher\libovr_launch.cpp`" /link /SUBSYSTEM:CONSOLE" 2>&1 | Out-File -Encoding utf8 "$work\launcher_build.log"
if (-not (Test-Path "$dist\libovr_launch.exe")) { Get-Content "$work\launcher_build.log" | Select-Object -Last 15; throw 'the launcher did not build' }
Get-ChildItem $dist -Recurse -File | Select-Object FullName, Length | Format-Table -AutoSize
Write-Host "flavour: $(if ($Debug) { 'debug' } else { 'release' })"
