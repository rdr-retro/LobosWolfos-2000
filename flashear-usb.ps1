#Requires -Version 5.1
<#
.SYNOPSIS
    Escribe la imagen del juego de LobosWolfos 2000 (loboswolfos.img) en un
    pendrive, en crudo.

.DESCRIPTION
    Es la version de loboswolfos de Rufus en "modo DD": escribe la imagen
    byte a byte sobre el dispositivo fisico, sin montar sistemas de ficheros
    ni interpretar nada. El pendrive queda arrancable por BIOS (MBR) y el
    juego del editor arranca desde la BIOS sin Windows.

    La imagen la genera el editor con el menu COMPILE, "Compilar para
    BIOS" (o el xtask de loboswolfos con `build`); el script solo la copia.

    El script se eleva solo (hace falta ser administrador para escribir en
    \\.\PhysicalDriveN) y nunca toca el disco 0. Basado en rocky-usb.ps1
    de rocky.

.PARAMETER Imagen
    Ruta de la imagen a escribir. Por defecto, `loboswolfos.img` junto al
    script.

.PARAMETER Disco
    Numero del disco fisico de destino. Si no se indica, se pregunta.

.EXAMPLE
    .\flashear-usb.ps1
    .\flashear-usb.ps1 -Disco 2
    .\flashear-usb.ps1 -Imagen .\mi-juego.img -Disco 2
#>
[CmdletBinding()]
param(
    [string]$Imagen,
    [int]$Disco = -1
)

$ErrorActionPreference = 'Stop'

# ------------------------------------------------------------
# 0) Elevarse si hace falta
# ------------------------------------------------------------
$esAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $esAdmin) {
    Write-Host "loboswolfos-usb: hacen falta permisos de administrador; relanzando..." -ForegroundColor Yellow
    $mi = $MyInvocation.MyCommand.Path
    $reargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$mi`"")
    if ($Imagen) { $reargs += @('-Imagen', "`"$Imagen`"") }
    if ($Disco -ge 0) { $reargs += @('-Disco', $Disco) }
    # -Wait es imprescindible: sin el, quien lanzo este script (flashear.bat
    # o el propio usuario) recupera el control al instante, no ve el
    # resultado de la escritura y cree que todo fue bien aunque el proceso
    # elevado siga (o haya dejado) a medias.
    $hijo = Start-Process -FilePath 'powershell.exe' -ArgumentList $reargs -Verb RunAs -Wait -PassThru
    exit $hijo.ExitCode
}

# ------------------------------------------------------------
# 1) Localizar la imagen
# ------------------------------------------------------------
if (-not $Imagen) {
    $Imagen = Join-Path $PSScriptRoot 'loboswolfos.img'
}
if (-not (Test-Path -LiteralPath $Imagen)) {
    Write-Host "loboswolfos-usb: no encuentro la imagen '$Imagen'." -ForegroundColor Red
    Write-Host '           compila el juego en el editor con el menu COMPILE,'
    Write-Host '           "Compilar para BIOS" (o el xtask de loboswolfos).'
    exit 1
}
$Imagen = (Resolve-Path -LiteralPath $Imagen).Path
$tamImagen = (Get-Item -LiteralPath $Imagen).Length
Write-Host ("loboswolfos-usb: imagen {0} ({1:N0} bytes, {2:N1} MiB)" -f $Imagen, $tamImagen, ($tamImagen / 1MB)) -ForegroundColor Cyan

# ------------------------------------------------------------
# 2) Enumerar discos fisicos
# ------------------------------------------------------------
$discos = @(Get-CimInstance Win32_DiskDrive | Where-Object {
        $_.InterfaceType -eq 'USB' -or $_.MediaType -like '*Removable*'
    } | Sort-Object Index)

if ($discos.Count -eq 0) {
    Write-Host 'loboswolfos-usb: no veo ningun disco extraible USB conectado.' -ForegroundColor Red
    exit 1
}

Write-Host ''
Write-Host 'Discos extraibles disponibles:' -ForegroundColor Cyan
Write-Host '  #  Modelo                            Tamano        Serial'
Write-Host '  -- --------------------------------- ------------  --------------------'
foreach ($d in $discos) {
    $gb = if ($d.Size) { [double]$d.Size / 1GB } else { 0 }
    $serial = if ($d.SerialNumber) { $d.SerialNumber.Trim() } else { '-' }
    Write-Host ('  {0,2} {1,-33} {2,9:N1} GB  {3}' -f $d.Index, $d.Model, $gb, $serial)
}
Write-Host ''

