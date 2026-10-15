Set-StrictMode -Version Latest

function Get-MeasurementMedian([double[]]$Values) {
    if (!$Values.Count) { return $null }
    $ordered = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($ordered.Count / 2)
    if ($ordered.Count % 2) { return $ordered[$middle] }
    return ($ordered[$middle - 1] + $ordered[$middle]) / 2
}

function Get-MeasurementStability {
    param([double[]]$Values, [int]$ExpectedCount,
        [double]$MaxRelativeRange = 0.15, [double]$MaxRelativeDrift = 0.10)
    if ($ExpectedCount -lt 3 -or ![double]::IsFinite($MaxRelativeRange) -or
        ![double]::IsFinite($MaxRelativeDrift) -or $MaxRelativeRange -le 0 -or $MaxRelativeDrift -le 0) {
        throw 'Stability requires >=3 expected samples and positive limits.'
    }
    $reasons = [Collections.Generic.List[string]]::new()
    if ($Values.Count -ne $ExpectedCount) { $reasons.Add('incomplete') }
    if ($Values.Count -lt 3) { $reasons.Add('insufficient-samples') }
    foreach ($value in $Values) {
        if (![double]::IsFinite($value) -or $value -le 0) { $reasons.Add('invalid-time'); break }
    }
    $median = $null; $range = $null; $drift = $null; $minimum = $null; $maximum = $null
    if ($Values.Count -ge 3 -and !$reasons.Contains('invalid-time')) {
        $median = Get-MeasurementMedian $Values
        $minimum = ($Values | Measure-Object -Minimum).Minimum
        $maximum = ($Values | Measure-Object -Maximum).Maximum
        $range = ($maximum - $minimum) / $median
        $half = [int][Math]::Floor($Values.Count / 2)
        # Exclude the central sample for odd counts; keep chronological order.
        $first = Get-MeasurementMedian $Values[0..($half - 1)]
        $last = Get-MeasurementMedian $Values[($Values.Count - $half)..($Values.Count - 1)]
        $drift = [Math]::Abs($last - $first) / $median
        if ($range -gt $MaxRelativeRange) { $reasons.Add('range') }
        if ($drift -gt $MaxRelativeDrift) { $reasons.Add('drift') }
    }
    [pscustomobject]@{ Accepted=($reasons.Count -eq 0); Reasons=$reasons.ToArray()
        Samples=$Values.Count; Expected=$ExpectedCount; Median=$median; Min=$minimum; Max=$maximum
        RelativeRange=$range; RelativeDrift=$drift
        MaxRelativeRange=$MaxRelativeRange; MaxRelativeDrift=$MaxRelativeDrift }
}

function Get-MeasurementOrder([object[]]$Cases, [int]$Round) {
    if (!$Cases.Count -or $Round -lt 0) { throw 'Require cases and a nonnegative round.' }
    # Rotate first position and alternate direction; no speed-based reordering.
    $offset = [int][Math]::Floor($Round / 2) % $Cases.Count
    $order = @(for ($i = 0; $i -lt $Cases.Count; ++$i) { $Cases[($offset + $i) % $Cases.Count] })
    if ($Round % 2) { [array]::Reverse($order) }
    return $order
}

function Get-MeasurementArtifacts([string[]]$Paths) {
    foreach ($path in $Paths | Sort-Object -Unique) {
        $item = Get-Item -LiteralPath $path -ErrorAction Stop
        [pscustomobject]@{ Path=$item.FullName; Bytes=$item.Length
            Hash=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash }
    }
}

function Test-MeasurementArtifacts([object[]]$Before, [object[]]$After) {
    if ($Before.Count -ne $After.Count) { return $false }
    foreach ($item in $Before) {
        $matches = @($After | Where-Object { $_.Path -eq $item.Path -and $_.Hash -eq $item.Hash -and $_.Bytes -eq $item.Bytes })
        if ($matches.Count -ne 1) { return $false }
    }
    return $true
}

