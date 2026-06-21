. "$PSScriptRoot\windows-common.ps1"

Build-WindowsApp

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NativeIcon {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool DestroyIcon(IntPtr hIcon);
}
'@

function New-AimTrainerIcon([string]$Path) {
    $bmp = New-Object System.Drawing.Bitmap 256, 256
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::FromArgb(18, 20, 24))
    $ringPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(34, 197, 94)), 12
    $ringPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $ringPen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawEllipse($ringPen, 50, 50, 156, 156)
    $thinPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(226, 232, 240)), 8
    $g.DrawEllipse($thinPen, 83, 83, 90, 90)
    $crossPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(248, 250, 252)), 10
    $g.DrawLine($crossPen, 128, 22, 128, 78)
    $g.DrawLine($crossPen, 128, 178, 128, 234)
    $g.DrawLine($crossPen, 22, 128, 78, 128)
    $g.DrawLine($crossPen, 178, 128, 234, 128)
    $dotBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(239, 68, 68))
    $g.FillEllipse($dotBrush, 113, 113, 30, 30)
    $hicon = $bmp.GetHicon()
    try {
        $icon = [System.Drawing.Icon]::FromHandle($hicon)
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
        try {
            $icon.Save($fs)
        } finally {
            $fs.Dispose()
            $icon.Dispose()
        }
    } finally {
        [NativeIcon]::DestroyIcon($hicon) | Out-Null
        $dotBrush.Dispose()
        $crossPen.Dispose()
        $thinPen.Dispose()
        $ringPen.Dispose()
        $g.Dispose()
        $bmp.Dispose()
    }
}

$iconPath = Join-Path $Script:BuildDir "Aim Trainer.ico"
if (-not (Test-Path -LiteralPath $iconPath)) {
    New-AimTrainerIcon $iconPath
}

$desktop = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktop "Aim Trainer.lnk"
$powershell = Join-Path $PSHOME "powershell.exe"
$runScript = Join-Path $PSScriptRoot "windows-run.ps1"

$wsh = New-Object -ComObject WScript.Shell
$shortcut = $wsh.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $powershell
$shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$runScript`""
$shortcut.WorkingDirectory = $Script:RepoRoot
$shortcut.IconLocation = "$iconPath,0"
$shortcut.Description = "Build if needed and launch Aim Trainer"
$shortcut.WindowStyle = 7
$shortcut.Save()

Write-Host "Installed desktop shortcut: $shortcutPath"