# ------------------------------------------------------------
# 3) Elegir destino
# ------------------------------------------------------------
if ($Disco -lt 0) {
    $resp = Read-Host 'Numero de disco de destino (Enter para cancelar)'
    if ([string]::IsNullOrWhiteSpace($resp)) {
        Write-Host 'loboswolfos-usb: cancelado.' -ForegroundColor Yellow
        exit 1
    }
    $Disco = [int]$resp
}
if ($Disco -eq 0) {
    Write-Host 'loboswolfos-usb: el disco 0 es el del sistema; no lo toco.' -ForegroundColor Red
    exit 1
}
$destino = $discos | Where-Object { $_.Index -eq $Disco } | Select-Object -First 1
if (-not $destino) {
    Write-Host "loboswolfos-usb: el disco $Disco no esta en la lista de extraibles." -ForegroundColor Red
    exit 1
}

$letras = @(Get-CimInstance Win32_LogicalDisk | Where-Object { $_.DeviceID } |
    ForEach-Object {
        $dsk = Get-CimInstance -Query "ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='$($_.DeviceID)'} WHERE AssocClass=Win32_LogicalDiskToPartition" -ErrorAction SilentlyContinue
        foreach ($p in $dsk) {
            $part = Get-CimInstance -Query "ASSOCIATORS OF {Win32_DiskPartition.DeviceID='$($p.DeviceID)'} WHERE AssocClass=Win32_DiskDriveToDiskPartition" -ErrorAction SilentlyContinue
            if ($part -and $part.Index -eq $Disco) { $_.DeviceID }
        }
    }) | Select-Object -Unique

Write-Host ''
Write-Host '  ATENCION: se va a BORRAR POR COMPLETO este disco:' -ForegroundColor Red
Write-Host ("    #{0}  {1}  ({2:N1} GB)" -f $destino.Index, $destino.Model, ($destino.Size / 1GB)) -ForegroundColor Red
if ($letras.Count -gt 0) {
    Write-Host ("    Unidades montadas: {0}" -f ($letras -join ', ')) -ForegroundColor Red
    Write-Host '    Se perderan TODOS los ficheros de esas unidades (fotos incluidas).' -ForegroundColor Red
}
Write-Host ''

$r = Read-Host 'Escribe BORRAR (en mayusculas) para continuar'
if ($r -cne 'BORRAR') {
    Write-Host 'loboswolfos-usb: cancelado.' -ForegroundColor Yellow
    exit 1
}

# ------------------------------------------------------------
# 4) Dejar el disco escribible en crudo
# ------------------------------------------------------------
# Windows NO deja escribir en crudo sobre la zona que ocupa un volumen montado.
# La particion del juego se puede volver a montar sola (por GUID, sin letra
# de unidad), asi que toda escritura a partir de 1 MiB falla con "Access to
# the path is denied". Hay que desmontar TODOS los volumenes del disco
# destino, con o sin letra.

$discoInfo = Get-Disk -Number $Disco -ErrorAction SilentlyContinue
if ($discoInfo -and $discoInfo.IsReadOnly) {
    Write-Host 'loboswolfos-usb: el disco esta marcado como solo lectura; quitandolo...' -ForegroundColor Yellow
    try {
        Set-Disk -Number $Disco -IsReadOnly $false -ErrorAction Stop
    } catch {
        Write-Host ("loboswolfos-usb: no se pudo quitar: {0}" -f $_.Exception.Message) -ForegroundColor Red
        Write-Host '  Mueve a la posicion DESBLOQUEADO el interruptor del lateral de la SD.'
        exit 2
    }
}

# Desmontar volumenes con letra asignada
foreach ($l in $letras) {
    if ([string]::IsNullOrWhiteSpace($l)) { continue }
    Write-Host "loboswolfos-usb: desmontando $l ..."
    & mountvol.exe $l /P 2>$null | Out-Null
}

# Desmontar TODOS los volumenes asociados al disco destino (con o sin letra)
$volsADesmontar = @()
try {
    $parts = Get-Partition -DiskNumber $Disco -ErrorAction SilentlyContinue
    foreach ($p in $parts) {
        $v = Get-Volume -Partition $p -ErrorAction SilentlyContinue
        if ($v) { $volsADesmontar += $v }
    }
} catch { }
# Y tambien por Win32_Volume extraibles que puedan no aparecer por asociacion
try {
    $win32v = Get-CimInstance Win32_Volume -ErrorAction SilentlyContinue | Where-Object { $_.DriveType -eq 2 }
    foreach ($wv in $win32v) {
        $ya = $volsADesmontar | Where-Object { $_.Path -eq $wv.DeviceID }
        if (-not $ya) {
            $vobj = Get-Volume -Path $wv.DeviceID -ErrorAction SilentlyContinue
            if ($vobj) { $volsADesmontar += $vobj }
        }
    }
} catch { }

