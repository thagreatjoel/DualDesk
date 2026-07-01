# DualDesk Driver Test Script
Write-Host "DualDesk Driver Test" -ForegroundColor Cyan
Write-Host "====================" -ForegroundColor Cyan
Write-Host ""

# Check service status
$driver = Get-Service -Name "DualDesk" -ErrorAction SilentlyContinue
if ($driver) {
    Write-Host "✅ Driver Service: RUNNING" -ForegroundColor Green
    Write-Host "   Status: $($driver.Status)" -ForegroundColor Green
} else {
    Write-Host "❌ Driver service not found!" -ForegroundColor Red
}

# Check driver file
$path = "C:\Windows\System32\drivers\DualDesk\dualddesk.sys"
if (Test-Path $path) {
    $info = Get-Item $path
    Write-Host "✅ Driver file found: $path" -ForegroundColor Green
    Write-Host "   Size: $($info.Length) bytes" -ForegroundColor Green
} else {
    Write-Host "❌ Driver file not found!" -ForegroundColor Red
}

Write-Host ""
Read-Host "Press Enter to exit"