param(
    [switch]$SkipBuildCheck
)

try {
    . "$PSScriptRoot\windows-common.ps1"

    if (-not $SkipBuildCheck) {
        if (-not (Test-WindowsBuildCurrent)) {
            Build-WindowsApp
        } else {
            Sync-WindowsRuntimeDlls
        }
    } elseif (-not (Test-Path -LiteralPath $Script:ExePath)) {
        Build-WindowsApp
    } else {
        Sync-WindowsRuntimeDlls
    }

    $env:PATH = "$Script:UcrtBin;$env:PATH"
    Start-Process -FilePath $Script:ExePath -WorkingDirectory $Script:BuildDir
} catch {
    $message = $_.Exception.Message + "`n`nRun .\scripts\windows-build.ps1 from PowerShell for full build output."
    try {
        Add-Type -AssemblyName System.Windows.Forms
        [System.Windows.Forms.MessageBox]::Show($message, "Aim Trainer launch failed", "OK", "Error") | Out-Null
    } catch {
        Write-Error $message
    }
    exit 1
}