$volsADesmontar = $volsADesmontar | Sort-Object -Unique -Property Path
foreach ($v in $volsADesmontar) {
    $rutaVol = $v.Path
    if (-not $rutaVol) { continue }
    Write-Host ("loboswolfos-usb: desmontando volumen {0} ..." -f $rutaVol)
    # Forzar desmontaje (remove mount points). mountvol /P funciona incluso sin letra
    & mountvol.exe $rutaVol /P 2>&1 | Out-Null
    # Intento extra por CIM/WMI por si queda algun handle
    try {
        $cimv = Get-CimInstance Win32_Volume -Filter "DeviceID='$rutaVol'" -ErrorAction SilentlyContinue
        if ($cimv) { $null = $cimv | Invoke-CimMethod -MethodName Dismount -Arguments @{Force=$true} -ErrorAction SilentlyContinue }
    } catch { }
}

# Pequeno respiro para que PnP/libere handles
Start-Sleep -Seconds 2

# Verificar: si sigue habiendo algun volumen montado en ese disco, volvemos a intentar
try {
    $resto = @(Get-Partition -DiskNumber $Disco -ErrorAction SilentlyContinue | Get-Volume -ErrorAction SilentlyContinue)
    if ($resto.Count -gt 0) {
        Write-Host ("loboswolfos-usb: aun hay {0} volumen(es) montados; reintentando desmontaje..." -f $resto.Count) -ForegroundColor Yellow
        foreach ($v in $resto) {
            if ($v.Path) { & mountvol.exe $v.Path /P 2>&1 | Out-Null }
        }
        Start-Sleep -Seconds 2
    }
} catch { }

# La imagen cruda ya contiene el MBR y la tabla de particiones definitivos.
# No hacemos Clear-Disk aqui: algunos lectores SD reinicializan la tarjeta al
# recibir ese IOCTL y despues devuelven "El dispositivo no esta listo" al
# primer Write. Con todos los volumenes desmontados evitamos el bloqueo por
# handles.
Write-Host 'loboswolfos-usb: tabla de particiones incluida en la imagen; no hace falta Clear-Disk.'
Start-Sleep -Milliseconds 500

# ------------------------------------------------------------
# 5) Escribir la imagen en crudo
# ------------------------------------------------------------
$ruta = "\\.\PhysicalDrive$Disco"
Write-Host "loboswolfos-usb: escribiendo en $ruta ..." -ForegroundColor Cyan

$total = $tamImagen
$bloque = 1MB
$buf = New-Object byte[] $bloque
$escrito = $false
$ultimoError = $null
$maxPasadas = 5

# Si el lector se reinicia durante el primer Write, hay que cerrar el handle,
# esperar a que vuelva a presentar el medio y repetir desde el principio. No se
# reanuda desde el offset anterior: asi nunca queda una imagen mezclada.
for ($pasada = 1; $pasada -le $maxPasadas -and -not $escrito; $pasada++) {
    $orig = $null
    $dev = $null
    $hecho = 0
    try {
        if ($pasada -gt 1) {
            Write-Host ("loboswolfos-usb: el lector aun no esta listo; reintento de escritura {0} de {1}..." -f $pasada, $maxPasadas) -ForegroundColor Yellow
            try { Update-HostStorageCache -ErrorAction SilentlyContinue } catch { }
            Start-Sleep -Seconds 3
        }

        $orig = [IO.File]::OpenRead($Imagen)
        for ($intento = 1; $intento -le 8; $intento++) {
            try {
                $dev = New-Object IO.FileStream($ruta, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None, 1MB, [IO.FileOptions]::WriteThrough)
                break
            } catch {
                if ($intento -eq 8) { throw }
                Write-Host ("loboswolfos-usb: esperando al dispositivo; reintento {0} de 8..." -f $intento) -ForegroundColor Yellow
                Start-Sleep -Milliseconds 1000
            }
        }
        if (-not $dev) { throw "no se pudo abrir $ruta para escritura" }

        $reloj = [Diagnostics.Stopwatch]::StartNew()
        while (($leido = $orig.Read($buf, 0, $bloque)) -gt 0) {
            $dev.Write($buf, 0, $leido)
            $hecho += $leido
            if ($reloj.ElapsedMilliseconds -gt 300) {
                $pct = [math]::Floor($hecho * 100 / $total)
                Write-Host ("`r  {0,3}%  ({1:N1} / {2:N1} MiB)" -f $pct, ($hecho / 1MB), ($total / 1MB)) -NoNewline
                $reloj.Restart()
            }
        }
        $dev.Flush($true)
        $escrito = $true
        Write-Host ("`r  100%  ({0:N1} MiB escritos)          " -f ($total / 1MB))
    } catch {
        $ultimoError = $_.Exception.Message
        Write-Host ''
        Write-Host ("loboswolfos-usb: fallo al escribir (pasada {0}): {1}" -f $pasada, $ultimoError) -ForegroundColor Yellow
        if ($pasada -lt $maxPasadas) {
            Write-Host 'loboswolfos-usb: cerrando el dispositivo y esperando a que el lector se reinicialice...' -ForegroundColor Yellow
        }
    } finally {
        if ($dev) { $dev.Dispose(); $dev = $null }
        if ($orig) { $orig.Dispose(); $orig = $null }
    }
}

