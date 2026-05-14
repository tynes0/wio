[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$HeaderPath,

    [Parameter(Mandatory = $true)]
    [string]$RealmName,

    [string]$OutputPath,

    [string]$HeaderInclude,

    [switch]$PreferFlagset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RequiredPath {
    param([string]$PathValue)

    $resolved = Resolve-Path -LiteralPath $PathValue -ErrorAction Stop
    return $resolved.Path
}

function Add-Line {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Text = ''
    )

    $Lines.Add($Text) | Out-Null
}

function Quote-WioString {
    param([string]$Value)
    return '"' + ($Value -replace '\\', '\\' -replace '"', '\"') + '"'
}

function Remove-CComments {
    param([string]$Text)

    $withoutBlock = [System.Text.RegularExpressions.Regex]::Replace(
        $Text,
        '/\*.*?\*/',
        '',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )

    return [System.Text.RegularExpressions.Regex]::Replace(
        $withoutBlock,
        '//.*?$',
        '',
        [System.Text.RegularExpressions.RegexOptions]::Multiline
    )
}

function Collapse-Whitespace {
    param([string]$Text)

    return (($Text -replace '\s+', ' ') -replace '^\s+|\s+$', '')
}

function Get-MatchingBraceIndex {
    param(
        [string]$Text,
        [int]$OpenBraceIndex
    )

    $depth = 0
    for ($index = $OpenBraceIndex; $index -lt $Text.Length; ++$index) {
        $current = $Text[$index]
        if ($current -eq '{') {
            $depth += 1
        }
        elseif ($current -eq '}') {
            $depth -= 1
            if ($depth -eq 0) {
                return $index
            }
        }
    }

    throw "Unbalanced braces while parsing header."
}

function Split-TopLevelCommaList {
    param([string]$Text)

    $items = [System.Collections.Generic.List[string]]::new()
    $depthAngle = 0
    $depthParen = 0
    $start = 0

    for ($index = 0; $index -lt $Text.Length; ++$index) {
        $current = $Text[$index]
        switch ($current) {
            '<' { $depthAngle += 1 }
            '>' { if ($depthAngle -gt 0) { $depthAngle -= 1 } }
            '(' { $depthParen += 1 }
            ')' { if ($depthParen -gt 0) { $depthParen -= 1 } }
            ',' {
                if ($depthAngle -eq 0 -and $depthParen -eq 0) {
                    $items.Add($Text.Substring($start, $index - $start)) | Out-Null
                    $start = $index + 1
                }
            }
        }
    }

    if ($start -lt $Text.Length) {
        $items.Add($Text.Substring($start)) | Out-Null
    }

    return @($items | ForEach-Object { Collapse-Whitespace $_ } | Where-Object { $_ -ne '' })
}

function Convert-CppTypeToWioType {
    param([string]$TypeText)

    $typeText = Collapse-Whitespace $TypeText
    $typeText = $typeText -replace '\bstruct\s+', ''
    $typeText = $typeText -replace '\benum\s+class\s+', ''
    $typeText = $typeText -replace '\benum\s+', ''
    $typeText = $typeText -replace '\bclass\s+', ''

    if ($typeText -match '^const\s+char\s*\*$') { return 'string' }
    if ($typeText -match '^char\s*\*$') { return 'string' }
    if ($typeText -match '^const\s+void\s*\*$') { return 'opaque' }
    if ($typeText -match '^void\s*\*$') { return 'opaque' }

    switch ($typeText) {
        'void' { return 'void' }
        'bool' { return 'bool' }
        'char' { return 'char' }
        'signed char' { return 'i8' }
        'unsigned char' { return 'u8' }
        'short' { return 'i16' }
        'unsigned short' { return 'u16' }
        'int' { return 'i32' }
        'unsigned int' { return 'u32' }
        'long long' { return 'i64' }
        'unsigned long long' { return 'u64' }
        'float' { return 'f32' }
        'double' { return 'f64' }
        'std::int8_t' { return 'i8' }
        'int8_t' { return 'i8' }
        'std::int16_t' { return 'i16' }
        'int16_t' { return 'i16' }
        'std::int32_t' { return 'i32' }
        'int32_t' { return 'i32' }
        'std::int64_t' { return 'i64' }
        'int64_t' { return 'i64' }
        'std::uint8_t' { return 'u8' }
        'uint8_t' { return 'u8' }
        'std::uint16_t' { return 'u16' }
        'uint16_t' { return 'u16' }
        'std::uint32_t' { return 'u32' }
        'uint32_t' { return 'u32' }
        'std::uint64_t' { return 'u64' }
        'uint64_t' { return 'u64' }
        'std::size_t' { return 'usize' }
        'size_t' { return 'usize' }
        default { return $typeText }
    }
}

