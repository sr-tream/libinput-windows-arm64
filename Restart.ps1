$ErrorActionPreference='Stop'
$app=Join-Path $PSScriptRoot 'libinput-scroll.exe'
$stop=Start-Process -FilePath $app -ArgumentList '--stop' -WindowStyle Hidden -PassThru
$stop.WaitForExit()
if ($stop.ExitCode -ne 0) { throw 'Could not signal the running scroll handler.' }
try {
    $service=New-Object -ComObject 'Schedule.Service'
    $service.Connect()
    $task=$service.GetFolder('\').GetTask('LibinputASCE1200Scroll')
    $until=[DateTime]::UtcNow.AddSeconds(10)
    while ($task.State -eq 4 -and [DateTime]::UtcNow -lt $until) { Start-Sleep -Milliseconds 100 }
    if ($task.State -eq 4) { throw 'The previous task has not stopped yet.' }
    [void]$task.Run($null)
} catch {
    Start-Sleep -Milliseconds 700
    # The ordinary launcher requests UAC if no elevated task is available.
    Start-Process -FilePath $app -ArgumentList '--supervise' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden
}