function Initialize-MeasurementWindows {
    if (!$IsWindows -or [IntPtr]::Size -ne 8) { throw 'Controlled timing requires 64-bit Windows PowerShell 7.' }
    if (!('WioMeasurementSystem' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WioMeasurementSystem
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetSystemTimes(out long idle, out long kernel, out long user);
    [DllImport("kernel32.dll")]
    public static extern ushort GetActiveProcessorGroupCount();
}
'@
    }
    if ([WioMeasurementSystem]::GetActiveProcessorGroupCount() -ne 1) {
        throw 'Multiple processor groups require an explicit group-aware controller; refusing ambiguous affinity.'
    }
}

function Enter-MeasurementAffinity([int]$LogicalProcessor = -1) {
    Initialize-MeasurementWindows
    $process = [Diagnostics.Process]::GetCurrentProcess()
    $original = $process.ProcessorAffinity.ToInt64()
    if ($LogicalProcessor -eq -1) {
        for ($i = 0; $i -lt 64; ++$i) {
            if (($original -band (1L -shl $i)) -ne 0) { $LogicalProcessor = $i; break }
        }
    }
    if ($LogicalProcessor -lt 0 -or $LogicalProcessor -gt 63) {
        $process.Dispose()
        throw 'LogicalProcessor must be an allowed index in [0,63].'
    }
    $mask = 1L -shl $LogicalProcessor
    if (($original -band $mask) -eq 0) {
        $process.Dispose()
        throw 'Requested processor is outside the controller affinity.'
    }
    # Windows children inherit affinity at creation, BEFORE any child instruction.
    # Restore on verification failure as well as on normal completion.
    try {
        $process.ProcessorAffinity = [IntPtr]::new($mask)
        $process.Refresh()
        if ($process.ProcessorAffinity.ToInt64() -ne $mask) { throw 'Controller affinity verification failed.' }
    } catch {
        $process.ProcessorAffinity = [IntPtr]::new($original)
        $process.Dispose()
        throw
    }
    [pscustomobject]@{ Process=$process; Original=$original; Mask=$mask; LogicalProcessor=$LogicalProcessor }
}

function Exit-MeasurementAffinity($Context) {
    try { $Context.Process.ProcessorAffinity = [IntPtr]::new($Context.Original) }
    finally { $Context.Process.Dispose() }
}

function Get-MeasurementSystemTimes {
    [long]$idle = 0; [long]$kernel = 0; [long]$user = 0
    if (![WioMeasurementSystem]::GetSystemTimes([ref]$idle, [ref]$kernel, [ref]$user)) {
        throw 'GetSystemTimes failed.'
    }
    [pscustomobject]@{ Idle=$idle; Total=($kernel + $user) }
}

function Get-MeasurementEnvironment {
    $power = (& powercfg /getactivescheme 2>&1 | Out-String).Trim()
    $processors = $null; $processorError = $null
    $processorName = [Microsoft.Win32.Registry]::GetValue(
        'HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0', 'ProcessorNameString', $null)
    try {
        $processors = @(Get-CimInstance Win32_Processor -ErrorAction Stop |
            Select-Object Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed, CurrentClockSpeed)
    } catch { $processorError = $_.Exception.Message }
    [pscustomobject]@{ Utc=[DateTime]::UtcNow.ToString('o'); OS=[Environment]::OSVersion.VersionString
        Processor=$env:PROCESSOR_IDENTIFIER; ProcessorName=$processorName
        Processors=$processors; ProcessorQueryError=$processorError
        PowerScheme=$power; PowerQueryExit=$LASTEXITCODE
        ClockCaveat='CIM nominal/snapshot clocks, when available, are NOT measured effective workload frequency.' }
}

