. "$PSScriptRoot\windows-common.ps1"

Build-WindowsApp -Clean

$env:PATH = "$Script:UcrtBin;$env:PATH"
& $Script:ExePath --self-test
if ($LASTEXITCODE -ne 0) {
    throw "Self-test failed with exit code $LASTEXITCODE"
}

$debugBmp = Join-Path $Script:BuildDir "debug-menu.bmp"
$debug = Start-Process -FilePath $Script:ExePath -ArgumentList @("--debug-menu", $debugBmp, "1280", "720") -WorkingDirectory $Script:BuildDir -Wait -PassThru
if ($debug.ExitCode -ne 0) {
    throw "Debug menu render failed with exit code $($debug.ExitCode)"
}
if (-not (Test-Path -LiteralPath $debugBmp)) {
    throw "Debug menu render did not create $debugBmp"
}
Remove-Item -LiteralPath $debugBmp -Force
Write-Host "Windows build, self-test, and debug render passed."
