<#
.SYNOPSIS
    Collecte l'état de la sauvegarde Perforce d'un serveur distant et l'écrit en JSON.

.DESCRIPTION
    Lancé par P4 Backup Monitor depuis un poste administrateur. Lecture seule :
    rien n'est modifié sur le serveur ni sur le NAS.

    Sources :
      - \\<serveur>\C$\PerforceBackup\config.psd1 et \logs : partage administratif
      - tâches planifiées et disques : CIM (WinRM, sinon DCOM)
      - <NasRoot>\status\last-backup.json et espace libre du NAS : partage du NAS

    Chaque source est indépendante : une source en échec est signalée dans
    "problems" sans empêcher les autres.

.EXAMPLE
    .\Get-BackupStatus.ps1 -Server NUK -OutFile $env:TEMP\status.json
#>
param(
    [Parameter(Mandatory)][string]$Server,
    [string]$ScriptDir = 'C:\PerforceBackup',
    [string]$TaskFolder = '\Perforce\',
    [int]$LogDays = 8,
    [string]$OutFile
)

$ErrorActionPreference = 'Stop'
$problems = New-Object System.Collections.ArrayList
function Add-Problem([string]$Source, [string]$Message) {
    [void]$problems.Add([ordered]@{ source = $Source; message = $Message })
}
function To-Iso($d) { if ($d -and ([datetime]$d).Year -gt 1999) { ([datetime]$d).ToString('yyyy-MM-ddTHH:mm:ss') } else { $null } }

# Chemin local du serveur (C:\PerforceBackup) vers son partage administratif (\\NUK\C$\PerforceBackup)
function Get-AdminPath([string]$LocalPath) {
    if ($Server -in @('localhost', '.', $env:COMPUTERNAME)) { return $LocalPath }
    if ($LocalPath -match '^([A-Za-z]):\\(.*)$') { return "\\$Server\$($Matches[1])`$\$($Matches[2])" }
    return $LocalPath
}

