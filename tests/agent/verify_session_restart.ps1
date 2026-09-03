[CmdletBinding()]
param(
    [string]$DosboxExecutable = (Join-Path $PSScriptRoot 'runtime\dosbox-x.exe'),
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'agent-test.env'),
    [int]$ConnectTimeoutMs = 15000
)

$ErrorActionPreference = 'Stop'

function Assert-Condition {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (!$Condition) {
        throw $Message
    }
}

Assert-Condition (Test-Path -LiteralPath $DosboxExecutable -PathType Leaf) "DOSBox-X executable was not found: $DosboxExecutable"
Assert-Condition (Test-Path -LiteralPath $ConfigPath -PathType Leaf) "Agent config was not found: $ConfigPath"

& (Join-Path $PSScriptRoot 'build_fixture.ps1')
if (-not $? -or ($LASTEXITCODE -and $LASTEXITCODE -ne 0)) {
    throw 'Fixture generation failed'
}

$process = $null
$pipe = $null
$writer = $null
$reader = $null
try {
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $process = Start-Process -FilePath (Resolve-Path $DosboxExecutable).Path `
        -ArgumentList @('--agent-config', (Resolve-Path $ConfigPath).Path) `
        -WorkingDirectory $repositoryRoot -WindowStyle Hidden -PassThru

    $pipe = [System.IO.Pipes.NamedPipeClientStream]::new('.', 'dosbox-agent-test', [System.IO.Pipes.PipeDirection]::InOut, [System.IO.Pipes.PipeOptions]::None)
    $pipe.Connect($ConnectTimeoutMs)
    $writer = [System.IO.StreamWriter]::new($pipe, [System.Text.UTF8Encoding]::new($false), 4096, $true)
    $writer.NewLine = "`n"
    $reader = [System.IO.StreamReader]::new($pipe, [System.Text.UTF8Encoding]::new($false), $false, 4096, $true)

    function Invoke-AgentRpc {
        param([object]$Request)

        $writer.WriteLine(($Request | ConvertTo-Json -Compress -Depth 10))
        $writer.Flush()
        $line = $reader.ReadLine()
        if ($null -eq $line) {
            throw 'Named pipe closed before a response was received'
        }
        return $line | ConvertFrom-Json
    }

    function Start-And-StopFixture {
        param([string]$Suffix)

        $start = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "restart-start-$Suffix"
            method = 'session.start'
            params = @{
                target = @{ command = 'AGENTFIX.COM'; arguments = @() }
                mounts = @(@{ drive = 'C'; host_path = 'tests/agent/runtime' })
                break_at = 'entry'
            }
        }
        Assert-Condition ($null -eq $start.error) ("session.start $Suffix failed: " + ($start | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($start.result.state -eq 'stopped') ("session.start $Suffix did not stop: " + ($start | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($start.result.stop_reason.kind -eq 'startup') ("session.start $Suffix did not stop at entry: " + ($start | ConvertTo-Json -Compress -Depth 10))

        $sessionId = $start.result.session_id
        $stop = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "restart-stop-$Suffix"
            method = 'session.stop'
            params = @{ session_id = $sessionId; graceful_timeout_ms = 0 }
        }
        Assert-Condition ($null -eq $stop.error) ("session.stop $Suffix failed: " + ($stop | ConvertTo-Json -Compress -Depth 10))

        $wait = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "restart-wait-$Suffix"
            method = 'execution.wait'
            params = @{ session_id = $sessionId; operation_id = $stop.result.operation_id; timeout_ms = 10000 }
        }
        Assert-Condition ($null -eq $wait.error) ("execution.wait $Suffix failed: " + ($wait | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($wait.result.state -eq 'exited') ("session.stop $Suffix did not exit: " + ($wait | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($wait.result.stop_reason.kind -eq 'program_exit') ("session.stop $Suffix had unexpected stop reason: " + ($wait | ConvertTo-Json -Compress -Depth 10))
    }

    Start-And-StopFixture '1'
    Start-And-StopFixture '2'
    Write-Output 'Session restart passed: one DOSBox-X process completed two entry-stop/exit cycles.'
} finally {
    if ($null -ne $writer) { $writer.Dispose() }
    if ($null -ne $reader) { $reader.Dispose() }
    if ($null -ne $pipe) { $pipe.Dispose() }
    if ($null -ne $process) {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.WaitForExit()
    }
}