function Convert-CppDeclarationParameter {
    param([string]$ParameterText)

    $parameterText = Collapse-Whitespace $ParameterText
    if ($parameterText -eq '' -or $parameterText -eq 'void') {
        return $null
    }

    if ($parameterText -match '^(?<type>.+?)\s+(?<name>[A-Za-z_]\w*)$') {
        $typePart = Collapse-Whitespace $Matches.type
        $namePart = $Matches.name
    }
    else {
        throw "Unsupported parameter declaration: '$parameterText'"
    }

    $isReference = $typePart -like '*&'
    $isPointer = $typePart -like '*`*'
    $isConst = $typePart -like 'const *'

    if ($isReference) {
        $baseType = Collapse-Whitespace(($typePart.Substring(0, $typePart.Length - 1)) -replace '^const\s+', '')
        $wioBaseType = Convert-CppTypeToWioType $baseType
        if ($isConst) {
            return "${namePart}: view $wioBaseType"
        }
        return "${namePart}: ref $wioBaseType"
    }

    if ($isPointer) {
        $baseType = Collapse-Whitespace(($typePart.Substring(0, $typePart.Length - 1)) -replace '^const\s+', '')
        $wioBaseType = Convert-CppTypeToWioType $baseType
        if ($wioBaseType -eq 'string' -or $wioBaseType -eq 'opaque') {
            return "${namePart}: $wioBaseType"
        }

        if ($isConst) {
            return "${namePart}: view $wioBaseType"
        }
        return "${namePart}: ref $wioBaseType"
    }

    return "${namePart}: $(Convert-CppTypeToWioType $typePart)"
}

function Convert-CppReturnTypeToWioType {
    param([string]$ReturnTypeText)

    $returnTypeText = Collapse-Whitespace $ReturnTypeText
    if ($returnTypeText -like '*&' -or $returnTypeText -like '*`*') {
        $baseType = $returnTypeText.TrimEnd('&', '*').Trim()
        $baseType = $baseType -replace '^const\s+', ''
        return Convert-CppTypeToWioType $baseType
    }

    return Convert-CppTypeToWioType $returnTypeText
}

function Convert-UnderlyingTypeToAttribute {
    param([string]$UnderlyingType)

    if ([string]::IsNullOrWhiteSpace($UnderlyingType)) {
        return $null
    }

    $mapped = Convert-CppTypeToWioType $UnderlyingType
    if ($mapped -eq 'void') {
        return $null
    }

    return "@Type($mapped)"
}

function Parse-EnumMembers {
    param([string]$Body)

    $members = [System.Collections.Generic.List[object]]::new()
    $entries = $Body.Split(',')
    foreach ($rawEntry in $entries) {
        $entry = Collapse-Whitespace $rawEntry
        if ($entry -eq '') {
            continue
        }

        if ($entry -match '^(?<name>[A-Za-z_]\w*)\s*=\s*(?<value>.+)$') {
            $members.Add([pscustomobject]@{
                    name = $Matches.name
                    value = Collapse-Whitespace $Matches.value
                }) | Out-Null
        }
        elseif ($entry -match '^(?<name>[A-Za-z_]\w*)$') {
            $members.Add([pscustomobject]@{
                    name = $Matches.name
                    value = $null
                }) | Out-Null
        }
    }

    return @($members)
}

function Test-IsLikelyFlagset {
    param(
        [string]$Name,
        [object[]]$Members
    )

    if ($PreferFlagset) {
        return $true
    }

    if ($Name -match '(Flags|Bits|Mask|Modifiers|Options)$') {
        return $true
    }

    foreach ($member in $Members) {
        if ($null -eq $member.value) {
            continue
        }

        if ($member.value -match '(<<|\||&|~)') {
            return $true
        }
    }

    return $false
}