if (-not $escrito) {
    Write-Host ''
    Write-Host ("loboswolfos-usb: no se pudo escribir la imagen despues de {0} intentos: {1}" -f $maxPasadas, $ultimoError) -ForegroundColor Red
    Write-Host '  Extrae y vuelve a insertar la tarjeta, o prueba otro lector/puerto USB.'
    exit 2
}

# Algunos lectores de tarjetas USB responden "hecho" desde su cache interna y
# solo bajan los datos al medio un poco despues. Volvemos a desmontar y damos
# un respiro antes de releer, para verificar lo que hay de verdad en el
# pendrive.
Start-Sleep -Milliseconds 600
foreach ($l in $letras) { & mountvol.exe $l /P 2>$null | Out-Null }
Start-Sleep -Milliseconds 600

# ------------------------------------------------------------
# 6) Verificar releyendo del dispositivo
# ------------------------------------------------------------
Write-Host 'loboswolfos-usb: verificando (releyendo del pendrive)...' -ForegroundColor Cyan

# Comparamos bloque a bloque (no solo un SHA global): asi, si algo no cuadra,
# sabemos exactamente donde.
$ver = New-Object IO.FileStream($ruta, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
$imgStream = [IO.File]::OpenRead($Imagen)
$ib = New-Object byte[] $bloque
$db = New-Object byte[] $bloque
$md5 = [Security.Cryptography.MD5]::Create()
$chunkDiff = 0
$totalChunks = 0
$primerDiff = -1
for ($off = 0; $off -lt $tamImagen; $off += $bloque) {
    $n = [math]::Min([int]$bloque, [int]($tamImagen - $off))
    $a = 0; while ($a -lt $n) { $k = $imgStream.Read($ib, $a, $n - $a); if ($k -le 0) { break }; $a += $k }
    $b = 0; while ($b -lt $n) { $k = $ver.Read($db, $b, $n - $b); if ($k -le 0) { break }; $b += $k }
    $totalChunks++
    if (([BitConverter]::ToString($md5.ComputeHash($ib, 0, $n))) -ne ([BitConverter]::ToString($md5.ComputeHash($db, 0, $n)))) {
        $chunkDiff++
        if ($primerDiff -lt 0) { $primerDiff = $off }
    }
}
$imgStream.Dispose(); $ver.Dispose(); $md5.Dispose()

Write-Host ''
if ($chunkDiff -eq 0) {
    Write-Host 'loboswolfos-usb: OK - el pendrive coincide con la imagen.' -ForegroundColor Green
    Write-Host ("           {0} bloques de 1 MiB verificados." -f $totalChunks)
    Write-Host ''
    Write-Host 'Ya puedes arrancar el juego:' -ForegroundColor Cyan
    Write-Host '  - Enciende el PC con el pendrive puesto y elige el USB como'
    Write-Host '    disco de arranque en el menu de la BIOS.'
    exit 0
} else {
    Write-Host 'loboswolfos-usb: la verificacion NO coincide.' -ForegroundColor Red
    Write-Host ("  bloques distintos: {0} de {1}" -f $chunkDiff, $totalChunks)
    Write-Host ("  primer bloque distinto: offset {0}  (LBA {1})" -f $primerDiff, ($primerDiff / 512))
    Write-Host '  Prueba con otro pendrive o repite la operacion.'
    exit 2
}
