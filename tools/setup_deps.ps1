<#
.SYNOPSIS
    Downloads and prepares the runtime dependencies of Control_GP.

.DESCRIPTION
    Reads deps.lock and requirements.txt from the repository root and fills:
        python-embed/   embeddable Python + packages from requirements.txt
        poppler/        Poppler binaries (used by pdf2image)

    Safe to run repeatedly: parts that are already in the right version are
    skipped. Changing requirements.txt re-runs only the pip install.
    Downloads are cached in .deps-cache/ and verified against SHA256 from
    deps.lock when a hash is given.

.PARAMETER Force
    Delete and re-create every selected component from scratch.

.PARAMETER Only
    Limit the run to some components: python, poppler.

.PARAMETER TesseractDir
    Tesseract installation to check (not installed by this script).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup_deps.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup_deps.ps1 -Only python -Force
#>
[CmdletBinding()]
param(
    [switch]$Force,
    [ValidateSet('python', 'poppler')]
    [string[]]$Only,
    [string]$TesseractDir = 'C:\Program Files\Tesseract-OCR'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$env:PYTHONNOUSERSITE = '1'                # keep the user's site-packages out of python-embed's pip
$ProgressPreference = 'SilentlyContinue'   # makes Invoke-WebRequest much faster in PS 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$RepoRoot = Split-Path -Parent $PSScriptRoot
$LockFile = Join-Path $RepoRoot 'deps.lock'
$ReqFile  = Join-Path $RepoRoot 'requirements.txt'
$CacheDir = Join-Path $RepoRoot '.deps-cache'

# Modules imported as a smoke test after pip install (import names, not package names)
$SmokeImports = @('pypdf', 'pdf2image', 'PIL', 'onnxruntime', 'cv2', 'numpy', 'requests')

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }
function Write-Ok([string]$Message)   { Write-Host "    $Message" -ForegroundColor Green }
function Write-Info([string]$Message) { Write-Host "    $Message" -ForegroundColor Gray }

# Safe property access (works with StrictMode and missing keys)
function Get-Prop($Obj, [string]$Name) {
    if ($null -eq $Obj) { return $null }
    $p = $Obj.PSObject.Properties[$Name]
    if ($p) { return $p.Value }
    return $null
}

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed (exit code $LASTEXITCODE): $Exe $($Arguments -join ' ')"
    }
}

function Get-Download([string]$Url, [string]$Sha256) {
    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    $fileName = [IO.Path]::GetFileName(([Uri]$Url).AbsolutePath)
    $target = Join-Path $CacheDir $fileName

    if (Test-Path -LiteralPath $target) {
        if (-not $Sha256) {
            Write-Info "Using cached $fileName"
            return $target
        }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $Sha256.ToUpperInvariant()) {
            Write-Info "Using cached $fileName (hash OK)"
            return $target
        }
        Write-Info "Cached $fileName has a wrong hash, downloading again"
        Remove-Item -LiteralPath $target -Force
    }

    Write-Info "Downloading $Url"
    $part = "$target.part"
    Invoke-WebRequest -Uri $Url -OutFile $part -UseBasicParsing
    Move-Item -LiteralPath $part -Destination $target -Force

    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($Sha256) {
        if ($hash -ne $Sha256.ToUpperInvariant()) {
            Remove-Item -LiteralPath $target -Force
            throw "SHA256 mismatch for $fileName`n  expected: $Sha256`n  actual:   $hash"
        }
        Write-Info "SHA256 OK"
    }
    else {
        Write-Warning "No sha256 for $fileName in deps.lock. Computed hash: $hash  -> add it to deps.lock."
    }
    return $target
}

# Extracts a zip into $Destination. If the zip contains a single top-level
# folder (e.g. poppler-24.08.0/), its contents are moved up one level.
function Expand-Flat([string]$Zip, [string]$Destination) {
    $tmp = Join-Path $CacheDir ('extract-' + [guid]::NewGuid().ToString('N'))
    Expand-Archive -LiteralPath $Zip -DestinationPath $tmp -Force

    $items = @(Get-ChildItem -LiteralPath $tmp -Force)
    $src = $tmp
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) { $src = $items[0].FullName }

    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    New-Item -ItemType Directory -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $src -Force | Move-Item -Destination $Destination
    Remove-Item -LiteralPath $tmp -Recurse -Force
}

function Read-Stamp([string]$Dir) {
    $f = Join-Path $Dir '.installed.json'
    if (Test-Path -LiteralPath $f) { return Get-Content -LiteralPath $f -Raw | ConvertFrom-Json }
    return $null
}

function Write-Stamp([string]$Dir, [hashtable]$Data) {
    $Data | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Dir '.installed.json') -Encoding UTF8
}

# The embeddable Python ignores site-packages by default. This edits
# python3XX._pth so that pip and installed packages work.
function Enable-SitePackages([string]$Dir) {
    $pth = Get-ChildItem -LiteralPath $Dir -Filter 'python*._pth' | Select-Object -First 1
    if (-not $pth) { throw "No python*._pth file found in $Dir" }

    $lines = @(Get-Content -LiteralPath $pth.FullName | Where-Object {
        ($_ -notmatch '^\s*#?\s*import site\s*$') -and ($_ -ne 'Lib\site-packages')
    })
    $lines += 'Lib\site-packages'
    $lines += 'import site'
    Set-Content -LiteralPath $pth.FullName -Value $lines -Encoding ascii
    Write-Info "Enabled site-packages in $($pth.Name)"
}

# ---------------------------------------------------------------------------
# Components
# ---------------------------------------------------------------------------