function Parse-FlatDeclarations {
    param(
        [string]$Text,
        [string[]]$NamespaceStack
    )

    $results = [pscustomobject]@{
        enums = [System.Collections.Generic.List[object]]::new()
        structs = [System.Collections.Generic.List[object]]::new()
        functions = [System.Collections.Generic.List[object]]::new()
    }

    $working = $Text

    $enumPattern = [System.Text.RegularExpressions.Regex]::new(
        'enum(?:\s+class)?\s+(?<name>[A-Za-z_]\w*)\s*(?::\s*(?<under>[^{};]+))?\s*\{(?<body>.*?)\}\s*;',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    foreach ($match in $enumPattern.Matches($working)) {
        $members = Parse-EnumMembers $match.Groups['body'].Value
        $results.enums.Add([pscustomobject]@{
                name = $match.Groups['name'].Value
                cppName = (($NamespaceStack + @($match.Groups['name'].Value)) -join '::')
                underlyingType = Collapse-Whitespace $match.Groups['under'].Value
                members = $members
                isFlagset = Test-IsLikelyFlagset $match.Groups['name'].Value $members
            }) | Out-Null
    }
    $working = $enumPattern.Replace($working, ' ')

    $structPattern = [System.Text.RegularExpressions.Regex]::new(
        'struct\s+(?<name>[A-Za-z_]\w*)\s*\{(?<body>.*?)\}\s*;',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    foreach ($match in $structPattern.Matches($working)) {
        $fields = [System.Collections.Generic.List[object]]::new()
        $fieldLines = @($match.Groups['body'].Value -split ';') |
            ForEach-Object { Collapse-Whitespace $_ } |
            Where-Object { $_ -ne '' }

        foreach ($fieldLine in $fieldLines) {
            if ($fieldLine -match '^(?<type>.+?)\s+(?<name>[A-Za-z_]\w*)$') {
                $fields.Add([pscustomobject]@{
                        name = $Matches.name
                        type = Convert-CppTypeToWioType $Matches.type
                    }) | Out-Null
            }
        }

        $results.structs.Add([pscustomobject]@{
                name = $match.Groups['name'].Value
                cppName = (($NamespaceStack + @($match.Groups['name'].Value)) -join '::')
                fields = @($fields)
            }) | Out-Null
    }
    $working = $structPattern.Replace($working, ' ')

    $functionPattern = [System.Text.RegularExpressions.Regex]::new(
        '(?<ret>[A-Za-z_:\s\*&<>\d,]+?)\s+(?<name>[A-Za-z_]\w*)\s*\((?<params>[^;{}()]*)\)\s*;',
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    foreach ($match in $functionPattern.Matches($working)) {
        $name = $match.Groups['name'].Value
        if ($name -in @('if', 'for', 'while', 'switch', 'return')) {
            continue
        }

        $parameterSpecs = [System.Collections.Generic.List[string]]::new()
        foreach ($parameter in (Split-TopLevelCommaList $match.Groups['params'].Value)) {
            $rendered = Convert-CppDeclarationParameter $parameter
            if ($null -ne $rendered) {
                $parameterSpecs.Add($rendered) | Out-Null
            }
        }

        $results.functions.Add([pscustomobject]@{
                name = $name
                cppName = (($NamespaceStack + @($name)) -join '::')
                returnType = Convert-CppReturnTypeToWioType $match.Groups['ret'].Value
                parameters = @($parameterSpecs)
            }) | Out-Null
    }

    return $results
}

function Parse-NamespaceAwareDeclarations {
    param(
        [string]$Text,
        [string[]]$NamespaceStack = @()
    )

    $aggregate = [pscustomobject]@{
        enums = [System.Collections.Generic.List[object]]::new()
        structs = [System.Collections.Generic.List[object]]::new()
        functions = [System.Collections.Generic.List[object]]::new()
    }

    $namespaceRegex = [System.Text.RegularExpressions.Regex]::new('namespace\s+(?<name>[A-Za-z_]\w*)\s*\{')
    $cursor = 0

    foreach ($match in $namespaceRegex.Matches($Text)) {
        if ($match.Index -lt $cursor) {
            continue
        }

        $prefix = $Text.Substring($cursor, $match.Index - $cursor)
        $flatResults = Parse-FlatDeclarations -Text $prefix -NamespaceStack $NamespaceStack
        foreach ($enumSpec in $flatResults.enums) { $aggregate.enums.Add($enumSpec) | Out-Null }
        foreach ($structSpec in $flatResults.structs) { $aggregate.structs.Add($structSpec) | Out-Null }
        foreach ($functionSpec in $flatResults.functions) { $aggregate.functions.Add($functionSpec) | Out-Null }

        $openBraceIndex = $match.Index + $match.Length - 1
        $closeBraceIndex = Get-MatchingBraceIndex -Text $Text -OpenBraceIndex $openBraceIndex
        $body = $Text.Substring($openBraceIndex + 1, $closeBraceIndex - $openBraceIndex - 1)
        $childResults = Parse-NamespaceAwareDeclarations -Text $body -NamespaceStack ($NamespaceStack + @($match.Groups['name'].Value))
        foreach ($enumSpec in $childResults.enums) { $aggregate.enums.Add($enumSpec) | Out-Null }
        foreach ($structSpec in $childResults.structs) { $aggregate.structs.Add($structSpec) | Out-Null }
        foreach ($functionSpec in $childResults.functions) { $aggregate.functions.Add($functionSpec) | Out-Null }

        $cursor = $closeBraceIndex + 1
    }

    if ($cursor -lt $Text.Length) {
        $tail = $Text.Substring($cursor)
        $tailResults = Parse-FlatDeclarations -Text $tail -NamespaceStack $NamespaceStack
        foreach ($enumSpec in $tailResults.enums) { $aggregate.enums.Add($enumSpec) | Out-Null }
        foreach ($structSpec in $tailResults.structs) { $aggregate.structs.Add($structSpec) | Out-Null }
        foreach ($functionSpec in $tailResults.functions) { $aggregate.functions.Add($functionSpec) | Out-Null }
    }

    return $aggregate
}

$resolvedHeader = Resolve-RequiredPath $HeaderPath
$headerDirectory = Split-Path -Parent $resolvedHeader

if ([string]::IsNullOrWhiteSpace($HeaderInclude)) {
    $HeaderInclude = [System.IO.Path]::GetFileName($resolvedHeader)
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $headerStem = [System.IO.Path]::GetFileNameWithoutExtension($resolvedHeader)
    $OutputPath = Join-Path $headerDirectory ($headerStem + '.wio')
}
elseif (-not [System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
}

$headerText = Get-Content -LiteralPath $resolvedHeader -Raw
$cleanText = Remove-CComments $headerText
$parsed = Parse-NamespaceAwareDeclarations -Text $cleanText

$lines = [System.Collections.Generic.List[string]]::new()
Add-Line $lines "// Generated by Import-CHeaderToWioBinding.ps1"
Add-Line $lines "// Source header: $resolvedHeader"
Add-Line $lines
Add-Line $lines "realm $RealmName {"
Add-Line $lines ("    use @CppHeader(" + (Quote-WioString $HeaderInclude) + ");")
Add-Line $lines

foreach ($enumSpec in $parsed.enums) {
    Add-Line $lines "    @Native"
    $underlyingAttribute = Convert-UnderlyingTypeToAttribute $enumSpec.underlyingType
    if ($null -ne $underlyingAttribute) {
        Add-Line $lines "    $underlyingAttribute"
    }
    Add-Line $lines "    @CppName($($enumSpec.cppName))"
    $keyword = if ($enumSpec.isFlagset) { 'flagset' } else { 'enum' }
    Add-Line $lines "    $keyword $($enumSpec.name) {"
    foreach ($member in $enumSpec.members) {
        if ($null -ne $member.value) {
            Add-Line $lines "        $($member.name) = $($member.value),"
        }
        else {
            Add-Line $lines "        $($member.name),"
        }
    }
    Add-Line $lines "    };"
    Add-Line $lines
}

foreach ($structSpec in $parsed.structs) {
    Add-Line $lines "    @Native"
    Add-Line $lines "    @CppName($($structSpec.cppName))"
    Add-Line $lines "    component $($structSpec.name) {"
    foreach ($field in $structSpec.fields) {
        Add-Line $lines "        $($field.name): $($field.type);"
    }
    Add-Line $lines "    }"
    Add-Line $lines
}

foreach ($functionSpec in $parsed.functions) {
    Add-Line $lines "    @Native"
    Add-Line $lines "    @CppName($($functionSpec.cppName))"
    $parameterList = $functionSpec.parameters -join ', '
    if ($functionSpec.returnType -eq 'void') {
        Add-Line $lines "    fn $($functionSpec.name)($parameterList);"
    }
    else {
        Add-Line $lines "    fn $($functionSpec.name)($parameterList) -> $($functionSpec.returnType);"
    }
    Add-Line $lines
}

Add-Line $lines "}"

$parent = Split-Path -Parent $OutputPath
if (-not [string]::IsNullOrWhiteSpace($parent)) {
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

Set-Content -LiteralPath $OutputPath -Value $lines -Encoding UTF8
Write-Output "Generated Wio binding module: $OutputPath"
