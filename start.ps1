# Sampark Local Host Script
# Starts both the C++ backend server and the React/Vite frontend

$hostAddress = "127.0.0.1"
$port = 8080
$dbPath = "connectx.db"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "       Sampark Local Development         " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# 1. Build backend if server.exe does not exist
if (-not (Test-Path ".\backend\server.exe")) {
    Write-Host "[1/3] Building C++ backend server..." -ForegroundColor Yellow
    Push-Location backend
    mingw32-make all
    Pop-Location
} else {
    Write-Host "[1/3] C++ backend executable found." -ForegroundColor Green
}

# 2. Stop any existing server process on port 8080
Get-Process -Name server -ErrorAction SilentlyContinue | Stop-Process -Force

# 3. Start C++ backend
Write-Host "[2/3] Starting C++ backend on http://$hostAddress`:$port..." -ForegroundColor Yellow
$backendProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList $hostAddress, "$port", $dbPath -PassThru

Start-Sleep -Seconds 2

# Check if backend is alive
try {
    $health = Invoke-RestMethod -Uri "http://$hostAddress`:$port/api/health" -Method Get -TimeoutSec 3
    if ($health.status -eq "ok") {
        Write-Host "   -> C++ Backend is ACTIVE (PID: $($backendProcess.Id))" -ForegroundColor Green
    }
} catch {
    Write-Host "   -> WARNING: Backend health check did not respond immediately." -ForegroundColor Red
}

# 4. Start frontend Vite dev server
Write-Host "[3/3] Starting Frontend Dev Server on http://localhost:5173..." -ForegroundColor Yellow
Write-Host "-----------------------------------------" -ForegroundColor Cyan
Write-Host "Press Ctrl+C to terminate both servers." -ForegroundColor Gray
Write-Host "-----------------------------------------" -ForegroundColor Cyan

try {
    Push-Location frontend
    npm run dev
} finally {
    Pop-Location
    Write-Host "`nStopping C++ backend server..." -ForegroundColor Yellow
    if ($backendProcess -and -not $backendProcess.HasExited) {
        Stop-Process -Id $backendProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Write-Host "All services stopped." -ForegroundColor Green
}