function Install-Python {
    $entry = Get-Prop $Lock 'python'
    if (-not $entry) { throw "deps.lock has no 'python' entry." }

    $version = Get-Prop $entry 'version'
    if (-not $version) { throw "deps.lock: python.version is empty." }
    $url = Get-Prop $entry 'url'
    if (-not $url) { $url = "https://www.python.org/ftp/python/$version/python-$version-embed-amd64.zip" }

    $dest    = Join-Path $RepoRoot 'python-embed'
    $pyExe   = Join-Path $dest 'python.exe'
    $reqHash = (Get-FileHash -LiteralPath $ReqFile -Algorithm SHA256).Hash
    $stamp   = Read-Stamp $dest

    $needBase = $Force -or -not (Test-Path -LiteralPath $pyExe) -or ((Get-Prop $stamp 'version') -ne $version)
    $needPkgs = $needBase -or ((Get-Prop $stamp 'requirements') -ne $reqHash)

    Write-Step "Python $version"
    if (-not $needPkgs) {
        Write-Ok 'Up to date.'
        return
    }

    if ($needBase) {
        $zip = Get-Download $url (Get-Prop $entry 'sha256')
        Expand-Flat $zip $dest
        Enable-SitePackages $dest

        Write-Info 'Installing pip'
        $getPip = Join-Path $CacheDir 'get-pip.py'
        Invoke-WebRequest -Uri 'https://bootstrap.pypa.io/get-pip.py' -OutFile $getPip -UseBasicParsing
        Invoke-Checked $pyExe @($getPip, '--no-warn-script-location')
    }

    Write-Info 'Installing packages from requirements.txt'
    Invoke-Checked $pyExe @('-m', 'pip', 'install', '--no-warn-script-location',
                            '--disable-pip-version-check', '-r', $ReqFile)
    Invoke-Checked $pyExe @('-m', 'pip', 'check')
    Invoke-Checked $pyExe @('-c', "import $($SmokeImports -join ', '); print('    imports OK')")

    Write-Stamp $dest @{ version = $version; requirements = $reqHash }
    Write-Ok "Python $version ready in python-embed\"
}

# Generic "download a zip and extract it" component (poppler)
function Install-Archive([string]$Name, [string]$CheckFile) {
    $entry   = Get-Prop $Lock $Name
    $url     = Get-Prop $entry 'url'
    $version = Get-Prop $entry 'version'

    Write-Step "$Name $version"
    if (-not $url) {
        Write-Info "Not configured in deps.lock, skipping."
        return
    }

    $dest  = Join-Path $RepoRoot $Name
    $stamp = Read-Stamp $dest
    if (-not $Force -and (Test-Path -LiteralPath $dest) -and ((Get-Prop $stamp 'version') -eq $version)) {
        Write-Ok 'Up to date.'
        return
    }

    $zip = Get-Download $url (Get-Prop $entry 'sha256')
    Expand-Flat $zip $dest

    if ($CheckFile) {
        $found = Get-ChildItem -LiteralPath $dest -Recurse -Filter $CheckFile | Select-Object -First 1
        if (-not $found) { throw "$CheckFile not found after extracting $Name." }
        $rel = $found.DirectoryName.Substring($RepoRoot.Length).TrimStart('\')
        Write-Info "Binaries: $rel"
    }

    Write-Stamp $dest @{ version = $version }
    Write-Ok "$Name $version ready in $Name\"
}

# ---------------------------------------------------------------------------
# Checks of things this script does not install
# ---------------------------------------------------------------------------

function Test-Tesseract {
    Write-Step 'Tesseract (check only)'
    $dll = Join-Path $TesseractDir 'libtesseract-5.dll'
    $ces = Join-Path $TesseractDir 'tessdata\ces.traineddata'
    if (-not (Test-Path -LiteralPath $dll)) {
        Write-Warning "Tesseract not found in $TesseractDir. Install the UB Mannheim build (with Czech) or pass -TesseractDir."
    }
    elseif (-not (Test-Path -LiteralPath $ces)) {
        Write-Warning "Czech language data missing: $ces"
    }
    else {
        Write-Ok 'Found, including Czech language data.'
    }
}

# CMake builds against a full Python install; its major.minor must match python-embed.
function Test-BuildPython {
    $version = Get-Prop (Get-Prop $Lock 'python') 'version'
    if (-not $version) { return }
    $mm = (($version -split '\.')[0..1]) -join '.'

    Write-Step "Build Python $mm (check only)"
    $found = $null
    try {
        $found = & py "-$mm" -c "import sys; print('%d.%d' % sys.version_info[:2])" 2>$null
    } catch { }
    if ($LASTEXITCODE -eq 0 -and $found -eq $mm) {
        Write-Ok "Python $mm is installed for building."
    }
    else {
        Write-Warning "Python $mm (full install with headers/libs) not found via the py launcher. CMake needs it to build Control_GP."
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if (-not (Test-Path -LiteralPath $LockFile)) { throw "deps.lock not found in $RepoRoot" }
if (-not (Test-Path -LiteralPath $ReqFile))  { throw "requirements.txt not found in $RepoRoot" }

$Lock = Get-Content -LiteralPath $LockFile -Raw | ConvertFrom-Json
$components = if ($Only) { $Only } else { @('python', 'poppler') }

foreach ($c in $components) {
    switch ($c) {
        'python'  { Install-Python }
        'poppler' { Install-Archive 'poppler' 'pdftoppm.exe' }
        default   { Install-Archive $c }
    }
}

Test-Tesseract
Test-BuildPython

Write-Host ''
Write-Host 'Done. You can now configure and build Control_GP with CMake / Qt Creator.' -ForegroundColor Green
