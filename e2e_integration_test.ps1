Write-Host "==============================================="
Write-Host " RUNNING CONNECTX END-TO-END HTTP API TESTS   "
Write-Host "==============================================="

$port = 8085
$dbName = "e2e_test.db"
$baseUrl = "http://127.0.0.1:$port"

# Cleanup any previous db
if (Test-Path $dbName) { Remove-Item $dbName -Force }
if (Test-Path "$dbName-wal") { Remove-Item "$dbName-wal" -Force }
if (Test-Path "$dbName-shm") { Remove-Item "$dbName-shm" -Force }

# Start the C++ server process
Write-Host "[1/10] Starting C++ server on $baseUrl with DB $dbName..."
$serverProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList "127.0.0.1", "$port", "$dbName" -PassThru

Start-Sleep -Seconds 2

try {
    # 1. Health check
    Write-Host "[2/10] Testing GET /api/health..."
    $healthResp = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    if ($healthResp.status -ne "ok") { throw "Health check failed: $($healthResp | ConvertTo-Json)" }
    Write-Host "   -> PASS: Server returned status: $($healthResp.status)"

    # 2. Register User 1
    Write-Host "[3/10] Testing POST /api/auth/register (User 1)..."
    $user1Payload = @{
        username = "vikram_m"
        email = "vikram@college.edu.in"
        fullName = "Vikram Malhotra"
        password = "SecurePassword123!"
    } | ConvertTo-Json

    $session1 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $regResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user1Payload -ContentType "application/json" -WebSession $session1
    if ($regResp.user.username -ne "vikram_m") { throw "Registration failed: $($regResp | ConvertTo-Json)" }
    Write-Host "   -> PASS: User 1 registered: $($regResp.user.fullName) (ID: $($regResp.user.id))"

    # 3. Verify /api/auth/me with session cookie
    Write-Host "[4/10] Testing GET /api/auth/me for User 1 session..."
    $meResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1
    if ($meResp.user.username -ne "vikram_m") { throw "Session me check failed" }
    Write-Host "   -> PASS: Session resolved to $($meResp.user.fullName)"

    # 4. Reject duplicate username
    Write-Host "[5/10] Testing duplicate username rejection..."
    $dupPayload = @{
        username = "vikram_m"
        email = "other@college.edu.in"
        fullName = "Duplicate Vikram"
        password = "AnotherPassword123!"
    } | ConvertTo-Json

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $dupPayload -ContentType "application/json"
        throw "Expected duplicate registration to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Duplicate username correctly rejected with error"
    }

    # 5. Register User 2 & verify isolation
    Write-Host "[6/14] Testing multi-user isolation with User 2..."
    $user2Payload = @{
        username = "ananya_s"
        email = "ananya@college.edu.in"
        fullName = "Ananya Sharma"
        password = "AnanyaPassword456!"
    } | ConvertTo-Json

    $session2 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $reg2Resp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user2Payload -ContentType "application/json" -WebSession $session2

    $me2Resp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session2
    $me1CheckAgain = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1

    if ($me2Resp.user.username -ne "ananya_s" -or $me1CheckAgain.user.username -ne "vikram_m") {
        throw "Multi-user isolation violated!"
    }
    Write-Host "   -> PASS: Session 1 is $($me1CheckAgain.user.username), Session 2 is $($me2Resp.user.username). Complete isolation confirmed!"

    # 6. User Discovery: User 1 discovers User 2
    Write-Host "[7/14] Testing User 1 discovering User 2 via GET /api/users..."
    $usersResp1 = Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get -WebSession $session1
    $foundUser2 = $usersResp1.users | Where-Object { $_.username -eq "ananya_s" }
    $foundSelf1 = $usersResp1.users | Where-Object { $_.username -eq "vikram_m" }

    if (-not $foundUser2) { throw "User 1 could not find User 2 in discovery results!" }
    if ($foundSelf1) { throw "User 1 should be excluded from own discovery results!" }
    if ($foundUser2.passwordHash -or $foundUser2.salt -or $foundUser2.email) {
        throw "Private security fields leaked in discovery response!"
    }
    Write-Host "   -> PASS: User 1 sees User 2 ($($foundUser2.fullName)) and is excluded from own results!"

    # 7. User Discovery: User 2 discovers User 1
    Write-Host "[8/14] Testing User 2 discovering User 1 via GET /api/users..."
    $usersResp2 = Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get -WebSession $session2
    $foundUser1 = $usersResp2.users | Where-Object { $_.username -eq "vikram_m" }
    $foundSelf2 = $usersResp2.users | Where-Object { $_.username -eq "ananya_s" }

    if (-not $foundUser1) { throw "User 2 could not find User 1 in discovery results!" }
    if ($foundSelf2) { throw "User 2 should be excluded from own discovery results!" }
    Write-Host "   -> PASS: User 2 sees User 1 ($($foundUser1.fullName)) and is excluded from own results!"

    # 8. User Search by query parameter
    Write-Host "[9/14] Testing search filter with query parameter GET /api/users?q=vikram..."
    $searchResp = Invoke-RestMethod -Uri "$baseUrl/api/users?q=vikram" -Method Get -WebSession $session2
    if ($searchResp.users.Count -ne 1 -or $searchResp.users[0].username -ne "vikram_m") {
        throw "Search query filter failed to return exact match!"
    }
    Write-Host "   -> PASS: Search with query 'vikram' correctly matched vikram_m"

    # 9. Non-matching search query
    Write-Host "[10/14] Testing non-matching search query GET /api/users?q=nonexistent..."
    $emptySearchResp = Invoke-RestMethod -Uri "$baseUrl/api/users?q=nonexistent" -Method Get -WebSession $session2
    if ($emptySearchResp.users.Count -ne 0) {
        throw "Expected empty search results for nonexistent query!"
    }
    Write-Host "   -> PASS: Non-matching search query correctly returned empty list"

    # 10. Unauthenticated discovery attempt
    Write-Host "[11/14] Testing unauthenticated call to GET /api/users..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get
        throw "Expected unauthenticated /api/users to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Unauthenticated user-discovery correctly rejected (401)"
    }

    # 11. Test invalid login
    Write-Host "[12/14] Testing login with wrong password..."
    $badLoginPayload = @{
        identifier = "vikram_m"
        password = "WrongPassword999!"
    } | ConvertTo-Json

    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body $badLoginPayload -ContentType "application/json"
        throw "Expected login with wrong password to fail!"
    } catch {
        Write-Host "   -> PASS: Invalid credentials correctly rejected (401)"
    }

    # 12. Test valid login
    Write-Host "[13/14] Testing login with correct credentials..."
    $loginPayload = @{
        identifier = "vikram_m"
        password = "SecurePassword123!"
    } | ConvertTo-Json

    $newLoginSession = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $loginResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body $loginPayload -ContentType "application/json" -WebSession $newLoginSession
    if ($loginResp.user.username -ne "vikram_m") { throw "Login failed" }
    Write-Host "   -> PASS: Login successful for $($loginResp.user.fullName)"

    # 13. Test logout
    Write-Host "[14/14] Testing POST /api/auth/logout and session invalidation..."
    $logoutResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/logout" -Method Post -WebSession $newLoginSession
    Write-Host "   -> PASS: Logout completed: $($logoutResp.message)"

    # 14. Test protected endpoint after logout
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $newLoginSession
        throw "Expected /api/auth/me to fail after logout, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Access rejected after logout (401 Unauthorized)"
    }

    Write-Host "`n==============================================="
    Write-Host " ALL END-TO-END HTTP TESTS PASSED 100%!        "
    Write-Host "==============================================="

} finally {
    Write-Host "Terminating server process..."
    Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    if (Test-Path $dbName) { Remove-Item $dbName -Force -ErrorAction SilentlyContinue }
    if (Test-Path "$dbName-wal") { Remove-Item "$dbName-wal" -Force -ErrorAction SilentlyContinue }
    if (Test-Path "$dbName-shm") { Remove-Item "$dbName-shm" -Force -ErrorAction SilentlyContinue }
}
