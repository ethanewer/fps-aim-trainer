param(
    [switch]$Clean
)

. "$PSScriptRoot\windows-common.ps1"

Build-WindowsApp -Clean:$Clean
Write-Host "Built $Script:ExePath"
