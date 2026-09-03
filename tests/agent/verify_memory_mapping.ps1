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

function Assert-AddressFailure {
    param(
        [object]$Response,
        [string]$ExpectedCause
    )

    Assert-Condition ($null -ne $Response.error) ("Expected ADDRESS_NOT_MAPPED for {0}" -f $ExpectedCause)
    Assert-Condition ($Response.error.data.reason -eq 'ADDRESS_NOT_MAPPED') ("Unexpected reason for {0}: {1}" -f $ExpectedCause, $Response.error.data.reason)
    Assert-Condition ($Response.error.data.cause -eq $ExpectedCause) ("Unexpected cause: {0}" -f $Response.error.data.cause)
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
        param([string]$Request)

        $writer.WriteLine($Request)
        $writer.Flush()
        $line = $reader.ReadLine()
        if ($null -eq $line) {
            throw 'Named pipe closed before a response was received'
        }
        return $line | ConvertFrom-Json
    }

    $start = Invoke-AgentRpc '{"jsonrpc":"2.0","id":"start","method":"session.start","params":{"target":{"command":"AGPMODE.COM","arguments":[]},"mounts":[{"drive":"C","host_path":"tests/agent/runtime"}],"break_at":"entry"}}'
    Assert-Condition ($null -eq $start.error) ('session.start failed: ' + ($start | ConvertTo-Json -Compress -Depth 10))
    $sessionId = $start.result.session_id
    $entrySegment = $start.result.stop_reason.address.segment

    # pm_paged is offset 0x191 in the COM image. DEBUGBOX's entry CS makes it a physical breakpoint before the far jump.
    $breakpoint = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"breakpoint","method":"breakpoints.create","params":{"session_id":"' + $sessionId + '","kind":"execution","address":{"space":"segmented","segment":"' + $entrySegment + '","offset":"0x00000191"}}}')
    Assert-Condition ($null -eq $breakpoint.error) ('breakpoints.create failed: ' + ($breakpoint | ConvertTo-Json -Compress -Depth 10))
    $continue = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"continue","method":"execution.continue","params":{"session_id":"' + $sessionId + '"}}')
    Assert-Condition ($null -eq $continue.error) ('execution.continue failed: ' + ($continue | ConvertTo-Json -Compress -Depth 10))
    $wait = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"wait","method":"execution.wait","params":{"session_id":"' + $sessionId + '","operation_id":"' + $continue.result.operation_id + '","timeout_ms":10000}}')
    Assert-Condition ($wait.result.stop_reason.kind -eq 'breakpoint') ('Protected-mode stop did not reach its breakpoint: ' + ($wait | ConvertTo-Json -Compress -Depth 10))

    $registers = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"registers","method":"state.get_registers","params":{"session_id":"' + $sessionId + '"}}')
    Assert-Condition ($registers.result.cpu_mode -eq 'protected') ('Expected protected mode: ' + ($registers | ConvertTo-Json -Compress -Depth 10))
    Assert-Condition ($registers.result.segments.cs -eq '0x0008') ('Unexpected protected-mode CS: ' + $registers.result.segments.cs)

    foreach ($mappedAddress in @(
        @{ id = 'mapped-segmented'; value = '{"space":"segmented","segment":"0x0010","offset":"0x00000000"}' },
        @{ id = 'mapped-linear'; value = '{"space":"linear","offset":"0x00000000"}' },
        @{ id = 'mapped-physical'; value = '{"space":"physical","offset":"0x00000000"}' }
    )) {
        $read = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"' + $mappedAddress.id + '","method":"memory.read","params":{"session_id":"' + $sessionId + '","address":' + $mappedAddress.value + ',"length":1}}')
        Assert-Condition ($null -eq $read.error) ('Expected mapped byte: ' + ($read | ConvertTo-Json -Compress -Depth 10))
        Assert-Condition ($read.result.byte_count -eq 1) 'Mapped memory read returned an unexpected byte count'
    }

    $invalidSelector = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"invalid-selector","method":"memory.read","params":{"session_id":"' + $sessionId + '","address":{"space":"segmented","segment":"0x0020","offset":"0x00000000"},"length":1}}')
    Assert-AddressFailure $invalidSelector 'invalid_selector'
    $segmentLimit = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"segment-limit","method":"memory.read","params":{"session_id":"' + $sessionId + '","address":{"space":"segmented","segment":"0x0018","offset":"0x00000010"},"length":1}}')
    Assert-AddressFailure $segmentLimit 'segment_limit'
    $pageNotPresent = Invoke-AgentRpc ('{"jsonrpc":"2.0","id":"page-not-present","method":"memory.read","params":{"session_id":"' + $sessionId + '","address":{"space":"linear","offset":"0x00400000"},"length":1}}')
    Assert-AddressFailure $pageNotPresent 'page_not_present'

    Write-Output 'RPC-C05 passed: protected-mode address spaces and failure causes are verified.'
} finally {
    if ($null -ne $writer) { $writer.Dispose() }
    if ($null -ne $reader) { $reader.Dispose() }
    if ($null -ne $pipe) { $pipe.Dispose() }
    if ($null -ne $process) {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
        $process.WaitForExit()
    }
}
