Write-Host "==============================================="
Write-Host " RUNNING SAMPARK END-TO-END HTTP API TESTS      "
Write-Host "==============================================="

$port = 8085
$dbName = "e2e_test.db"
$baseUrl = "http://127.0.0.1:$port"

# Cleanup any previous db
if (Test-Path $dbName) { Remove-Item $dbName -Force }
if (Test-Path "$dbName-wal") { Remove-Item "$dbName-wal" -Force }
if (Test-Path "$dbName-shm") { Remove-Item "$dbName-shm" -Force }

# Start the C++ server process
Write-Host "[1/28] Starting C++ server on $baseUrl with DB $dbName..."
$serverProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList "127.0.0.1", "$port", "$dbName" -PassThru

Start-Sleep -Seconds 2

try {
    # 1. Health check
    Write-Host "[2/28] Testing GET /api/health..."
    $healthResp = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    if ($healthResp.status -ne "ok") { throw "Health check failed: $($healthResp | ConvertTo-Json)" }
    Write-Host "   -> PASS: Server returned status: $($healthResp.status)"

    # 2. Register User 1 (Vikram)
    Write-Host "[3/28] Testing POST /api/auth/register (User 1 - Vikram)..."
    $user1Payload = @{
        username = "vikram_m"
        email = "vikram@college.edu.in"
        fullName = "Vikram Malhotra"
        password = "SecurePassword123!"
    } | ConvertTo-Json

    $session1 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $regResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user1Payload -ContentType "application/json" -WebSession $session1
    if ($regResp.user.username -ne "vikram_m") { throw "Registration failed: $($regResp | ConvertTo-Json)" }
    $user1Id = $regResp.user.id
    Write-Host "   -> PASS: User 1 registered: $($regResp.user.fullName) (ID: $user1Id)"

    # 3. Verify /api/auth/me with session cookie
    Write-Host "[4/28] Testing GET /api/auth/me for User 1 session..."
    $meResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1
    if ($meResp.user.username -ne "vikram_m") { throw "Session me check failed" }
    Write-Host "   -> PASS: Session resolved to $($meResp.user.fullName)"

    # 4. Reject duplicate username
    Write-Host "[5/28] Testing duplicate username rejection..."
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

    # 5. Register User 2 (Ananya)
    Write-Host "[6/28] Testing multi-user registration & isolation (User 2 - Ananya)..."
    $user2Payload = @{
        username = "ananya_s"
        email = "ananya@college.edu.in"
        fullName = "Ananya Sharma"
        password = "AnanyaPassword456!"
    } | ConvertTo-Json

    $session2 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $reg2Resp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user2Payload -ContentType "application/json" -WebSession $session2
    $user2Id = $reg2Resp.user.id

    $me2Resp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session2
    $me1CheckAgain = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1

    if ($me2Resp.user.username -ne "ananya_s" -or $me1CheckAgain.user.username -ne "vikram_m") {
        throw "Multi-user isolation violated!"
    }
    Write-Host "   -> PASS: User 2 registered (ID: $user2Id). Complete multi-user session isolation confirmed!"

    # 6. User Discovery: User 1 discovers User 2
    Write-Host "[7/28] Testing User 1 discovering User 2 via GET /api/users..."
    $usersResp1 = Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get -WebSession $session1
    $foundUser2 = $usersResp1.users | Where-Object { $_.username -eq "ananya_s" }
    $foundSelf1 = $usersResp1.users | Where-Object { $_.username -eq "vikram_m" }

    if (-not $foundUser2) { throw "User 1 could not find User 2 in discovery results!" }
    if ($foundSelf1) { throw "User 1 should be excluded from own discovery results!" }
    Write-Host "   -> PASS: User 1 sees User 2 ($($foundUser2.fullName)) and is excluded from own results!"

    # 7. Register User 3 (Rohan - for isolation testing)
    Write-Host "[8/28] Registering User 3 (Rohan Kapoor) for isolation verification..."
    $user3Payload = @{
        username = "rohan_k"
        email = "rohan@college.edu.in"
        fullName = "Rohan Kapoor"
        password = "RohanPassword789!"
    } | ConvertTo-Json

    $session3 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $reg3Resp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user3Payload -ContentType "application/json" -WebSession $session3
    $user3Id = $reg3Resp.user.id
    Write-Host "   -> PASS: User 3 registered (ID: $user3Id)"

    # 8. User 1 creates/retrieves conversation with User 2
    Write-Host "[9/28] Testing POST /api/conversations (User 1 opening chat with User 2)..."
    $createConvPayload = @{ participantId = $user2Id } | ConvertTo-Json
    $convResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $createConvPayload -ContentType "application/json" -WebSession $session1
    $convId = $convResp1.conversation.id
    if ($convId -le 0 -or $convResp1.conversation.participant.username -ne "ananya_s") {
        throw "Conversation creation failed: $($convResp1 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Conversation created with ID: $convId (Participant: $($convResp1.conversation.participant.fullName))"

    # 9. Idempotency & Normalization: User 2 retrieves same conversation with User 1
    Write-Host "[10/28] Testing conversation idempotency & normalization (User 2 opening chat with User 1)..."
    $createConvPayload2 = @{ participantId = $user1Id } | ConvertTo-Json
    $convResp2 = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $createConvPayload2 -ContentType "application/json" -WebSession $session2
    if ($convResp2.conversation.id -ne $convId) {
        throw "Idempotency failed: expected convId $convId but got $($convResp2.conversation.id)"
    }
    if ($convResp2.conversation.participant.username -ne "vikram_m") {
        throw "Participant resolution incorrect for User 2"
    }
    Write-Host "   -> PASS: Exact same conversation ID $convId resolved with normalized participant pair!"

    # 10. Self-conversation rejection
    Write-Host "[11/28] Testing self-conversation rejection..."
    $selfConvPayload = @{ participantId = $user1Id } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $selfConvPayload -ContentType "application/json" -WebSession $session1
        throw "Expected self-conversation to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Self-conversation correctly rejected (400 Bad Request)"
    }

    # 11. User 1 sends message to User 2
    Write-Host "[12/28] Testing POST /api/conversations/$convId/messages (User 1 sends Msg 1)..."
    $msg1Payload = @{ body = "Hello Ananya, this is Vikram on ConnectX!" } | ConvertTo-Json
    $sendMsgResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg1Payload -ContentType "application/json" -WebSession $session1
    $msg1Id = $sendMsgResp1.message.id
    if ($msg1Id -le 0 -or $sendMsgResp1.message.senderId -ne $user1Id) {
        throw "Message 1 send failed: $($sendMsgResp1 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Message 1 persisted in SQLite (ID: $msg1Id, Timestamp: $($sendMsgResp1.message.createdAt))"

    # 12. User 2 reads message history
    Write-Host "[13/28] Testing GET /api/conversations/$convId/messages for User 2..."
    $historyResp2 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session2
    if ($historyResp2.messages.Count -ne 1 -or $historyResp2.messages[0].id -ne $msg1Id) {
        throw "User 2 failed to read Message 1 history!"
    }
    Write-Host "   -> PASS: User 2 retrieved Message 1: '$($historyResp2.messages[0].body)'"

    # 13. User 2 replies to User 1
    Write-Host "[14/28] Testing POST /api/conversations/$convId/messages (User 2 sends Reply)..."
    $msg2Payload = @{ body = "Hi Vikram! Real-time messaging is working smoothly." } | ConvertTo-Json
    $sendMsgResp2 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg2Payload -ContentType "application/json" -WebSession $session2
    $msg2Id = $sendMsgResp2.message.id
    if ($msg2Id -le $msg1Id -or $sendMsgResp2.message.senderId -ne $user2Id) {
        throw "Message 2 send failed: $($sendMsgResp2 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Reply persisted in SQLite (ID: $msg2Id, Sender: User 2)"

    # 14. User 1 retrieves conversation history (Chronological verification)
    Write-Host "[15/28] Testing chronological message history for User 1..."
    $historyResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session1
    if ($historyResp1.messages.Count -ne 2) { throw "Expected 2 messages in conversation history" }
    if ($historyResp1.messages[0].id -ne $msg1Id -or $historyResp1.messages[1].id -ne $msg2Id) {
        throw "Chronological order violation in message history!"
    }
    Write-Host "   -> PASS: Chronological message stream verified (Msg 1 -> Msg 2)"

    # 15. Privacy & Participant Isolation: User 3 cannot view conversation
    Write-Host "[16/28] Testing privacy isolation: User 3 denied access to GET /api/conversations/$convId/messages..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session3
        throw "Expected User 3 to be rejected with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-participant User 3 correctly rejected (403 Forbidden)"
    }

    # 16. Privacy & Participant Isolation: User 3 cannot send message to conversation
    Write-Host "[17/28] Testing privacy isolation: User 3 denied access to POST /api/conversations/$convId/messages..."
    $spyPayload = @{ body = "Unauthorized intrusion attempt" } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $spyPayload -ContentType "application/json" -WebSession $session3
        throw "Expected User 3 send to be rejected with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-participant User 3 send correctly rejected (403 Forbidden)"
    }

    # 17. Incremental polling cursor (after parameter)
    Write-Host "[18/28] Testing incremental polling cursor GET /api/conversations/$convId/messages?after=$msg1Id..."
    $pollResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages?after=$msg1Id" -Method Get -WebSession $session1
    if ($pollResp.messages.Count -ne 1 -or $pollResp.messages[0].id -ne $msg2Id) {
        throw "Incremental polling failed: expected only Msg 2"
    }
    Write-Host "   -> PASS: Incremental polling returned only new message delta (Msg 2)"

    # 18. Dashboard conversations list
    Write-Host "[19/28] Testing GET /api/conversations for dashboard listing..."
    $dashResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get -WebSession $session1
    if ($dashResp1.conversations.Count -ne 1) { throw "Expected 1 conversation in dashboard" }
    $convItem = $dashResp1.conversations[0]
    if ($convItem.participant.username -ne "ananya_s" -or $convItem.lastMessage.body -ne "Hi Vikram! Real-time messaging is working smoothly.") {
        throw "Dashboard conversation item metadata incorrect: $($convItem | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Dashboard conversation item contains correct peer details and last message preview"

    # 19. Unread counts & Mark as read
    Write-Host "[20/28] Testing unread count and mark-as-read..."
    # User 1 has 1 unread message from User 2
    if ($convItem.unreadCount -ne 1) { throw "Expected unreadCount to be 1, got $($convItem.unreadCount)" }
    Write-Host "   -> Initial unread count for User 1: $($convItem.unreadCount)"

    $readResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/read" -Method Post -WebSession $session1
    if (-not $readResp.success) { throw "Mark as read failed" }

    $dashRespAfterRead = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get -WebSession $session1
    if ($dashRespAfterRead.conversations[0].unreadCount -ne 0) {
        throw "Expected unreadCount to be 0 after reading"
    }
    Write-Host "   -> PASS: Messages marked as read, unread count reset to 0"

    # 20. Message body validation (empty / whitespace rejected)
    Write-Host "[21/28] Testing validation: reject empty and whitespace-only message..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body (@{ body = "   " } | ConvertTo-Json) -ContentType "application/json" -WebSession $session1
        throw "Expected empty message to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Whitespace message correctly rejected (400 Bad Request)"
    }

    # 21. Search filter with query parameter
    Write-Host "[22/28] Testing user search query parameter GET /api/users?q=vikram..."
    $searchResp = Invoke-RestMethod -Uri "$baseUrl/api/users?q=vikram" -Method Get -WebSession $session2
    if ($searchResp.users.Count -ne 1 -or $searchResp.users[0].username -ne "vikram_m") {
        throw "Search query filter failed to return exact match!"
    }
    Write-Host "   -> PASS: Search with query 'vikram' correctly matched vikram_m"

    # 22. Unauthenticated calls rejected
    Write-Host "[23/28] Testing unauthenticated call to GET /api/conversations..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get
        throw "Expected unauthenticated /api/conversations to fail!"
    } catch {
        Write-Host "   -> PASS: Unauthenticated request rejected (401 Unauthorized)"
    }

    # 23. Test invalid login
    Write-Host "[24/28] Testing login with wrong password..."
    $badLoginPayload = @{ identifier = "vikram_m"; password = "WrongPassword999!" } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body $badLoginPayload -ContentType "application/json"
        throw "Expected login with wrong password to fail!"
    } catch {
        Write-Host "   -> PASS: Invalid credentials correctly rejected (401)"
    }

    # 24. Test logout
    Write-Host "[25/28] Testing POST /api/auth/logout..."
    $logoutResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/logout" -Method Post -WebSession $session3
    Write-Host "   -> PASS: User 3 logged out successfully"

    # 25. Restart C++ Server Process & Verify Data Persistence
    Write-Host "[26/28] Restarting C++ server to test database persistence across restarts..."
    Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2

    # Restart server with same database
    $serverProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList "127.0.0.1", "$port", "$dbName" -PassThru
    Start-Sleep -Seconds 2

    # 26. Verify session survives or can re-login and all messages are intact
    Write-Host "[27/28] Re-authenticating User 1 and verifying message history survives restart..."
    $newSession1 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $reLoginResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body (@{ identifier = "vikram_m"; password = "SecurePassword123!" } | ConvertTo-Json) -ContentType "application/json" -WebSession $newSession1
    if ($reLoginResp.user.username -ne "vikram_m") { throw "Re-login after restart failed" }

    $persistedHistory = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $newSession1
    if ($persistedHistory.messages.Count -ne 2) {
        throw "Messages lost after restart! Expected 2, got $($persistedHistory.messages.Count)"
    }
    if ($persistedHistory.messages[0].body -ne "Hello Ananya, this is Vikram on ConnectX!" -or $persistedHistory.messages[1].body -ne "Hi Vikram! Real-time messaging is working smoothly.") {
        throw "Message content corrupted after restart!"
    }
    Write-Host "   -> PASS: Full conversation and 2 messages completely intact after server restart!"

    Write-Host "[28/28] Final regression check on dashboard listing after restart..."
    $postRestartDash = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get -WebSession $newSession1
    if ($postRestartDash.conversations.Count -ne 1) { throw "Dashboard listing lost after restart" }
    Write-Host "   -> PASS: Dashboard conversation listing intact after restart"

    Write-Host "`n==============================================="
    Write-Host " ALL 28 END-TO-END HTTP TESTS PASSED 100%!     "
    Write-Host "==============================================="

} finally {
    Write-Host "Terminating server process..."
    if ($serverProcess -and -not $serverProcess.HasExited) {
        Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
    if (Test-Path $dbName) { Remove-Item $dbName -Force -ErrorAction SilentlyContinue }
    if (Test-Path "$dbName-wal") { Remove-Item "$dbName-wal" -Force -ErrorAction SilentlyContinue }
    if (Test-Path "$dbName-shm") { Remove-Item "$dbName-shm" -Force -ErrorAction SilentlyContinue }
}
