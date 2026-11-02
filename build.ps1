param([string]$Name='libinput-scroll',[string]$Source='touchpad.cpp')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ tools are required.' }
$vc = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$kitRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdk = Get-ChildItem -LiteralPath (Join-Path $kitRoot 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$compiler = 'C:\Program Files\LLVM\bin\clang-cl.exe'
$resource = $null
if ($Source -eq 'touchpad.cpp') {
    $resource = Join-Path $buildDir "$Name.res"
    $rc = Join-Path $kitRoot "bin\$($sdk.Name)\arm64\rc.exe"
    Push-Location -LiteralPath $projectRoot
    try {
        & $rc /nologo "/fo$resource" (Join-Path $projectRoot 'app.rc')
        if ($LASTEXITCODE -ne 0) { throw "Resource compilation failed: $LASTEXITCODE" }
    } finally { Pop-Location }
}
$buildArgs = @('/nologo','/std:c++20','/O2','/MT','/EHsc','/W4','/DUNICODE','/D_UNICODE','/DNOMINMAX','/D_WIN32_WINNT=0x0A00',"/I$($vc.FullName)\include")
foreach ($part in 'ucrt','shared','um','winrt') { $buildArgs += "/I$($sdk.FullName)\$part" }
$buildArgs += @((Join-Path $projectRoot $Source),"/Fe$projectRoot\$Name.exe","/Fo$buildDir\$Name.obj",'/link','/machine:arm64',"/libpath:$($vc.FullName)\lib\arm64", "/libpath:$kitRoot\Lib\$($sdk.Name)\ucrt\arm64", "/libpath:$kitRoot\Lib\$($sdk.Name)\um\arm64",'user32.lib','hid.lib','setupapi.lib','advapi32.lib','shell32.lib')
if ($Source -eq 'touchpad.cpp') { $buildArgs += @('/subsystem:windows','/entry:wmainCRTStartup',$resource) }
& $compiler @buildArgs
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
