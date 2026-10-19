param([string]$PackageDirectory = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
$package = [IO.Path]::GetFullPath($PackageDirectory)
$logDirectory = if ($env:APPDATA) { Join-Path $env:APPDATA 'Sunshine\logs' } else { Join-Path $package 'logs' }
$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$staging = Join-Path $temporaryRoot ('sunshine-logs-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($staging)

function Copy-ReportFile([string]$Source, [string]$Destination) {
    if (Test-Path -LiteralPath $Source -PathType Leaf) {
        $inputFile = [IO.File]::Open($Source, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
        try {
            $outputFile = [IO.File]::Create((Join-Path $Destination ([IO.Path]::GetFileName($Source))))
            try { $inputFile.CopyTo($outputFile) } finally { $outputFile.Dispose() }
        } finally { $inputFile.Dispose() }
    }
}

try {
    if (Test-Path -LiteralPath $logDirectory -PathType Container) {
        $recent = Get-ChildItem -LiteralPath $logDirectory -File |
            Where-Object { $_.Name -match '^session-\d{8}-\d{6}-\d{3}-\d+\.log$' } |
            Sort-Object Name -Descending | Select-Object -First 5
        foreach ($session in $recent) {
            Copy-ReportFile $session.FullName $staging
            Copy-ReportFile ([IO.Path]::ChangeExtension($session.FullName, '.crash.log')) $staging
        }
    }
    # Keep explicit play_logged/shell captures compatible, without recursively collecting user folders.
    foreach ($name in @('sunshine.log', 'crash.log')) {
        Copy-ReportFile (Join-Path (Get-Location).Path $name) $staging
    }
    $settings = if ($env:APPDATA) { Join-Path $env:APPDATA 'Sunshine\settings.ini' } else { Join-Path $package 'settings.ini' }
    Copy-ReportFile $settings $staging
    $hardware = @('Windows: ' + [Environment]::OSVersion.VersionString)
    try {
        $hardware += 'CPU: ' + (Get-CimInstance Win32_Processor).Name
        $hardware += Get-CimInstance Win32_VideoController | ForEach-Object { 'GPU: ' + $_.Name + ' driver ' + $_.DriverVersion }
    } catch { $hardware += 'Hardware query unavailable: ' + $_.Exception.Message }
    $executable = Join-Path $package 'sunshine.exe'
    if (Test-Path -LiteralPath $executable -PathType Leaf) {
        $algorithm = [Security.Cryptography.SHA256]::Create()
        $image = [IO.File]::OpenRead($executable)
        try { $hash = [BitConverter]::ToString($algorithm.ComputeHash($image)).Replace('-', '') }
        finally { $image.Dispose(); $algorithm.Dispose() }
        $hardware += 'Collected package executable SHA256: ' + $hash
    }
    $hardware | Set-Content -LiteralPath (Join-Path $staging 'hardware.txt') -Encoding UTF8
    $reports = if ($env:APPDATA) { Join-Path $env:APPDATA 'Sunshine' } else { $package }
    [void][IO.Directory]::CreateDirectory($reports)
    $archive = Join-Path $reports 'sunshine-logs.zip'
    [void][Reflection.Assembly]::LoadWithPartialName('System.IO.Compression.FileSystem')
    $temporaryZip = $staging + '.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($staging, $temporaryZip)
    Copy-Item -LiteralPath $temporaryZip -Destination $archive -Force
    Write-Output "Saved $archive - attach it to the bug report."
} finally {
    # Only remove this invocation's exact GUID directory inside the resolved temporary root.
    $resolved = [IO.Path]::GetFullPath($staging)
    if ([IO.Path]::GetDirectoryName($resolved) -eq $temporaryRoot.TrimEnd('\') -and
        [IO.Path]::GetFileName($resolved) -match '^sunshine-logs-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $resolved -Recurse -Force
        if (Test-Path -LiteralPath ($resolved + '.zip') -PathType Leaf) {
            Remove-Item -LiteralPath ($resolved + '.zip') -Force
        }
    }
}
