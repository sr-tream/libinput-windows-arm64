$ErrorActionPreference = 'Stop'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=[Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $installer=Start-Process -FilePath (Get-Process -Id $PID).Path -Verb RunAs -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"')) -WindowStyle Hidden -PassThru
    $installer.WaitForExit()
    if ($installer.ExitCode -ne 0) { throw "Autostart removal failed: $($installer.ExitCode)" }
    return
}
$service=New-Object -ComObject 'Schedule.Service'
$service.Connect()
$folder=$service.GetFolder('\')
foreach ($task in $folder.GetTasks(1)) {
    if ($task.Name -eq 'LibinputASCE1200Scroll') {
        $actions=@($task.Definition.Actions)
        if ($actions.Count -ne 1 -or $actions[0].Path -ne (Join-Path $PSScriptRoot 'libinput-scroll.exe')) { throw 'Task name belongs to another application.' }
        $folder.DeleteTask('LibinputASCE1200Scroll',0)
    }
}
$runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$valueName = 'LibinputASCE1200Scroll'
if (Get-ItemProperty -LiteralPath $runKey -Name $valueName -ErrorAction SilentlyContinue) {
    Remove-ItemProperty -LiteralPath $runKey -Name $valueName
}
'Autostart disabled. Use Stop.cmd to stop the current session.'
