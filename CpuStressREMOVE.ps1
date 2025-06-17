# 1. Stop the service if it's running. Ignore errors if it's not running.
Stop-Service -Name CpuStressSVC -ErrorAction SilentlyContinue

# 2. Delete the service entry.
sc.exe delete CpuStressSVC

# Give the system time to process the deletion (optional).
Start-Sleep -Seconds 2

# 3. Remove the service's registry key entirely.
$serviceRegKey = "HKLM:\SYSTEM\CurrentControlSet\Services\CpuStressSVC"
if (Test-Path $serviceRegKey) {
    Remove-Item -Path $serviceRegKey -Recurse -Force
    Write-Host "Removed registry key: $serviceRegKey"
} else {
    Write-Host "Registry key $serviceRegKey not found."
}

# 4. Remove the service name from the netsvcs group in the Svchost registry key.
$svchostKey = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost"
try {
    $netsvcs = (Get-ItemProperty -Path $svchostKey -Name "netsvcs" -ErrorAction Stop).netsvcs
    if ($netsvcs -contains "CpuStressSVC") {
        $updatedNetsvcs = $netsvcs | Where-Object { $_ -ne "CpuStressSVC" }
        Set-ItemProperty -Path $svchostKey -Name "netsvcs" -Value $updatedNetsvcs
        Write-Host "Removed 'CpuStressSVC' from netsvcs group."
    } else {
        Write-Host "'CpuStressSVC' was not found in netsvcs group."
    }
} catch {
    Write-Host "Error processing the Svchost key: $_"
}

Write-Host "Service CpuStressSVC has been successfully removed."