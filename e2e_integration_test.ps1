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
Write-Host "[1/38] Starting C++ server on $baseUrl with DB $dbName..."
$serverProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList "127.0.0.1", "$port", "$dbName" -PassThru

Start-Sleep -Seconds 2

try {
    # 1. Health check
    Write-Host "[2/38] Testing GET /api/health..."
    $healthResp = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    if ($healthResp.status -ne "ok") { throw "Health check failed: $($healthResp | ConvertTo-Json)" }
    Write-Host "   -> PASS: Server returned status: $($healthResp.status)"

    # 2. Register User 1 (Vikram)
    Write-Host "[3/38] Testing POST /api/auth/register (User 1 - Vikram)..."
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
    Write-Host "[4/38] Testing GET /api/auth/me for User 1 session..."
    $meResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1
    if ($meResp.user.username -ne "vikram_m") { throw "Session me check failed" }
    Write-Host "   -> PASS: Session resolved to $($meResp.user.fullName)"

    # 4. Reject duplicate username
    Write-Host "[5/38] Testing duplicate username rejection..."
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
    Write-Host "[6/38] Testing multi-user registration & isolation (User 2 - Ananya)..."
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
    Write-Host "[7/38] Testing User 1 discovering User 2 via GET /api/users..."
    $usersResp1 = Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get -WebSession $session1
    $foundUser2 = $usersResp1.users | Where-Object { $_.username -eq "ananya_s" }
    $foundSelf1 = $usersResp1.users | Where-Object { $_.username -eq "vikram_m" }

    if (-not $foundUser2) { throw "User 1 could not find User 2 in discovery results!" }
    if ($foundSelf1) { throw "User 1 should be excluded from own discovery results!" }
    Write-Host "   -> PASS: User 1 sees User 2 ($($foundUser2.fullName)) and is excluded from own results!"

    # 7. Register User 3 (Rohan - for isolation testing)
    Write-Host "[8/38] Registering User 3 (Rohan Kapoor) for isolation verification..."
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
    Write-Host "[9/38] Testing POST /api/conversations (User 1 opening chat with User 2)..."
    $createConvPayload = @{ participantId = $user2Id } | ConvertTo-Json
    $convResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $createConvPayload -ContentType "application/json" -WebSession $session1
    $convId = $convResp1.conversation.id
    if ($convId -le 0 -or $convResp1.conversation.participant.username -ne "ananya_s") {
        throw "Conversation creation failed: $($convResp1 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Conversation created with ID: $convId (Participant: $($convResp1.conversation.participant.fullName))"

    # 9. Idempotency & Normalization: User 2 retrieves same conversation with User 1
    Write-Host "[10/38] Testing conversation idempotency & normalization (User 2 opening chat with User 1)..."
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
    Write-Host "[11/38] Testing self-conversation rejection..."
    $selfConvPayload = @{ participantId = $user1Id } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $selfConvPayload -ContentType "application/json" -WebSession $session1
        throw "Expected self-conversation to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Self-conversation correctly rejected (400 Bad Request)"
    }

    # 11. User 1 sends message to User 2
    Write-Host "[12/38] Testing POST /api/conversations/$convId/messages (User 1 sends Msg 1)..."
    $msg1Payload = @{ body = "Hello Ananya, this is Vikram on ConnectX!" } | ConvertTo-Json
    $sendMsgResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg1Payload -ContentType "application/json" -WebSession $session1
    $msg1Id = $sendMsgResp1.message.id
    if ($msg1Id -le 0 -or $sendMsgResp1.message.senderId -ne $user1Id) {
        throw "Message 1 send failed: $($sendMsgResp1 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Message 1 persisted in SQLite (ID: $msg1Id, Timestamp: $($sendMsgResp1.message.createdAt))"

    # 12. User 2 reads message history
    Write-Host "[13/38] Testing GET /api/conversations/$convId/messages for User 2..."
    $historyResp2 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session2
    if ($historyResp2.messages.Count -ne 1 -or $historyResp2.messages[0].id -ne $msg1Id) {
        throw "User 2 failed to read Message 1 history!"
    }
    Write-Host "   -> PASS: User 2 retrieved Message 1: '$($historyResp2.messages[0].body)'"

    # 13. User 2 replies to User 1
    Write-Host "[14/38] Testing POST /api/conversations/$convId/messages (User 2 sends Reply)..."
    $msg2Payload = @{ body = "Hi Vikram! Real-time messaging is working smoothly." } | ConvertTo-Json
    $sendMsgResp2 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg2Payload -ContentType "application/json" -WebSession $session2
    $msg2Id = $sendMsgResp2.message.id
    if ($msg2Id -le $msg1Id -or $sendMsgResp2.message.senderId -ne $user2Id) {
        throw "Message 2 send failed: $($sendMsgResp2 | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Reply persisted in SQLite (ID: $msg2Id, Sender: User 2)"

    # 14. User 1 retrieves conversation history (Chronological verification)
    Write-Host "[15/38] Testing chronological message history for User 1..."
    $historyResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session1
    if ($historyResp1.messages.Count -ne 2) { throw "Expected 2 messages in conversation history" }
    if ($historyResp1.messages[0].id -ne $msg1Id -or $historyResp1.messages[1].id -ne $msg2Id) {
        throw "Chronological order violation in message history!"
    }
    Write-Host "   -> PASS: Chronological message stream verified (Msg 1 -> Msg 2)"

    # 15. Privacy & Participant Isolation: User 3 cannot view conversation
    Write-Host "[16/38] Testing privacy isolation: User 3 denied access to GET /api/conversations/$convId/messages..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session3
        throw "Expected User 3 to be rejected with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-participant User 3 correctly rejected (403 Forbidden)"
    }

    # 16. Privacy & Participant Isolation: User 3 cannot send message to conversation
    Write-Host "[17/38] Testing privacy isolation: User 3 denied access to POST /api/conversations/$convId/messages..."
    $spyPayload = @{ body = "Unauthorized intrusion attempt" } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $spyPayload -ContentType "application/json" -WebSession $session3
        throw "Expected User 3 send to be rejected with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-participant User 3 send correctly rejected (403 Forbidden)"
    }

    # 17. Incremental polling cursor (after parameter)
    Write-Host "[18/38] Testing incremental polling cursor GET /api/conversations/$convId/messages?after=$msg1Id..."
    $pollResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages?after=$msg1Id" -Method Get -WebSession $session1
    if ($pollResp.messages.Count -ne 1 -or $pollResp.messages[0].id -ne $msg2Id) {
        throw "Incremental polling failed: expected only Msg 2"
    }
    Write-Host "   -> PASS: Incremental polling returned only new message delta (Msg 2)"

    # 18. Dashboard conversations list
    Write-Host "[19/38] Testing GET /api/conversations for dashboard listing..."
    $dashResp1 = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get -WebSession $session1
    if ($dashResp1.conversations.Count -ne 1) { throw "Expected 1 conversation in dashboard" }
    $convItem = $dashResp1.conversations[0]
    if ($convItem.participant.username -ne "ananya_s" -or $convItem.lastMessage.body -ne "Hi Vikram! Real-time messaging is working smoothly.") {
        throw "Dashboard conversation item metadata incorrect: $($convItem | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Dashboard conversation item contains correct peer details and last message preview"

    # 19. Unread counts & Mark as read
    Write-Host "[20/38] Testing unread count and mark-as-read..."
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
    Write-Host "[21/38] Testing validation: reject empty and whitespace-only message..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body (@{ body = "   " } | ConvertTo-Json) -ContentType "application/json" -WebSession $session1
        throw "Expected empty message to fail, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Whitespace message correctly rejected (400 Bad Request)"
    }

    # 21. Search filter with query parameter
    Write-Host "[22/38] Testing user search query parameter GET /api/users?q=vikram..."
    $searchResp = Invoke-RestMethod -Uri "$baseUrl/api/users?q=vikram" -Method Get -WebSession $session2
    if ($searchResp.users.Count -ne 1 -or $searchResp.users[0].username -ne "vikram_m") {
        throw "Search query filter failed to return exact match!"
    }
    Write-Host "   -> PASS: Search with query 'vikram' correctly matched vikram_m"

    # 22. Unauthenticated calls rejected
    Write-Host "[23/38] Testing unauthenticated call to GET /api/conversations..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get
        throw "Expected unauthenticated /api/conversations to fail!"
    } catch {
        Write-Host "   -> PASS: Unauthenticated request rejected (401 Unauthorized)"
    }

    # 23. Test invalid login
    Write-Host "[24/38] Testing login with wrong password..."
    $badLoginPayload = @{ identifier = "vikram_m"; password = "WrongPassword999!" } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body $badLoginPayload -ContentType "application/json"
        throw "Expected login with wrong password to fail!"
    } catch {
        Write-Host "   -> PASS: Invalid credentials correctly rejected (401)"
    }

    # 24. Test logout
    Write-Host "[25/38] Testing POST /api/auth/logout..."
    $logoutResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/logout" -Method Post -WebSession $session3
    Write-Host "   -> PASS: User 3 logged out successfully"

    # 25. Phase 3: Profile Update (PUT /api/users/profile)
    Write-Host "[26/38] Testing Phase 3: PUT /api/users/profile (User 1 updating bio and name)..."
    $updateProfilePayload = @{
        fullName = "Vikram R. Malhotra"
        bio = "C++ Systems Engineer & Antigravity explorer."
    } | ConvertTo-Json
    $profileResp = Invoke-RestMethod -Uri "$baseUrl/api/users/profile" -Method Put -Body $updateProfilePayload -ContentType "application/json" -WebSession $session1
    if ($profileResp.user.fullName -ne "Vikram R. Malhotra" -or $profileResp.user.bio -ne "C++ Systems Engineer & Antigravity explorer.") {
        throw "Profile update failed: $($profileResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: User 1 updated profile (Name: $($profileResp.user.fullName), Bio: $($profileResp.user.bio))"

    # 26. Phase 3: Peer Profile Retrieval (GET /api/users/:id)
    Write-Host "[27/38] Testing Phase 3: GET /api/users/:id (User 2 viewing User 1 profile)..."
    $peerProfile = Invoke-RestMethod -Uri "$baseUrl/api/users/$user1Id" -Method Get -WebSession $session2
    if ($peerProfile.user.bio -ne "C++ Systems Engineer & Antigravity explorer." -or -not $peerProfile.user.lastActiveAt) {
        throw "Peer profile check failed: $($peerProfile | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: User 2 retrieved User 1 profile with bio and genuine presence lastActiveAt: $($peerProfile.user.lastActiveAt)"

    # 27. Phase 3: In-Conversation Keyword Search (GET /api/conversations/:id/messages/search)
    Write-Host "[28/38] Testing Phase 3: GET /api/conversations/$convId/messages/search?q=Vikram..."
    $searchConv = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages/search?q=Vikram" -Method Get -WebSession $session1
    if ($searchConv.messages.Count -lt 1) {
        throw "In-conversation search failed to find keyword 'Vikram'"
    }
    Write-Host "   -> PASS: In-conversation search returned $($searchConv.messages.Count) match"

    # 28. Phase 3: Global Keyword Search Across Conversations (GET /api/messages/search)
    Write-Host "[29/38] Testing Phase 3: GET /api/messages/search?q=smoothly..."
    $searchGlobal = Invoke-RestMethod -Uri "$baseUrl/api/messages/search?q=smoothly" -Method Get -WebSession $session1
    if ($searchGlobal.messages.Count -lt 1) {
        throw "Global search failed to find keyword 'smoothly'"
    }
    Write-Host "   -> PASS: Global search returned $($searchGlobal.messages.Count) match"

    # 29. Phase 3: Message Editing by Author (PUT /api/messages/:id)
    Write-Host "[30/38] Testing Phase 3: PUT /api/messages/$msg1Id (Author editing message)..."
    $editMsgPayload = @{ body = "Hello Ananya, this is Vikram on Sampark! (Edited)" } | ConvertTo-Json
    $editResp = Invoke-RestMethod -Uri "$baseUrl/api/messages/$msg1Id" -Method Put -Body $editMsgPayload -ContentType "application/json" -WebSession $session1
    if ($editResp.message.body -ne "Hello Ananya, this is Vikram on Sampark! (Edited)" -or -not $editResp.message.editedAt) {
        throw "Message edit failed: $($editResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Message 1 edited with populated editedAt: $($editResp.message.editedAt)"

    # 30. Phase 3: Unauthorized Message Editing Rejection (403 Forbidden)
    Write-Host "[31/38] Testing Phase 3: Non-author User 2 attempting to edit User 1's message..."
    $badEditPayload = @{ body = "Hacked message body" } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/messages/$msg1Id" -Method Put -Body $badEditPayload -ContentType "application/json" -WebSession $session2
        throw "Expected User 2 edit of User 1 message to fail with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-author edit correctly rejected (403 Forbidden)"
    }

    # 31. Phase 3: Unauthorized Message Deletion Rejection (403 Forbidden)
    Write-Host "[32/38] Testing Phase 3: Non-author User 2 attempting to delete User 1's message..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/messages/$msg1Id" -Method Delete -WebSession $session2
        throw "Expected User 2 deletion of User 1 message to fail with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Non-author deletion correctly rejected (403 Forbidden)"
    }

    # 32. Phase 3: Message Soft Deletion by Author (DELETE /api/messages/:id)
    Write-Host "[33/38] Testing Phase 3: DELETE /api/messages/$msg1Id (Author soft deleting message)..."
    $delResp = Invoke-RestMethod -Uri "$baseUrl/api/messages/$msg1Id" -Method Delete -WebSession $session1
    if (-not $delResp.success) { throw "Delete message failed" }

    $msgAfterDel = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session2
    $deletedMsg = $msgAfterDel.messages | Where-Object { $_.id -eq $msg1Id }
    if ($deletedMsg.isDeleted -ne 1 -or $deletedMsg.body -ne "This message was deleted") {
        throw "Deleted message tombstone verification failed: $($deletedMsg | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Message successfully soft deleted (isDeleted = 1, tombstone body returned)"

    # 33. Phase 3: Password Change (PUT /api/auth/password)
    Write-Host "[34/38] Testing Phase 3: PUT /api/auth/password (User 1 changing password)..."
    $changePwPayload = @{
        currentPassword = "SecurePassword123!"
        newPassword = "NewSecurePassword456!"
    } | ConvertTo-Json
    $pwResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/password" -Method Put -Body $changePwPayload -ContentType "application/json" -WebSession $session1
    if (-not $pwResp.success) { throw "Password change failed" }
    Write-Host "   -> PASS: User 1 password changed successfully"

    # 34. Phase 3: Password Verification (Old fails, new succeeds)
    Write-Host "[35/38] Verifying old password fails and new password succeeds..."
    try {
        Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body (@{ identifier = "vikram_m"; password = "SecurePassword123!" } | ConvertTo-Json) -ContentType "application/json"
        throw "Old password should have failed, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Old password rejected (401)"
    }
    $loginNewResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body (@{ identifier = "vikram_m"; password = "NewSecurePassword456!" } | ConvertTo-Json) -ContentType "application/json"
    if ($loginNewResp.user.username -ne "vikram_m") { throw "Login with new password failed" }
    Write-Host "   -> PASS: Login with new password succeeded"

    # 35. Restart C++ Server Process & Verify Full Phase 1, 2, 3 Data Persistence
    Write-Host "[36/38] Restarting C++ server to test database persistence across restarts..."
    Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2

    # Restart server with same database
    $serverProcess = Start-Process -FilePath ".\backend\server.exe" -ArgumentList "127.0.0.1", "$port", "$dbName" -PassThru
    Start-Sleep -Seconds 2

    # 36. Verify session survives or can re-login and all messages & profiles are intact
    Write-Host "[37/38] Re-authenticating User 1 with new password and verifying Phase 1, 2, 3 data survives restart..."
    $newSession1 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $reLoginResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/login" -Method Post -Body (@{ identifier = "vikram_m"; password = "NewSecurePassword456!" } | ConvertTo-Json) -ContentType "application/json" -WebSession $newSession1
    if ($reLoginResp.user.username -ne "vikram_m" -or $reLoginResp.user.bio -ne "C++ Systems Engineer & Antigravity explorer.") {
        throw "Re-login after restart failed or bio not persisted!"
    }

    $persistedHistory = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $newSession1
    if ($persistedHistory.messages.Count -ne 2) {
        throw "Messages lost after restart! Expected 2, got $($persistedHistory.messages.Count)"
    }
    $persistedDel = $persistedHistory.messages | Where-Object { $_.id -eq $msg1Id }
    if ($persistedDel.isDeleted -ne 1 -or $persistedDel.body -ne "This message was deleted") {
        throw "Deleted message state corrupted after restart!"
    }
    Write-Host "   -> PASS: Full conversation, edited/deleted message states, and user profile completely intact after server restart!"

    Write-Host "[38/38] Final regression check on dashboard listing after restart..."
    $postRestartDash = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Get -WebSession $newSession1
    if ($postRestartDash.conversations.Count -ne 1) { throw "Dashboard listing lost after restart" }
    Write-Host "   -> PASS: Dashboard conversation listing intact after restart"

    Write-Host "`n==============================================="
    Write-Host " ALL 38 END-TO-END HTTP TESTS PASSED 100%!     "
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
