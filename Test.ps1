param([string]$Trace,[switch]$Ui)
$ErrorActionPreference='Stop'
Push-Location -LiteralPath $PSScriptRoot
try {
    $data=Join-Path $PSScriptRoot 'build\test-data'
    New-Item -ItemType Directory -Path $data -Force | Out-Null
    foreach ($source in 'scroll_tests.cpp','gesture_handoff_tests.cpp','app_profiles_tests.cpp','touchscreen_output_tests.cpp','interaction_tests.cpp','pointer_acceleration_tests.cpp','pointer_output_tests.cpp') {
        $name=[IO.Path]::GetFileNameWithoutExtension($source)
        & (Join-Path $PSScriptRoot 'build.ps1') -Name $name -Source $source
        $testArguments=@()
        if ($source -eq 'app_profiles_tests.cpp') { $testArguments=@($data) }
        elseif ($Trace -and $source -in 'scroll_tests.cpp','gesture_handoff_tests.cpp') { $testArguments=@($Trace) }
        & (Join-Path $PSScriptRoot ($name+'.exe')) @testArguments
        if ($LASTEXITCODE -ne 0) { throw "$name failed: $LASTEXITCODE" }
    }
    if ($Ui) {
        & (Join-Path $PSScriptRoot 'build.ps1')
        $fixture=Join-Path $data 'ui.ini'
        $picture=Join-Path $data 'settings.png'
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gestures.example.ini') -Destination $fixture -Force
        $arguments='--settings-self-test "'+$fixture+'" "'+$picture+'"'
        $process=Start-Process -FilePath (Join-Path $PSScriptRoot 'libinput-scroll.exe') -ArgumentList $arguments -WindowStyle Normal -PassThru -Wait
        if ($process.ExitCode -ne 0) { throw "Settings UI test failed: $($process.ExitCode)" }
        'Settings UI save, validation and application picker passed. Captures: '+$picture
    }
} finally { Pop-Location }
