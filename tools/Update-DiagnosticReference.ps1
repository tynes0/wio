param(
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskOutput = Join-Path $taskRoot 'docs/WIO_ERROR_CODES.md'
$taskCodePattern = '"(?<code>(?:WIRN|WIR|LIR|WCPP|WBC|WVM)\d{3,5})"'
$taskStringPattern = '"(?<value>(?:\\.|[^"\\])*)"'
$taskCodes = [ordered]@{}
$taskDefinitionOverrides = @{
    WIR1460 = 'Intrinsic call metadata is incomplete or inconsistent: family, selector, receiver, target type and concrete operand signature must agree. The diagnostic includes the actual types/counts.'
    WIR3100 = 'Generic function/type materialization failed. The detailed message identifies the concrete substitution, pack/constant/layout/capture or specialization invariant that failed; there is no single fixed payload.'
    WVM1241 = 'VM object-layout indexing failed while loading the verified bytecode module. The detailed layout message is supplied by ObjectLayouts::build.'
    WIR1303 = 'Typed WIR terminator must be the final instruction in its block.'
    WIR1304 = 'Typed WIR block must end with a terminator.'
    LIR1303 = 'Lowered WIR terminator must be the final instruction in its block.'
    LIR1304 = 'Lowered WIR block must end with a terminator.'
    WIR1430 = 'Typed WIR place initialization requires a reference place and a matching value.'
    WIR1431 = 'Typed WIR store requires a mutable reference place and a matching value.'
    LIR1425 = 'Lowered WIR place initialization requires a reference place and a matching value.'
    LIR1426 = 'Lowered WIR store requires a mutable reference place and a matching value.'
    WIR2205 = 'Break statement has no active Typed WIR loop target.'
    WIR2206 = 'Continue statement has no active Typed WIR loop target.'
    WVM1152 = 'Upcast target is incompatible with the concrete object type.'
    WVM1153 = 'Checked object cast failed for the concrete object type.'
}

function Get-ExpressionEnd([string]$Source, [int]$Start) {
    $depth = 0
    $quoted = $false
    $escaped = $false
    for ($position = $Start; $position -lt $Source.Length; ++$position) {
        $character = $Source[$position]
        if ($quoted) {
            if ($escaped) { $escaped = $false }
            elseif ($character -eq '\') { $escaped = $true }
            elseif ($character -eq '"') { $quoted = $false }
            continue
        }
        if ($character -eq '"') { $quoted = $true; continue }
        if ($character -eq '(' -or $character -eq '[' -or $character -eq '{') { ++$depth; continue }
        if ($character -eq ')' -or $character -eq ']' -or $character -eq '}') {
            if ($depth -eq 0) { return $position }
            --$depth
            continue
        }
        if (($character -eq ',' -or $character -eq ';') -and $depth -eq 0) { return $position }
    }
    return $Source.Length
}

function Escape-Markdown([string]$Text) {
    return $Text.Replace('&', '&amp;').Replace('<', '&lt;').Replace('>', '&gt;').Replace('|', '\|').Replace('`', '\`')
}

$taskRoots = @('compiler', 'app', 'runtime', 'vm', 'sdk', 'cli', 'scripts', 'tools')
$taskFiles = @(foreach ($taskArea in $taskRoots) {
    Get-ChildItem -LiteralPath (Join-Path $taskRoot $taskArea) -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp', '.h', '.inl', '.wio') }
}) | Sort-Object FullName

foreach ($taskFile in $taskFiles) {
    $taskSource = Get-Content -LiteralPath $taskFile.FullName -Raw
    $taskRelative = [IO.Path]::GetRelativePath($taskRoot, $taskFile.FullName).Replace('\', '/')
    foreach ($taskMatch in [regex]::Matches($taskSource, $taskCodePattern)) {
        $taskCode = $taskMatch.Groups['code'].Value
        if (!$taskCodes.Contains($taskCode)) { $taskCodes[$taskCode] = [Collections.Generic.List[object]]::new() }
        # A conditional code selector can contain a second code before the
        # comma. Both IDs intentionally receive the same following message.
        $taskComma = $taskSource.IndexOf(',', $taskMatch.Index + $taskMatch.Length)
        $taskDefinition = ''
        $taskExpression = ''
        if ($taskComma -ge 0 -and $taskComma - $taskMatch.Index -lt 350) {
            $taskEnd = Get-ExpressionEnd $taskSource ($taskComma + 1)
            $taskExpression = $taskSource.Substring($taskComma + 1, $taskEnd - $taskComma - 1).Trim()
            $taskLiterals = @([regex]::Matches($taskExpression, $taskStringPattern))
            if ($taskLiterals.Count) {
                $taskParts = [Collections.Generic.List[string]]::new()
                $taskPreviousEnd = 0
                foreach ($taskLiteral in $taskLiterals) {
                    $taskGap = $taskExpression.Substring($taskPreviousEnd, $taskLiteral.Index - $taskPreviousEnd)
                    if ($taskGap.Trim()) { $taskParts.Add('{dynamic context}') }
                    $taskParts.Add($taskLiteral.Groups['value'].Value.Replace('\"', '"'))
                    $taskPreviousEnd = $taskLiteral.Index + $taskLiteral.Length
                }
                if ($taskExpression.Substring($taskPreviousEnd).Trim()) { $taskParts.Add('{dynamic context}') }
                $taskDefinition = $taskParts -join ''
            }
        }
        $taskLine = 1 + [regex]::Matches($taskSource.Substring(0, $taskMatch.Index), "`n").Count
        $taskCodes[$taskCode].Add([pscustomobject]@{
            Definition = $taskDefinition
            Expression = ($taskExpression -replace '\s+', ' ')
            Source = "$taskRelative#L$taskLine"
        })
    }
}

$taskLines = [Collections.Generic.List[string]]::new()
foreach ($taskLine in @(
    '# Wio Error and Status Code Reference',
    '',
    'Generated from maintained implementation sources by `tools/Update-DiagnosticReference.ps1`.',
    'This is the current development-source inventory, not a promise that every code exists in every installed release.',
    '',
    '## Scope and interpretation',
    '',
    '- Includes every literal WIR/LIR/WIRN/WCPP/WBC/WVM code found in compiler, app, runtime, VM, SDK, CLI, scripts and tools C++/Wio sources. Test expectations and historical documents are not definitions.',
    '- Each entry gives source message templates and linked definition sites. `{dynamic context}` denotes runtime details; it is not literal output. One ID may have several context-dependent definitions.',
    '- Dynamic-only messages are explicitly marked and given a reviewed category explanation. Follow the source expression and surrounding validation for their exact details. Reviewed overrides also distinguish conditional code/message branches.',
    '- Verifier errors mean an IR/bytecode invariant was violated; valid source reaching them can indicate a compiler defect. Other codes describe ordinary runtime failures such as bounds, lifetime, cancellation or foreign-boundary errors.',
    '- Native/module status enums, SDK exception categories and std error/domain enums are listed separately below. Pending/ready/success states are not errors.',
    '- Legacy lexer/parser/analyzer/CLI messages without assigned stable IDs cannot have code definitions here. User/custom Result codes and native OS error numbers are open domains, not a finite Wio registry.',
    '- This is a source inventory, not the future normative structured diagnostic registry, severity/fix-it catalog or `wio explain` implementation.',
    '',
    'Regenerate: `pwsh -File tools/Update-DiagnosticReference.ps1`.',
    'Check freshness: `pwsh -File tools/Update-DiagnosticReference.ps1 -Check`.',
    '',
    "Distinct diagnostic IDs: **$($taskCodes.Count)**.",
    '',
    '## Diagnostic families',
    '',
    '| Family | IDs | Meaning |',
    '| --- | ---: | --- |'
)) { $taskLines.Add($taskLine) }

$taskFamilyDescriptions = [ordered]@{
    WIR = 'Typed WIR building/verification, specialization and canonical lowering.'
    LIR = 'Lowered WIR verification and canonical ownership/control-flow invariants.'
    WIRN = 'Native ABI/thunk planning invariants.'
    WCPP = 'Lowered WIR C++ backend validation and emission.'
    WBC = 'Bytecode compilation, format decoding and verification.'
    WVM = 'VM loading/execution, intrinsics, native transport and lifetime failures.'
}
foreach ($taskFamily in $taskFamilyDescriptions.Keys) {
    $taskCount = @($taskCodes.Keys | Where-Object { $_ -match "^$taskFamily\d+$" }).Count
    $taskLines.Add("| [$taskFamily](#$($taskFamily.ToLowerInvariant())) | $taskCount | $($taskFamilyDescriptions[$taskFamily]) |")
}

$taskDynamic = [Collections.Generic.List[string]]::new()
foreach ($taskFamily in $taskFamilyDescriptions.Keys) {
    $taskLines.Add('')
    $taskLines.Add("## $taskFamily")
    foreach ($taskCode in @($taskCodes.Keys | Where-Object { $_ -match "^$taskFamily\d+$" } | Sort-Object)) {
        $taskLines.Add('')
        $taskLines.Add("### $taskCode")
        $taskLines.Add('')
        $taskDefinitions = @($taskCodes[$taskCode] | Where-Object Definition | Select-Object -ExpandProperty Definition -Unique)
        if ($taskDefinitionOverrides.ContainsKey($taskCode)) {
            $taskLines.Add('Definition: ' + $taskDefinitionOverrides[$taskCode])
            if (!$taskDefinitions.Count) {
                $taskDynamic.Add($taskCode)
                $taskLines.Add('')
                $taskLines.Add('Detailed message: dynamically supplied by the reporting path.')
                foreach ($taskExpression in @($taskCodes[$taskCode].Expression | Where-Object { $_ } | Sort-Object -Unique)) {
                    $taskLines.Add('Message expression: `' + $taskExpression.Replace('`', '') + '`.')
                }
            }
        }
        elseif ($taskDefinitions.Count) {
            foreach ($taskDefinition in $taskDefinitions) { $taskLines.Add('- ' + (Escape-Markdown $taskDefinition)) }
        }
        else {
            $taskDynamic.Add($taskCode)
            $taskLines.Add('Definition: supplied dynamically by the reporting path; no fixed message literal is declared at this code site.')
            foreach ($taskExpression in @($taskCodes[$taskCode].Expression | Where-Object { $_ } | Sort-Object -Unique)) {
                $taskLines.Add('Message expression: `' + $taskExpression.Replace('`', '') + '`.')
            }
        }
        $taskLines.Add('')
        $taskLinks = @($taskCodes[$taskCode].Source | Sort-Object -Unique | ForEach-Object {
            '[' + $_.Replace('#L', ':') + '](../' + $_ + ')'
        })
        $taskLines.Add('Definitions: ' + ($taskLinks -join ', ') + '.')
    }
}

$taskLines.Add('')
$taskLines.Add('## Native, SDK and std status/error enums')
$taskLines.Add('')
$taskLines.Add('Values below are current implementation values. Only explicitly ABI-versioned enums are wire contracts; do not rely on incidental SDK exception ordinals as a stable ABI. OS native codes retain their platform meaning and user Result codes retain their author-defined domain.')
$taskLines.Add('')
$taskLines.Add('`WioResultError` carries domain, portable code, native OS code and message separately. The same integer in two domains is not the same error. `none`/`Ok` denotes success/no error; `Pending`/`NotReady`/`TimedOut` is task state or an observation outcome and must not be conflated with a faulted payload. ConsoleErrorDomain and WioResultDomain classify provenance, not failure reasons.')

$taskSdkMeanings = @{
    InvalidArgument = 'The caller supplied an invalid argument to an SDK operation.'
    LibraryOpenFailed = 'The platform loader could not open the requested module library.'
    SymbolLookupFailed = 'A required exported symbol could not be resolved in the library.'
    ApiUnavailable = 'The requested module API or optional sidecar capability is unavailable.'
    InvalidApiDescriptor = 'Module/sidecar descriptor version, size or required pointers are invalid.'
    ExportNotFound = 'The requested exported entry is absent.'
    TypeNotFound = 'The requested exported/reflected type is absent.'
    CommandNotFound = 'The requested exported command is absent.'
    EventNotFound = 'The requested exported event is absent.'
    EventHookNotFound = 'The requested event hook is absent.'
    FieldNotFound = 'The requested field descriptor is absent.'
    MethodNotFound = 'The requested callable method descriptor is absent.'
    FieldNotWritable = 'The requested field is readonly or has no writable bridge.'
    SignatureMismatch = 'Requested type/signature/shape differs from the advertised bridge contract.'
    InvokeFailed = 'The exported/native invocation failed or returned an invalid result.'
    LifecycleFailed = 'A module/application lifecycle operation failed.'
    StaleBinding = 'A binding belongs to a retired/unloaded generation and cannot be used.'
    ReloadFailed = 'Staging or replacing the module during reload failed.'
    IoFailure = 'An SDK file/I/O operation failed.'
}

$taskEnumFiles = [ordered]@{
    'sdk/include/wio_sdk.h' = @('ErrorCode')
    'sdk/include/wio_native_abi.h' = @('WioNativeAbiStatus')
    'sdk/include/module_api.h' = @('WioInvokeStatus', 'WioCallbackStatus', 'WioApplicationStatus', 'WioAsyncTaskStatus', 'WioAsyncOperationStatus')
    'sdk/include/wio_values.h' = @('WioResultDomain')
    'runtime/include/io_errors.h' = @('WriteError', 'ReadError')
    'runtime/include/std_convert.h' = @('ParseError')
    'runtime/include/std_fs.h' = @('ErrorCode')
    'runtime/include/std_io.h' = @('FileError')
    'runtime/include/std_process.h' = @('ProcessError')
    'runtime/include/std_regex.h' = @('RegexError')
    'runtime/include/std_console.h' = @('ConsoleStatus', 'ConsoleErrorDomain')
}
foreach ($taskEnumFile in $taskEnumFiles.Keys) {
    $taskSource = Get-Content -LiteralPath (Join-Path $taskRoot $taskEnumFile) -Raw
    foreach ($taskEnum in $taskEnumFiles[$taskEnumFile]) {
        $taskMatch = [regex]::Match($taskSource, "(?s)enum\s+(?:class\s+)?$taskEnum\b[^\{]*\{(?<body>.*?)\};")
        if (!$taskMatch.Success) { throw "Missing enum $taskEnum in $taskEnumFile" }
        $taskLines.Add('')
        $taskLines.Add("### $taskEnum — $taskEnumFile")
        $taskLines.Add('')
        $taskLine = 1 + [regex]::Matches($taskSource.Substring(0, $taskMatch.Index), "`n").Count
        $taskLines.Add("Source: [$taskEnumFile](../$taskEnumFile#L$taskLine).")
        $taskLines.Add('')
        $taskLines.Add('| Name | Value | Meaning / category |')
        $taskLines.Add('| --- | ---: | --- |')
        $taskValue = 0
        foreach ($taskEntry in [regex]::Matches($taskMatch.Groups['body'].Value, '(?m)^\s*(?<name>\w+)\s*(?:=\s*(?<value>\d+))?\s*,?\s*$')) {
            if ($taskEntry.Groups['value'].Success) { $taskValue = [int]$taskEntry.Groups['value'].Value }
            $taskName = $taskEntry.Groups['name'].Value
            $taskMeaning = ($taskName -replace '^WIO_(NATIVE_ABI|INVOKE|CALLBACK|APPLICATION|ASYNC_TASK|ASYNC)_', '')
            $taskMeaning = ($taskMeaning -creplace '([a-z])([A-Z])', '$1 $2').Replace('_', ' ').ToLowerInvariant()
            if ($taskEnumFile -eq 'sdk/include/wio_sdk.h') { $taskMeaning = $taskSdkMeanings[$taskName] }
            $taskLines.Add("| ``$taskName`` | $taskValue | $taskMeaning |")
            ++$taskValue
        }
    }
}

$taskLines.Add('')
$taskText = ($taskLines -join "`n")
if ($Check) {
    if (!(Test-Path -LiteralPath $taskOutput) -or (Get-Content -LiteralPath $taskOutput -Raw).Replace("`r`n", "`n") -cne $taskText) {
        throw 'Error-code reference is stale. Run tools/Update-DiagnosticReference.ps1.'
    }
}
else {
    [IO.File]::WriteAllText($taskOutput, $taskText, [Text.UTF8Encoding]::new($false))
}
Write-Output "Diagnostic reference: $($taskCodes.Count) IDs, $($taskDynamic.Count) dynamic-only definitions; check=$Check."
if ($taskDynamic.Count) { Write-Output ('Dynamic-only IDs: ' + ($taskDynamic -join ', ')) }
