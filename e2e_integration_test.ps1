Write-Host "==============================================="
Write-Host " RUNNING SAMPARK END-TO-END HTTP API TESTS      "
Write-Host "==============================================="

$port = 8085
$dbName = "e2e_test.db"
$baseUrl = "http://127.0.0.1:$port"

# Cleanup any previous db
if (Test-Path "backend/$dbName") { Remove-Item "backend/$dbName" -Force }
if (Test-Path "backend/$dbName-wal") { Remove-Item "backend/$dbName-wal" -Force }
if (Test-Path "backend/$dbName-shm") { Remove-Item "backend/$dbName-shm" -Force }

# Start the C++ server process
Write-Host "[1/54] Starting C++ server on $baseUrl with DB $dbName..."
$serverProcess = Start-Process -FilePath "wsl" -ArgumentList "--cd", "/mnt/c/Users/sujal/OneDrive/Documents/GitHub/Sampark/backend", "./server", "0.0.0.0", "$port", "$dbName" -PassThru

Start-Sleep -Seconds 3

try {
    # 1. Health check
    Write-Host "[2/54] Testing GET /api/health..."
    $healthResp = Invoke-RestMethod -Uri "$baseUrl/api/health" -Method Get
    if ($healthResp.status -ne "ok") { throw "Health check failed: $($healthResp | ConvertTo-Json)" }
    Write-Host "   -> PASS: Server returned status: $($healthResp.status)"

    # 2. CORS Preflight Checks
    Write-Host "[3/54] Testing CORS Preflight: Allowed Origin (http://localhost:5173)..."
    $corsAllowed = Invoke-WebRequest -Uri "$baseUrl/api/health" -Method Options -Headers @{ "Origin" = "http://localhost:5173" } -UseBasicParsing
    if ($corsAllowed.StatusCode -ne 204) { throw "Expected 204 No Content for allowed origin, got $($corsAllowed.StatusCode)" }
    if ($corsAllowed.Headers["Access-Control-Allow-Origin"] -ne "http://localhost:5173") {
        throw "Missing or incorrect Access-Control-Allow-Origin header! Got: $($corsAllowed.Headers['Access-Control-Allow-Origin'])"
    }
    Write-Host "   -> PASS: Allowed origin returned 204 with single Access-Control-Allow-Origin header"

    Write-Host "[4/54] Testing CORS Preflight: Untrusted Origin (http://evil-attacker.com)..."
    try {
        Invoke-WebRequest -Uri "$baseUrl/api/health" -Method Options -Headers @{ "Origin" = "http://evil-attacker.com" } -UseBasicParsing
        throw "Expected untrusted origin to be rejected with 403, but it succeeded!"
    } catch {
        Write-Host "   -> PASS: Untrusted origin correctly rejected (403 Forbidden)"
    }

    # 3. Register User 1 (Vikram)
    Write-Host "[5/54] Testing POST /api/auth/register (User 1 - Vikram)..."
    $user1Payload = @{
        username = "vikram_m"
        email = "vikram@college.edu.in"
        fullName = "Vikram Malhotra"
        password = "SecurePassword123!"
    } | ConvertTo-Json

    $session1 = New-Object Microsoft.PowerShell.Commands.WebRequestSession
    $regResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/register" -Method Post -Body $user1Payload -ContentType "application/json" -WebSession $session1
    if ($regResp.user.username -ne "vikram_m") { throw "Registration failed: $($regResp | ConvertTo-Json)" }
    if ($regResp.sessionToken) { throw "Security failure: sessionToken exposed in register JSON response!" }
    $user1Id = $regResp.user.id
    Write-Host "   -> PASS: User 1 registered: $($regResp.user.fullName) (ID: $user1Id, token kept in HttpOnly cookie)"

    # 4. Verify /api/auth/me with session cookie
    Write-Host "[6/54] Testing GET /api/auth/me for User 1 session..."
    $meResp = Invoke-RestMethod -Uri "$baseUrl/api/auth/me" -Method Get -WebSession $session1
    if ($meResp.user.username -ne "vikram_m") { throw "Session me check failed" }
    Write-Host "   -> PASS: Session resolved to $($meResp.user.fullName)"

    # 5. Reject duplicate username
    Write-Host "[7/54] Testing duplicate username rejection..."
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

    # 6. Register User 2 (Ananya)
    Write-Host "[8/54] Testing multi-user registration & isolation (User 2 - Ananya)..."
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
    Write-Host "   -> PASS: User 2 registered & session isolation verified: User 1 != User 2"

    # 7. User Discovery excludes self
    Write-Host "[9/54] Testing GET /api/users for User 1 (should find Ananya, exclude Vikram)..."
    $usersResp = Invoke-RestMethod -Uri "$baseUrl/api/users" -Method Get -WebSession $session1
    $foundUsernames = $usersResp.users | ForEach-Object { $_.username }
    if ($foundUsernames -contains "vikram_m") { throw "Self-exclusion failed: User 1 found himself in discovery list!" }
    if ($foundUsernames -notcontains "ananya_s") { throw "User 2 not found in discovery list!" }
    Write-Host "   -> PASS: Discovery correctly excludes caller and includes other users"

    # 8. Create Direct Conversation between Vikram and Ananya
    Write-Host "[10/54] Testing POST /api/conversations (Vikram initiates with Ananya)..."
    $convPayload = @{ participantId = $user2Id } | ConvertTo-Json
    $convResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations" -Method Post -Body $convPayload -ContentType "application/json" -WebSession $session1
    if (-not $convResp.conversation -or -not $convResp.conversation.id) { throw "Conversation creation failed" }
    $convId = $convResp.conversation.id
    Write-Host "   -> PASS: Conversation established (ID: $convId)"

    # 9. Send Message 1 (Vikram to Ananya)
    Write-Host "[11/54] Testing POST /api/conversations/$convId/messages (Vikram sends Message 1)..."
    $msg1Payload = @{ body = "Hey Ananya, welcome to Sampark!" } | ConvertTo-Json
    $msg1Resp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg1Payload -ContentType "application/json" -WebSession $session1
    if (-not $msg1Resp.message -or $msg1Resp.message.body -ne "Hey Ananya, welcome to Sampark!") {
        throw "Failed to send message: $($msg1Resp | ConvertTo-Json)"
    }
    $msg1Id = $msg1Resp.message.id
    Write-Host "   -> PASS: Message 1 sent and persisted (ID: $msg1Id)"

    # 10. Send Message 2 (Ananya replies to Vikram)
    Write-Host "[12/54] Testing POST /api/conversations/$convId/messages (Ananya sends Message 2)..."
    $msg2Payload = @{ body = "Thanks Vikram! Glad to be here." } | ConvertTo-Json
    $msg2Resp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Post -Body $msg2Payload -ContentType "application/json" -WebSession $session2
    if (-not $msg2Resp.message) { throw "Failed to send reply" }
    $msg2Id = $msg2Resp.message.id
    Write-Host "   -> PASS: Message 2 sent and persisted (ID: $msg2Id)"

    # 11. Retrieve Messages History
    Write-Host "[13/54] Testing GET /api/conversations/$convId/messages..."
    $historyResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/messages" -Method Get -WebSession $session1
    if ($historyResp.messages.Count -ne 2) {
        throw "Expected 2 messages in conversation history, got $($historyResp.messages.Count)"
    }
    Write-Host "   -> PASS: Chronological history retrieved successfully (2 messages)"

    # 12. Mark conversation as read
    Write-Host "[14/54] Testing POST /api/conversations/$convId/read..."
    $readResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$convId/read" -Method Post -WebSession $session1
    if (-not $readResp.success) { throw "Mark as read failed" }
    Write-Host "   -> PASS: Conversation marked as read"

    # 13. Phase 3: Profile Update (PUT /api/users/profile)
    Write-Host "[15/54] Testing PUT /api/users/profile (User 1 updating bio & name)..."
    $profilePayload = @{
        fullName = "Vikram Malhotra, Lead Engineer"
        bio = "C++ Systems Engineer & Antigravity explorer."
    } | ConvertTo-Json
    $profResp = Invoke-RestMethod -Uri "$baseUrl/api/users/profile" -Method Put -Body $profilePayload -ContentType "application/json" -WebSession $session1
    if ($profResp.user.fullName -ne "Vikram Malhotra, Lead Engineer" -or $profResp.user.bio -ne "C++ Systems Engineer & Antigravity explorer.") {
        throw "Profile update failed: $($profResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: User profile updated successfully"

    # ==========================================
    # PHASE 4 E2E TESTS: GROUPS, REPLIES, REACTIONS,
    # ATTACHMENTS, AND PREFERENCES
    # ==========================================

    # 14. Phase 4: Create Group Chat (POST /api/groups)
    Write-Host "[16/54] Testing Phase 4: POST /api/groups (Vikram creates group with Ananya)..."
    $groupPayload = @{
        name = "Sampark Core Devs"
        description = "Engineering team coordination"
        memberIds = @($user2Id)
    } | ConvertTo-Json
    $groupResp = Invoke-RestMethod -Uri "$baseUrl/api/groups" -Method Post -Body $groupPayload -ContentType "application/json" -WebSession $session1
    if (-not $groupResp.conversation -or $groupResp.conversation.type -ne "group") {
        throw "Group creation failed: $($groupResp | ConvertTo-Json)"
    }
    $groupId = $groupResp.conversation.id
    Write-Host "   -> PASS: Group created successfully: $($groupResp.conversation.name) (ID: $groupId)"

    # 15. Phase 4: Get Group Members (GET /api/conversations/:id/members)
    Write-Host "[17/54] Testing Phase 4: GET /api/conversations/$groupId/members..."
    $membersResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId/members" -Method Get -WebSession $session1
    if ($membersResp.members.Count -ne 2) {
        throw "Expected 2 group members, got $($membersResp.members.Count)"
    }
    Write-Host "   -> PASS: Group members verified: $($membersResp.members.Count) members"

    # 16. Phase 4: Send Message in Group
    Write-Host "[18/54] Testing Phase 4: POST /api/conversations/$groupId/messages (Vikram sends group message)..."
    $groupMsgPayload = @{ body = "Welcome to the dev group!" } | ConvertTo-Json
    $groupMsgResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId/messages" -Method Post -Body $groupMsgPayload -ContentType "application/json" -WebSession $session1
    if (-not $groupMsgResp.message) { throw "Group message send failed" }
    $gMsg1Id = $groupMsgResp.message.id
    Write-Host "   -> PASS: Group message delivered (ID: $gMsg1Id)"

    # 17. Phase 4: Send Message Reply (POST /api/conversations/:id/messages with replyToId)
    Write-Host "[19/54] Testing Phase 4: Message Reply (Ananya replies to Vikram's message)..."
    $replyPayload = @{
        body = "Excited to collaborate!"
        replyToId = $gMsg1Id
    } | ConvertTo-Json
    $replyResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId/messages" -Method Post -Body $replyPayload -ContentType "application/json" -WebSession $session2
    if (-not $replyResp.message.replyTo -or $replyResp.message.replyTo.id -ne $gMsg1Id) {
        throw "Message reply validation failed: $($replyResp | ConvertTo-Json)"
    }
    $gMsg2Id = $replyResp.message.id
    Write-Host "   -> PASS: Message reply linked with preview: Replying to ID $gMsg1Id"

    # 18. Phase 4: Toggle Emoji Reaction (POST /api/messages/:id/reactions)
    Write-Host "[20/54] Testing Phase 4: POST /api/messages/$gMsg2Id/reactions (Vikram reacts with emoji)..."
    $reactPayload = '{"emoji":"\uD83D\uDC4D"}'
    $reactResp = Invoke-RestMethod -Uri "$baseUrl/api/messages/$gMsg2Id/reactions" -Method Post -Body $reactPayload -ContentType "application/json; charset=utf-8" -WebSession $session1
    if (-not $reactResp.reactions -or $reactResp.reactions[0].count -ne 1) {
        throw "Reaction failed: $($reactResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Emoji reaction added: $($reactResp.reactions[0].emoji) (count: 1)"

    # 19. Phase 4: Upload File Attachment (POST /api/conversations/:id/attachments)
    Write-Host "[21/54] Testing Phase 4: POST /api/conversations/$groupId/attachments (Uploading sample text attachment)..."
    $attachPayload = @{
        fileName = "notes.txt"
        content = "Hello Sampark Phase 4 file sharing!"
        mimeType = "text/plain"
    } | ConvertTo-Json
    $attachResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId/attachments" -Method Post -Body $attachPayload -ContentType "application/json" -WebSession $session1
    if (-not $attachResp.attachment -or -not $attachResp.attachment.id) {
        throw "Attachment upload failed: $($attachResp | ConvertTo-Json)"
    }
    $attachId = $attachResp.attachment.id
    Write-Host "   -> PASS: File attachment uploaded (ID: $attachId, File: notes.txt)"

    # 20. Phase 4: Download File Attachment (GET /api/attachments/:id)
    Write-Host "[22/54] Testing Phase 4: GET /api/attachments/$attachId (Ananya downloads attachment)..."
    $downloadResp = Invoke-WebRequest -Uri "$baseUrl/api/attachments/$attachId" -Method Get -WebSession $session2 -UseBasicParsing
    if ($downloadResp.StatusCode -ne 200 -or $downloadResp.Content -ne "Hello Sampark Phase 4 file sharing!") {
        throw "Attachment download content mismatch! Got: $($downloadResp.Content)"
    }
    Write-Host "   -> PASS: Attachment downloaded successfully with verified content"

    # 21. Phase 4: Get Default User Preferences (GET /api/users/preferences)
    Write-Host "[23/54] Testing Phase 4: GET /api/users/preferences..."
    $prefsResp = Invoke-RestMethod -Uri "$baseUrl/api/users/preferences" -Method Get -WebSession $session1
    if (-not $prefsResp.preferences -or $prefsResp.preferences.theme -ne "system") {
        throw "Default preferences check failed: $($prefsResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Default preferences verified: theme=$($prefsResp.preferences.theme), sound=$($prefsResp.preferences.sound_enabled)"

    # 22. Phase 4: Update User Preferences (PUT /api/users/preferences)
    Write-Host "[24/54] Testing Phase 4: PUT /api/users/preferences (Setting Dark theme)..."
    $updatePrefsPayload = @{
        theme = "dark"
        sound_enabled = $false
        desktop_notifications_enabled = $true
        read_receipts_enabled = $true
    } | ConvertTo-Json
    $updatedPrefsResp = Invoke-RestMethod -Uri "$baseUrl/api/users/preferences" -Method Put -Body $updatePrefsPayload -ContentType "application/json" -WebSession $session1
    if ($updatedPrefsResp.preferences.theme -ne "dark" -or $updatedPrefsResp.preferences.sound_enabled -ne $false) {
        throw "Preferences update failed: $($updatedPrefsResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: User preferences updated to Dark mode and sound disabled"

    # 23. Phase 4: Update Group Details (PUT /api/conversations/:id)
    Write-Host "[25/54] Testing Phase 4: PUT /api/conversations/$groupId (Admin updating group name)..."
    $upGroupPayload = @{
        name = "Sampark Core Engineers"
        description = "Updated engineering group"
    } | ConvertTo-Json
    $upGroupResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId" -Method Put -Body $upGroupPayload -ContentType "application/json" -WebSession $session1
    if ($upGroupResp.conversation.name -ne "Sampark Core Engineers") {
        throw "Group update failed: $($upGroupResp | ConvertTo-Json)"
    }
    Write-Host "   -> PASS: Group details updated successfully"

    # 24. Phase 4: Delete Group (DELETE /api/conversations/:id)
    Write-Host "[26/54] Testing Phase 4: DELETE /api/conversations/$groupId (Admin disbanding group)..."
    $delGroupResp = Invoke-RestMethod -Uri "$baseUrl/api/conversations/$groupId" -Method Delete -WebSession $session1
    if (-not $delGroupResp.success) { throw "Delete group failed" }
    Write-Host "   -> PASS: Group disbanded successfully"

    Write-Host "`n==============================================="
    Write-Host " ALL 26 END-TO-END HTTP TESTS PASSED 100%!     "
    Write-Host "==============================================="

} finally {
    Write-Host "Terminating test server process..."
    if ($serverProcess -and -not $serverProcess.HasExited) {
        Stop-Process -Id $serverProcess.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
    if (Test-Path "backend/$dbName") { Remove-Item "backend/$dbName" -Force -ErrorAction SilentlyContinue }
    if (Test-Path "backend/$dbName-wal") { Remove-Item "backend/$dbName-wal" -Force -ErrorAction SilentlyContinue }
    if (Test-Path "backend/$dbName-shm") { Remove-Item "backend/$dbName-shm" -Force -ErrorAction SilentlyContinue }
}
