param(
    [switch]$StartNow,
    [string]$UserSid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
)
$ErrorActionPreference='Stop'
if ($UserSid -notmatch '^S-1-5-\d+(?:-\d+)+$') { throw 'Invalid user SID.' }
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=[Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$PSCommandPath+'"'),'-UserSid',$UserSid)
    if ($StartNow) { $arguments+='-StartNow' }
    $installer=Start-Process -FilePath (Get-Process -Id $PID).Path -Verb RunAs -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $installer.WaitForExit()
    if ($installer.ExitCode -ne 0) { throw "Autostart installer failed: $($installer.ExitCode)" }
    return
}
$app=Join-Path $PSScriptRoot 'libinput-scroll.exe'
if (-not (Test-Path -LiteralPath $app -PathType Leaf)) { throw 'Application executable not found.' }
$taskName='LibinputASCE1200Scroll'
$service=New-Object -ComObject 'Schedule.Service'
$service.Connect()
$folder=$service.GetFolder('\')
foreach ($existing in $folder.GetTasks(1)) {
    if ($existing.Name -eq $taskName) {
        $actions=@($existing.Definition.Actions)
        if ($actions.Count -ne 1 -or $actions[0].Path -ne $app -or $actions[0].Arguments -ne '--supervise') { throw 'Task name belongs to another application.' }
    }
}
$definition=$service.NewTask(0)
$definition.RegistrationInfo.Description='ASCE1200 libinput scrolling for this interactive user, including administrator windows.'
$definition.RegistrationInfo.Author=$UserSid
$definition.Principal.UserId=$UserSid
$definition.Principal.LogonType=3 # TASK_LOGON_INTERACTIVE_TOKEN; no stored password.
$definition.Principal.RunLevel=1 # TASK_RUNLEVEL_HIGHEST.
$trigger=$definition.Triggers.Create(9) # TASK_TRIGGER_LOGON.
$trigger.UserId=$UserSid
$trigger.Enabled=$true
$definition.Settings.Enabled=$true
$definition.Settings.AllowDemandStart=$true
$definition.Settings.StartWhenAvailable=$true
$definition.Settings.DisallowStartIfOnBatteries=$false
$definition.Settings.StopIfGoingOnBatteries=$false
$definition.Settings.ExecutionTimeLimit='PT0S'
$definition.Settings.MultipleInstances=2 # TASK_INSTANCES_IGNORE_NEW.
$definition.Settings.Priority=4 # Normal, not Task Scheduler's BelowNormal default.
$action=$definition.Actions.Create(0)
$action.Path=$app
$action.Arguments='--supervise'
$action.WorkingDirectory=$PSScriptRoot
$task=$folder.RegisterTaskDefinition($taskName,$definition,6,$UserSid,$null,3,$null)
$registeredUser=$task.Definition.Principal.UserId
$registeredSid=if ($registeredUser -like 'S-1-*') { $registeredUser } else { [Security.Principal.NTAccount]::new($registeredUser).Translate([Security.Principal.SecurityIdentifier]).Value }
if ($task.Definition.Principal.RunLevel -ne 1 -or $registeredSid -ne $UserSid) { throw 'Elevated autostart verification failed.' }
$task.Xml | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'autostart-task.xml') -Encoding utf8
# Remove the old ordinary Run entry only after elevated registration succeeds.
$runKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
if ($identity.User.Value -eq $UserSid -and (Get-ItemProperty -LiteralPath $runKey -Name $taskName -ErrorAction SilentlyContinue)) {
    Remove-ItemProperty -LiteralPath $runKey -Name $taskName
}
if ($StartNow) { [void]$task.Run($null) }
'Elevated autostart enabled for the current interactive user.'
