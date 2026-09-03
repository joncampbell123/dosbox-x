[CmdletBinding()]
param(
    [string]$DosboxExecutable = (Join-Path $PSScriptRoot 'runtime\dosbox-x.exe'),
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'agent-test.env'),
    [string]$MountPath = 'tests/agent/runtime',
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
if ($LASTEXITCODE -ne 0) {
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

    function Start-FixtureSession {
        param([string]$Suffix)

        $response = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "start-$Suffix"
            method = 'session.start'
            params = @{
                target = @{ command = 'AGENTFIX.COM'; arguments = @() }
                mounts = @(@{ drive = 'C'; host_path = $MountPath })
                break_at = 'entry'
            }
        }
        Assert-Condition ($null -eq $response.error) ("session.start failed: " + ($response | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($response.result.state -eq 'stopped') 'session.start did not stop at entry'
        return $response.result.session_id
    }

    function Stop-FixtureSession {
        param(
            [string]$SessionId,
            [string]$Suffix
        )

        $stop = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "stop-$Suffix"
            method = 'session.stop'
            params = @{ session_id = $SessionId; graceful_timeout_ms = 0 }
        }
        Assert-Condition ($null -eq $stop.error) ("session.stop failed: " + ($stop | ConvertTo-Json -Compress -Depth 10))

        $wait = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "stop-wait-$Suffix"
            method = 'execution.wait'
            params = @{ session_id = $SessionId; operation_id = $stop.result.operation_id; timeout_ms = 10000 }
        }
        Assert-Condition ($null -eq $wait.error) ("session.stop wait failed: " + ($wait | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($wait.result.state -eq 'exited') 'session.stop did not exit the target'
    }

    function Run-Trace {
        param(
            [string]$SessionId,
            [int]$InstructionCount,
            [string]$Suffix
        )

        $start = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "trace-start-$Suffix"
            method = 'trace.start'
            params = @{ session_id = $SessionId; detail = 'normal'; instruction_count = $InstructionCount }
        }
        Assert-Condition ($null -eq $start.error) ("trace.start failed: " + ($start | ConvertTo-Json -Compress -Depth 10))

        $continue = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "trace-continue-$Suffix"
            method = 'execution.continue'
            params = @{ session_id = $SessionId }
        }
        Assert-Condition ($null -eq $continue.error) ("execution.continue failed: " + ($continue | ConvertTo-Json -Compress -Depth 10))

        $wait = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "trace-wait-$Suffix"
            method = 'execution.wait'
            params = @{ session_id = $SessionId; operation_id = $continue.result.operation_id; timeout_ms = 10000 }
        }
        Assert-Condition ($null -eq $wait.error) ("execution.wait failed: " + ($wait | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($wait.result.state -eq 'stopped') 'trace completion did not stop the target'

        $cursor = $null
        $events = @()
        do {
            $read = Invoke-AgentRpc @{
                jsonrpc = '2.0'
                id = "trace-read-$Suffix-$($events.Count)"
                method = 'trace.read'
                params = @{ session_id = $SessionId; cursor = $cursor; limit = 1 }
            }
            Assert-Condition ($null -eq $read.error) ("trace.read failed: " + ($read | ConvertTo-Json -Compress -Depth 10))
            $events += @($read.result.events)
            $cursor = $read.result.next_cursor
        } while ($null -ne $cursor)

        Assert-Condition ($events.Count -eq $InstructionCount) ("trace returned {0} events, expected {1}" -f $events.Count, $InstructionCount)
        for ($index = 0; $index -lt $events.Count; ++$index) {
            $event = $events[$index]
            Assert-Condition ($event.sequence -eq ($index + 1)) 'trace sequence was not session-local and contiguous'
            Assert-Condition ($event.address.space -eq 'segmented') 'trace address did not retain its address space'
            Assert-Condition (![string]::IsNullOrWhiteSpace($event.address.segment)) 'trace address segment was empty'
            Assert-Condition (![string]::IsNullOrWhiteSpace($event.address.offset)) 'trace address offset was empty'
            Assert-Condition (![string]::IsNullOrWhiteSpace($event.instruction)) 'trace instruction was empty'
            Assert-Condition ($null -ne $event.register_changes) 'trace register_changes was missing'
        }

        $stop = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "trace-stop-$Suffix"
            method = 'trace.stop'
            params = @{ session_id = $SessionId }
        }
        Assert-Condition ($null -eq $stop.error) ("trace.stop failed: " + ($stop | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($stop.result.event_count -eq $InstructionCount) 'trace.stop reported an unexpected event count'
        return $events
    }

    $sessionId = Start-FixtureSession 'd'
    foreach ($command in @('HELP', 'CPU', 'PIC')) {
        $response = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "command-$command"
            method = 'debugger.execute_command'
            params = @{ session_id = $sessionId; command = $command }
        }
        Assert-Condition ($null -eq $response.error) ("$command was rejected: " + ($response | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($response.result.accepted -eq $true) "$command was not accepted"
        Assert-Condition (![string]::IsNullOrWhiteSpace($response.result.raw_output)) "$command returned empty raw_output"
    }

    $outputCursor = $null
    $outputRecords = @()
    do {
        $output = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "output-read-$($outputRecords.Count)"
            method = 'debug.output.read'
            params = @{ session_id = $sessionId; cursor = $outputCursor; limit = 1 }
        }
        Assert-Condition ($null -eq $output.error) ("debug.output.read failed: " + ($output | ConvertTo-Json -Compress -Depth 10))
        $outputRecords += @($output.result.output)
        $outputCursor = $output.result.next_cursor
    } while ($null -ne $outputCursor)

    Assert-Condition ($outputRecords.Count -ge 3) 'debug.output.read did not retain all command output records'
    for ($index = 0; $index -lt $outputRecords.Count; ++$index) {
        $record = $outputRecords[$index]
        Assert-Condition ($record.sequence -eq ($index + 1)) 'output sequence was not contiguous across pages'
        Assert-Condition (![string]::IsNullOrWhiteSpace($record.source)) 'output source was empty'
        Assert-Condition (![string]::IsNullOrWhiteSpace($record.message)) 'output message was empty'
    }

    foreach ($command in @('RUN', 'SM', 'SR', 'BP 1234:0100')) {
        $rejected = Invoke-AgentRpc @{
            jsonrpc = '2.0'
            id = "rejected-$($command.Replace(' ', '-').Replace(':', '-'))"
            method = 'debugger.execute_command'
            params = @{ session_id = $sessionId; command = $command }
        }
        Assert-Condition ($null -ne $rejected.error) "$command was unexpectedly accepted"
        Assert-Condition ($rejected.error.data.reason -eq 'COMMAND_REJECTED') "$command did not return COMMAND_REJECTED"
    }

    $firstTrace = Run-Trace $sessionId 2 'first'
    Stop-FixtureSession $sessionId 'd'

    $secondSessionId = Start-FixtureSession 'isolation'
    $secondTrace = Run-Trace $secondSessionId 1 'second'
    Assert-Condition ($secondTrace.Count -eq 1) 'second session trace returned an unexpected event count'
    Assert-Condition ($secondTrace[0].sequence -eq 1) 'second session trace included events from the first session'
    Stop-FixtureSession $secondSessionId 'isolation'

    Write-Output 'RPC-D01 through RPC-D05 passed: output paging, command allowlist, parser fixtures, trace shape, and session isolation are verified.'
} finally {
    if ($null -ne $writer) { $writer.Dispose() }
    if ($null -ne $reader) { $reader.Dispose() }
    if ($null -ne $pipe) { $pipe.Dispose() }
    if ($null -ne $process) {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.WaitForExit()
    }
}
