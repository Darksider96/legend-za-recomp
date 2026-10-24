param(
    [string]$GhidraHome = $env:GHIDRA_INSTALL_DIR,
    [string]$ProjectPath = (Join-Path $PSScriptRoot 'project'),
    [string]$ProjectName = 'PokemonZA',
    [string]$ProgramName = 'main',
    [string]$Manifest = (Join-Path $PSScriptRoot 'phase4-main-functions.txt'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'phase4-reproduced'),
    [string]$ExpectedProgramSha256 = '056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720'
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($GhidraHome)) {
    throw 'Informe -GhidraHome ou configure GHIDRA_INSTALL_DIR.'
}
$headless = Join-Path $GhidraHome 'support\analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $headless -PathType Leaf)) { throw "Ghidra ausente: $headless" }
if (-not (Test-Path -LiteralPath (Join-Path $ProjectPath ($ProjectName + '.gpr')) -PathType Leaf)) {
    throw 'Use um projeto Ghidra existente com o NSO importado pelo SwitchLoader.'
}
$selectors = @(
    Get-Content -LiteralPath $Manifest | ForEach-Object {
        $item = ($_ -split '#', 2)[0].Trim()
        if ($item) {
            if ($item -notmatch '^[0-9A-Fa-f]+:[0-9A-Fa-f]+$') { throw "Seletor inválido: $item" }
            $item
        }
    }
)
if ($selectors.Count -eq 0) { throw 'Manifesto vazio.' }
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
$headlessArgs = @(
    [IO.Path]::GetFullPath($ProjectPath), $ProjectName,
    '-process', $ProgramName, '-readOnly', '-noanalysis',
    '-scriptPath', $PSScriptRoot,
    '-postScript', 'Phase4Analyze.java', $outputPath,
    ('sha256:' + $ExpectedProgramSha256)
) + $selectors
# Read-only project transaction; no target execution and no SDK installation.
& $headless @headlessArgs *> (Join-Path $outputPath 'headless-console.txt')
if ($LASTEXITCODE -ne 0) { throw "Ghidra saiu com código $LASTEXITCODE; veja headless-console.txt." }
if (-not (Test-Path -LiteralPath (Join-Path $outputPath 'COMPLETE.txt'))) {
    throw 'O coletor não concluiu; consulte o log (o launcher pode retornar 0 mesmo com erro de script).'
}
Write-Output "Evidências estáticas salvas em $outputPath"