Add-Type -Namespace P4Mon -Name Native -MemberDefinition @'
[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern bool GetDiskFreeSpaceEx(string path, out ulong freeForUser, out ulong total, out ulong totalFree);
'@

function Get-SpaceInfo([string]$Path) {
    $free = [uint64]0; $total = [uint64]0; $tf = [uint64]0
    $p = $Path.TrimEnd('\') + '\'
    if (-not [P4Mon.Native]::GetDiskFreeSpaceEx($p, [ref]$free, [ref]$total, [ref]$tf)) {
        throw (New-Object ComponentModel.Win32Exception ([Runtime.InteropServices.Marshal]::GetLastWin32Error())).Message
    }
    return [ordered]@{ path = $Path; totalBytes = $total; freeBytes = $free }
}

$result = [ordered]@{
    collectedAt = (Get-Date).ToString('yyyy-MM-ddTHH:mm:ss')
    server      = $Server
    collector   = 'Get-BackupStatus.ps1 1.0'
    config      = $null
    tasks       = @()
    disks       = @()
    nas         = $null
    lastBackup  = $null
    logs        = @()
    problems    = $problems
}

# ----------------------------------------------------------------------
# 1. Configuration de la sauvegarde sur le serveur
# ----------------------------------------------------------------------
$cfg = $null
try {
    $cfgPath = Join-Path (Get-AdminPath $ScriptDir) 'config.psd1'
    $cfg = Import-PowerShellDataFile -Path $cfgPath
    $result.config = [ordered]@{
        path          = $cfgPath
        serverLabel   = if ($cfg.ServerLabel) { $cfg.ServerLabel } else { "Perforce $Server" }
        nasRoot       = $cfg.NasRoot
        backupAccount = $cfg.BackupAccount
        testAccount   = $cfg.TestAccount
        p4Root        = $cfg.P4Root
        maxAgeHours   = $cfg.MaxCheckpointAgeHours
        logDir        = $cfg.LocalLogDir
    }
} catch {
    Add-Problem 'config' "Configuration illisible ($cfgPath) : $($_.Exception.Message)"
}

# ----------------------------------------------------------------------
# 2. Tâches planifiées et disques (CIM : WinRM, puis DCOM)
# ----------------------------------------------------------------------
$cim = $null
foreach ($proto in 'Wsman', 'Dcom') {
    try {
        $opt = New-CimSessionOption -Protocol $proto
        $cim = New-CimSession -ComputerName $Server -SessionOption $opt -OperationTimeoutSec 15 -ErrorAction Stop
        $result.cimProtocol = $proto
        break
    } catch { $lastCimError = $_.Exception.Message }
}
if (-not $cim) {
    Add-Problem 'cim' "Connexion CIM impossible (WinRM et DCOM) : $lastCimError"
} else {
    try {
        $tasks = Get-ScheduledTask -CimSession $cim -TaskPath $TaskFolder -ErrorAction Stop
        $result.tasks = @(foreach ($t in $tasks) {
            $i = $t | Get-ScheduledTaskInfo -CimSession $cim
            [ordered]@{
                name       = $t.TaskName
                state      = "$($t.State)"
                account    = $t.Principal.UserId
                lastRun    = To-Iso $i.LastRunTime
                lastResult = [int64]$i.LastTaskResult
                nextRun    = To-Iso $i.NextRunTime
                triggers   = @($t.Triggers | ForEach-Object { To-Iso $_.StartBoundary })
            }
        })
        if (-not $result.tasks) { Add-Problem 'tasks' "Aucune tâche dans le dossier $TaskFolder du Planificateur" }
    } catch {
        Add-Problem 'tasks' "Lecture des tâches planifiées impossible : $($_.Exception.Message)"
    }
    try {
        $result.disks = @(Get-CimInstance -CimSession $cim -ClassName Win32_LogicalDisk -Filter 'DriveType=3' | ForEach-Object {
            [ordered]@{ name = $_.DeviceID; label = $_.VolumeName; totalBytes = [uint64]$_.Size; freeBytes = [uint64]$_.FreeSpace }
        })
    } catch {
        Add-Problem 'disks' "Lecture des disques du serveur impossible : $($_.Exception.Message)"
    }
    Remove-CimSession $cim -ErrorAction SilentlyContinue
}

# ----------------------------------------------------------------------
# 3. NAS : espace libre et statut de la dernière sauvegarde
# ----------------------------------------------------------------------
if ($cfg -and $cfg.NasRoot) {
    try {
        $result.nas = Get-SpaceInfo $cfg.NasRoot
        $result.nas.reachable = $true
    } catch {
        $result.nas = [ordered]@{ path = $cfg.NasRoot; reachable = $false }
        Add-Problem 'nas' "NAS inaccessible ($($cfg.NasRoot)) : $($_.Exception.Message)"
    }
    if ($result.nas.reachable) {
        $statusFile = Join-Path (Join-Path $cfg.NasRoot 'status') 'last-backup.json'
        try {
            $s = Get-Content -Path $statusFile -Raw -Encoding UTF8 | ConvertFrom-Json
            $date = [datetime]$s.Date
            $result.lastBackup = [ordered]@{
                date             = To-Iso $date
                ageHours         = [math]::Round(((Get-Date) - $date).TotalHours, 1)
                checkpointNumber = $s.CheckpointNumber
                checkpointFile   = $s.CheckpointFile
                changeCounter    = $s.ChangeCounter
                depotCount       = @($s.Depots).Count
                durationSeconds  = $s.DurationSeconds
            }
        } catch {
            Add-Problem 'status' "Statut de sauvegarde illisible ($statusFile) : $($_.Exception.Message)"
        }
    }
}

# ----------------------------------------------------------------------
# 4. Journaux récents
#    Format d'une ligne : 2026-11-05 21:00:01 [INFO|WARN|ERROR] message [CODE]
# ----------------------------------------------------------------------
$logDir = if ($cfg -and $cfg.LocalLogDir) { $cfg.LocalLogDir } else { Join-Path $ScriptDir 'logs' }
$logPath = Get-AdminPath $logDir
$lineRx = '^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) \[(INFO|WARN|ERROR)\] (.*)$'
$codeRx = '\[((?:SAV|TEST|REP|MES|NAS|GEN)-\d{2})\]\s*$'
try {
    $files = Get-ChildItem -Path $logPath -Filter '*.log' -File -ErrorAction Stop |
        Where-Object { $_.LastWriteTime -gt (Get-Date).AddDays(-$LogDays) } |
        Sort-Object LastWriteTime -Descending
    $result.logs = @(foreach ($f in $files) {
        $lines = @(Get-Content -Path $f.FullName -Encoding UTF8)
        $entries = New-Object System.Collections.ArrayList
        $summary = $null
        foreach ($l in $lines) {
            if ($l -match $lineRx) {
                $time = $Matches[1]; $lvl = $Matches[2]; $msg = $Matches[3]
                if ($msg -match '^=== (.*) ===$') { $summary = $Matches[1] }
                if ($lvl -ne 'INFO') {
                    $code = if ($msg -match $codeRx) { $Matches[1] } else { $null }
                    [void]$entries.Add([ordered]@{ time = $time; level = $lvl; code = $code; text = $msg })
                }
            }
        }
        $hasError = @($entries | Where-Object { $_.level -eq 'ERROR' }).Count -gt 0
        $hasWarn = @($entries | Where-Object { $_.level -eq 'WARN' }).Count -gt 0
        $status = if ($hasError) { 'error' } elseif ($summary -and $hasWarn) { 'warning' } elseif ($summary) { 'ok' } else { 'incomplete' }
        [ordered]@{
            file     = $f.Name
            path     = $f.FullName
            kind     = ($f.BaseName -replace '_\d{8}_\d{6}$', '')
            started  = To-Iso $f.CreationTime
            modified = To-Iso $f.LastWriteTime
            status   = $status
            summary  = $summary
            issues   = @($entries)
            text     = ($lines -join "`n")
        }
    })
} catch {
    Add-Problem 'logs' "Journaux illisibles ($logPath) : $($_.Exception.Message)"
}

$json = $result | ConvertTo-Json -Depth 8
if ($OutFile) {
    [IO.File]::WriteAllText($OutFile, $json, (New-Object Text.UTF8Encoding $false))
} else {
    [Console]::OutputEncoding = New-Object Text.UTF8Encoding $false
    $json
}