function Invoke-MeasurementProcess {
    param([string]$Program, [string[]]$Arguments, [string]$WorkingDirectory,
        [int]$TimeoutSeconds, [string]$Label = 'benchmark')
    if ($TimeoutSeconds -lt 1) { throw 'Require a positive process timeout.' }
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Program; $info.WorkingDirectory = $WorkingDirectory
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $before = Get-MeasurementSystemTimes
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = $null; $output = $null; $errorOutput = $null
    $row = [pscustomobject]@{ Status='launch-failed'; ExitCode=$null; WallSeconds=$null
        CpuSeconds=$null; CpuToWall=$null; SystemBusyFraction=$null; Output=''; Error=''
        Utc=[DateTime]::UtcNow.ToString('o') }
    try {
        $process = [Diagnostics.Process]::Start($info)
        $output = $process.StandardOutput.ReadToEndAsync()
        $errorOutput = $process.StandardError.ReadToEndAsync()
        $timedOut = $false; $nextNotice = 30
        while (!$process.WaitForExit(250)) {
            if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $process.Kill($true); $process.WaitForExit(); $timedOut = $true; break
            }
            if ($watch.Elapsed.TotalSeconds -ge $nextNotice) {
                Write-Host "${Label}: running ($nextNotice s)"
                $nextNotice += 30
            }
        }
        $watch.Stop()
        $after = Get-MeasurementSystemTimes
        $row.CpuSeconds = $process.TotalProcessorTime.TotalSeconds
        $row.CpuToWall = $row.CpuSeconds / $watch.Elapsed.TotalSeconds
        if ($after.Total -gt $before.Total) {
            $row.SystemBusyFraction = 1 - ($after.Idle - $before.Idle) / ($after.Total - $before.Total)
        }
        $row.ExitCode = $process.ExitCode
        $row.Output = $output.Result.Trim(); $row.Error = $errorOutput.Result.Trim()
        $row.Status = if ($timedOut) { 'timeout' } elseif ($process.ExitCode -ne 0) { 'failed' } else { 'passed' }
    } catch {
        $row.Status = if ($process) { 'process-error' } else { 'launch-failed' }
        $row.Error = $_.Exception.Message
    } finally {
        $watch.Stop(); $row.WallSeconds = $watch.Elapsed.TotalSeconds
        if ($process) {
            try {
                if (!$process.HasExited) { $process.Kill($true); $process.WaitForExit() }
                if ($output -and !$row.Output) { $row.Output = $output.Result.Trim() }
                if ($errorOutput -and !$row.Error) { $row.Error = $errorOutput.Result.Trim() }
            } finally { $process.Dispose() }
        }
    }
    return $row
}

function ConvertFrom-MeasurementCostOutput([string]$Output) {
    $row = [pscustomobject]@{ Status='malformed-output'; Seconds=$null; Checksum=$null
        Allocations=$null; RequestedBytes=$null }
    $match = [regex]::Match($Output,
        '^seconds=(\d+\.\d+) checksum=(\d+) allocations=(\d+) requested_bytes=(\d+)$')
    if (!$match.Success) { return $row }
    [double]$seconds = 0; [ulong]$checksum = 0; [ulong]$allocations = 0; [ulong]$bytes = 0
    $culture = [Globalization.CultureInfo]::InvariantCulture
    $integerStyle = [Globalization.NumberStyles]::None
    if (![double]::TryParse($match.Groups[1].Value, [Globalization.NumberStyles]::AllowDecimalPoint, $culture, [ref]$seconds) -or
        ![ulong]::TryParse($match.Groups[2].Value, $integerStyle, $culture, [ref]$checksum) -or
        ![ulong]::TryParse($match.Groups[3].Value, $integerStyle, $culture, [ref]$allocations) -or
        ![ulong]::TryParse($match.Groups[4].Value, $integerStyle, $culture, [ref]$bytes)) { return $row }
    $row.Seconds = $seconds; $row.Checksum = $checksum.ToString($culture)
    $row.Allocations = $allocations; $row.RequestedBytes = $bytes
    $row.Status = if (![double]::IsFinite($seconds) -or $seconds -le 0) { 'invalid-time' } else { 'passed' }
    return $row
}

Export-ModuleMember -Function *-Measurement*
