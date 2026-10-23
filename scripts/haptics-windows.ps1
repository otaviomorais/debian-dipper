param([ValidateSet('Test','Save','Restore')][string]$Mode = 'Test')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$adb = Join-Path $PSScriptRoot 'tools\windows\adb.exe'
$fastboot = Join-Path $PSScriptRoot 'tools\windows\fastboot.exe'
$images = Join-Path $PSScriptRoot 'backup-vibracao'
$originalHash = 'a17e3ea8be5697744e37841e3c5a7da521eca6d7ca23ee45eac6e5b1b1f6c98c'
$patchedHash = '294a5472fc7d73d1549639849f6136d1bc9e94f7116947892160ca7cbfddeaac'

function Adb-Check([string[]]$Arguments) {
    & $adb @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Falha no ADB. Confira a conexao USB e o diagnostico exibido.' }
}
function Check-Image([string]$Name, [string]$Expected) {
    $path = Join-Path $images $Name
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Expected) {
        throw "Arquivo ausente ou checksum incorreto: $Name. Execute primeiro testar-vibracao.bat."
    }
}
function Read-Tool([string]$File, [string]$Arguments) {
    # Capture fastboot stderr without PowerShell 5 treating normal getvar
    # output as a terminating NativeCommandError. FileName handles spaces.
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = $File
    $start.Arguments = $Arguments
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::Start($start)
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit(15000)) {
        $process.Kill()
        $process.Dispose()
        throw 'O comando USB excedeu o tempo de espera.'
    }
    $result = [PSCustomObject]@{ ExitCode = $process.ExitCode; Text = ($stdout.Result + $stderr.Result) }
    $process.Dispose()
    return $result
}
function Check-Fastboot {
    Write-Host 'Aguarde o fastboot e confira o driver USB do Windows.'
    $ready = $false
    for ($i=0; $i -lt 30; $i++) {
        $devices = @(& $fastboot devices | Where-Object { $_ -match '\sfastboot\s*$' })
        if ($devices.Count -eq 1) { $ready = $true; break }
        if ($devices.Count -gt 1) { throw 'Conecte somente o Mi 8.' }
        Start-Sleep -Seconds 2
    }
    if (!$ready) { throw 'Fastboot nao encontrado. Confira o driver. Segure Power para voltar ao Debian.' }
    # fastboot writes successful getvar results on stderr.
    $product = Read-Tool $fastboot 'getvar product'
    if ($product.ExitCode -ne 0 -or $product.Text -notmatch 'product:\s*dipper\b') {
        throw 'O produto fastboot nao corresponde ao Xiaomi Mi 8 (dipper).'
    }
}

try {
    if (!(Test-Path $adb) -or !(Test-Path $fastboot)) { throw 'Extraia TODO o ZIP antes de executar.' }
    if ($Mode -ne 'Restore') { Adb-Check -Arguments @('get-state') }
    if ($Mode -eq 'Test') {
        Write-Host 'Teste temporario da vibracao. O aparelho sera reiniciado; salve seu trabalho.'
        if (Test-Path $images) {
            Check-Image 'boot-original.img' $originalHash
            Check-Image 'boot-vibracao.img' $patchedHash
        } else {
            $remote = '/tmp/dipper-haptics-' + [Guid]::NewGuid().ToString('N')
            Adb-Check -Arguments @('push','payload\prepare-haptics-boot.py','/tmp/dipper-prepare-haptics.py')
            Adb-Check -Arguments @('shell',"python3 /tmp/dipper-prepare-haptics.py --prepare-device --output $remote")
            New-Item -ItemType Directory -Path $images | Out-Null
            Adb-Check -Arguments @('pull',"$remote/boot-original.img",(Join-Path $images 'boot-original.img'))
            Adb-Check -Arguments @('pull',"$remote/boot-vibracao.img",(Join-Path $images 'boot-vibracao.img'))
            Adb-Check -Arguments @('pull',"$remote/SHA256SUMS",(Join-Path $images 'SHA256SUMS'))
            Check-Image 'boot-original.img' $originalHash
            Check-Image 'boot-vibracao.img' $patchedHash
        }
    } elseif ($Mode -eq 'Save') {
        Check-Image 'boot-original.img' $originalHash
        Check-Image 'boot-vibracao.img' $patchedHash
        Adb-Check -Arguments @('push','payload\prepare-haptics-boot.py','/tmp/dipper-prepare-haptics.py')
        Adb-Check -Arguments @('shell','modprobe qcom-spmi-haptics && python3 /tmp/dipper-prepare-haptics.py --verify-running')
        Write-Host 'Gravando o boot testado. Use este arquivo somente apos sentir a vibracao nas teclas.'
    } else {
        Check-Image 'boot-original.img' $originalHash
        Write-Host 'Restaurando o boot original salvo neste computador.'
    }
    if ($Mode -eq 'Restore') {
        # Recovery must also work with an unbootable Debian: enter fastboot
        # manually with Power + Volume Down and connect USB.
        $state = Read-Tool $adb 'get-state'
        if ($state.ExitCode -eq 0) { Adb-Check -Arguments @('reboot','bootloader') }
    } else {
        Adb-Check -Arguments @('reboot','bootloader')
    }
    Check-Fastboot
    if ($Mode -eq 'Test') {
        & $fastboot boot (Join-Path $images 'boot-vibracao.img')
        if ($LASTEXITCODE -ne 0) { throw 'Boot temporario recusado. Nenhuma particao foi gravada. Segure Power para voltar.' }
        Write-Host 'Aguardando o Debian iniciar por ate dois minutos...'
        $booted = $false
        for ($i=0; $i -lt 60; $i++) {
            $state = Read-Tool $adb 'get-state'
            if ($state.ExitCode -eq 0 -and $state.Text.Trim() -eq 'device') { $booted = $true; break }
            Start-Sleep -Seconds 2
        }
        if (!$booted) { throw 'Debian ainda nao respondeu por USB. Nada foi gravado; segure Power para voltar ao boot anterior.' }
        Adb-Check -Arguments @('push','payload\prepare-haptics-boot.py','/tmp/dipper-prepare-haptics.py')
        Adb-Check -Arguments @('shell','modprobe qcom-spmi-haptics; systemctl restart fbkeyboard; systemctl status fbkeyboard --no-pager -l; journalctl -b -u fbkeyboard --no-pager -n 20; python3 /tmp/dipper-prepare-haptics.py --verify-running && systemctl is-active --quiet fbkeyboard')
        Write-Host 'Driver reconhecido. Toque nas teclas. Reiniciar normalmente volta ao boot anterior.'
        Write-Host 'Se funcionar, execute salvar-vibracao.bat com o Debian ligado.'
    } else {
        $name = if ($Mode -eq 'Save') { 'boot-vibracao.img' } else { 'boot-original.img' }
        & $fastboot flash boot (Join-Path $images $name)
        if ($LASTEXITCODE -ne 0) { throw 'Falha ao gravar boot. Preserve backup-vibracao e o diagnostico.' }
        & $fastboot reboot
        if ($LASTEXITCODE -ne 0) { throw 'Boot gravado; falhou apenas o reinicio. Reinicie pelo Power.' }
        Write-Host 'Concluido. Preserve a pasta backup-vibracao para restauracao.'
    }
} catch {
    Write-Host "ERRO: $_" -ForegroundColor Red
    exit 1
}
