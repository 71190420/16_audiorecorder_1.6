$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFile = Join-Path $projectRoot "16_audiorecorder.pro"
$projectText = Get-Content -LiteralPath $projectFile -Raw

$projectEntries = [regex]::Matches(
    $projectText,
    '(?m)^\s*([A-Za-z0-9_./-]+\.(?:cpp|h|ui|qrc))\s*\\?\s*$'
) | ForEach-Object { $_.Groups[1].Value }

[xml]$resources = Get-Content -LiteralPath (Join-Path $projectRoot "res.qrc") -Raw
$resourceEntries = $resources.RCC.qresource.file | ForEach-Object { [string]$_ }

foreach ($entry in @($projectEntries) + @($resourceEntries)) {
    $path = Join-Path $projectRoot $entry
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing project file: $entry"
    }
}

foreach ($form in $projectEntries | Where-Object { $_ -like '*.ui' }) {
    [xml](Get-Content -LiteralPath (Join-Path $projectRoot $form) -Raw) | Out-Null
}

$mainText = Get-Content -LiteralPath (Join-Path $projectRoot "main.cpp") -Raw
if ($mainText -match 'ui_testwidget\.h') {
    throw "main.cpp still references the removed generated header."
}

$recorderText = Get-Content -LiteralPath (Join-Path $projectRoot "audiorecorder.cpp") -Raw
$serialConnections = [regex]::Matches(
    $recorderText,
    'connect\s*\(\s*serialPort\s*,\s*&QSerialPort::readyRead'
).Count
if ($serialConnections -ne 1) {
    throw "Expected one serial readyRead connection, found $serialConnections."
}

foreach ($command in @('led2_on', 'led2_off')) {
    if ($recorderText -notmatch $command) {
        throw "Missing hardware frequency command: $command"
    }
}
if ($recorderText -notmatch 'QTimer::singleShot\(0, this, &AudioRecorder::recorderBtClicked\)') {
    throw "The application does not automatically enter listening mode."
}
if ($recorderText -notmatch 'm_liveSpectrumPlot->graph\(0\)->setData') {
    throw "The main frequency spectrum is not connected to sample data."
}
if ($recorderText -notmatch 'xAxis->setRange\(100, 1000\)') {
    throw "The main frequency axis is not configured for 100-1000 Hz."
}
if ($recorderText -match 'progressBar\[1\]->setValue') {
    throw "The live signal path still updates a separate right-channel meter."
}
$analysisText = Get-Content -LiteralPath (Join-Path $projectRoot "newwindow.cpp") -Raw
if ($analysisText -match 'butterworthBandpassFilter') {
    throw "Software band-pass filtering is still present in the live analysis module."
}

$clang = Get-Command clang++ -ErrorAction SilentlyContinue
$node = Get-Command node -ErrorAction SilentlyContinue
if ($clang -and $node) {
    $wasmPath = Join-Path ([IO.Path]::GetTempPath()) "signalquality_test_$PID.wasm"
    try {
        & $clang.Source --target=wasm32-unknown-unknown -std=c++11 -nostdlib -fno-builtin `
            '-Wl,--no-entry' '-Wl,--export=runTests' `
            (Join-Path $projectRoot "signalquality_test.cpp") -o $wasmPath
        if ($LASTEXITCODE -ne 0) { throw "Signal-quality test compilation failed." }

        & $node.Source -e "const fs=require('fs'); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>process.exit(instance.exports.runTests()));" $wasmPath
        if ($LASTEXITCODE -ne 0) { throw "Signal-quality behavior test failed." }
    } finally {
        if (Test-Path -LiteralPath $wasmPath) {
            Remove-Item -LiteralPath $wasmPath -Force
        }
    }
}

Write-Host "Project structure verification passed." -ForegroundColor Green
