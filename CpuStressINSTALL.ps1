# 1. Create the service entry.
#    The service will be hosted by svchost.exe in the netsvcs group.
sc.exe create CpuStressSVC type= share binPath= "C:\Windows\System32\svchost.exe -k netsvcs" start= demand

# 2. (Optional) Reconfigure the service in case you need to update any properties.
sc.exe config CpuStressSVC type= share binPath= "C:\Windows\System32\svchost.exe -k netsvcs"

# 3. Set up the Parameters key with the correct ServiceDll path.
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Services\CpuStressSVC\Parameters"
New-Item -Path $regPath -Force | Out-Null
Set-ItemProperty -Path $regPath -Name "ServiceDll" -Value "C:\svc\CpuStressSVC.dll" -Type ExpandString

# 4. Add the service name to the netsvcs group in the Svchost registry key if not there.
$svchostKey = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost"
$netsvcs = (Get-ItemProperty -Path $svchostKey -Name "netsvcs" -ErrorAction SilentlyContinue).netsvcs
if ($netsvcs -notcontains "CpuStressSVC") {
    $netsvcs += "CpuStressSVC"
    Set-ItemProperty -Path $svchostKey -Name "netsvcs" -Value $netsvcs
}

# 5. Verify the service configuration.
Write-Host "Service configuration:"
sc.exe qc CpuStressSVC

Write-Host "`nRegistry ServiceDll path:"
Get-ItemProperty -Path $regPath -Name "ServiceDll"

# 6. Try to start the service.
Write-Host "`nStarting service..."
Start-Service -Name CpuStressSVC

Write-Host "Installation completed."