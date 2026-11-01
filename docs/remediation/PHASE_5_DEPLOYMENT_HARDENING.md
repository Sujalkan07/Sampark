# Phase 5 Remediation Report: Deployment, CI, and Infrastructure Hardening

**Document Version:** 1.0.0  
**Project:** Sampark Messaging Application  
**Execution Date:** November 1, 2026  
**Status:** Complete & Verified  

---

## 1. Executive Summary

Phase 5 addresses container security, cloud persistent storage architecture, CI/CD pipeline dependencies, and infrastructure integrity for Sampark.

Prior to this phase, the production Docker container ran as root with wide-open permissions (`chmod 777 uploads`), Render deployment lacked persistent disk provisioning (risking database and attachment wipes upon every container restart or redeployment), and the GitHub Actions CI workflow omitted `libssl-dev`, risking build failures during native Linux compilation.

Through this phase, we established a non-root container user (`sampark:10001`), restricted permissions to `700` on data and upload directories, configured a persistent 1GB disk volume in `render.yaml` with explicit mount paths, added automatic parent directory provisioning in C++, and fixed the GitHub Actions CI workflow dependencies.

---

## 2. Identified Vulnerabilities & Implemented Solutions

### 2.1 Container Hardening & Rootless Execution (Phase 5.1)
- **Vulnerabilities Identified:**
  - The runtime container ran as `root`. If a vulnerability occurred in native C++ code or libraries, an attacker would have root-level control over the container namespace.
  - The upload directory was created with insecure `chmod 777 uploads` permissions.
  - Build artifacts, test databases, and local uploads could inadvertently be included in image contexts.
- **Remediation Implemented:**
  - In `Dockerfile`, created a dedicated, unprivileged system user and group: `sampark` with UID and GID `10001`.
  - Configured ownership of `/app` and `/var/data` to `sampark:sampark`.
  - Restricted directory and executable permissions:
    - Application directory `/app`: `755`
    - Server binary `./server`: `750`
    - Persistent mount `/var/data` and upload directories: `700`
  - Declared `USER sampark` prior to `CMD ["./server"]`.
  - Hardened `.dockerignore`: explicitly excluded `.git`, `.github`, `docs/`, `uploads/`, `backend/uploads/`, `*.log`, `*.ps1`, `backend/*.db*`, `backend/*.o`, `backend/server*`, and `backend/test_suite*`.

### 2.2 Render Deployment Persistence & Disk Mounts (Phase 5.2)
- **Vulnerabilities Identified:**
  - `render.yaml` was configured on the ephemeral `free` plan without persistent disk storage.
  - Container redeployments, restarts, or node migrations wiped out the SQLite database (`connectx.db`) and all uploaded user attachments.
- **Remediation Implemented:**
  - Updated `render.yaml` to specify the persistent `starter` tier with an attached volume:
    ```yaml
    disk:
      name: sampark-data
      mountPath: /var/data
      sizeGB: 1
    ```
  - Configured environment variables in `render.yaml` pointing to the persistent mount:
    - `DATABASE_PATH=/var/data/connectx.db`
    - `UPLOAD_DIRECTORY=/var/data/uploads`
  - In `backend/src/Database.cpp`, updated `Database::open` to inspect the path using `std::filesystem::path` and automatically create any missing parent directories (`std::filesystem::create_directories(p.parent_path())`) before calling `sqlite3_open`.
  - In `backend/src/MessagingService.cpp`, implemented `getUploadsDirectory()` honoring `UPLOAD_DIRECTORY` and ensuring directory creation on server initialization and attachment write.

### 2.3 GitHub Actions CI Pipeline Dependencies (Phase 5.3)
- **Vulnerabilities Identified:**
  - `.github/workflows/ci.yml` installed `build-essential libsqlite3-dev` but omitted `libssl-dev`.
  - Because `make server` and `make test` link `-lssl -lcrypto` for OpenSSL cryptography (PBKDF2-HMAC-SHA256 password hashing and CSPRNG byte generation), CI runners could fail with linker errors.
- **Remediation Implemented:**
  - Updated `.github/workflows/ci.yml` to install `libssl-dev` alongside `build-essential` and `libsqlite3-dev`:
    ```yaml
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential libsqlite3-dev libssl-dev
    ```
  - Verified local build pipeline matches the GitHub Actions Ubuntu environment.

---

## 3. Files Modified

| Component | Path | Changes |
|-----------|------|---------|
| Dockerfile | `Dockerfile` | Created dedicated user `sampark` (10001); restricted file permissions (700); added `/var/data` persistence defaults. |
| Docker Ignore | `.dockerignore` | Added upload paths, database artifacts, logs, and documentation to exclusion list. |
| Render Blueprint | `render.yaml` | Added persistent disk `sampark-data` (1GB) mounted at `/var/data`; defined `DATABASE_PATH` and `UPLOAD_DIRECTORY`. |
| CI Workflow | `.github/workflows/ci.yml` | Added `libssl-dev` to apt package installation step. |
| Database Source | `backend/src/Database.cpp` | Added automatic parent directory creation before `sqlite3_open`. |
| Messaging Service Source | `backend/src/MessagingService.cpp` | Integrated `UPLOAD_DIRECTORY` environment variable support and safe directory creation. |

---

## 4. Verification and Test Results

- **Backend Test Suite:** Executed `make test` under WSL Ubuntu 24.04 (`g++ 13.3.0`).
  - Total Tests: **88**
  - Passed: **88**
  - Failed: **0**
- **Non-Root & Permissions Check:** Verified permissions are properly scoped for user UID 10001.
- **Directory Auto-Creation:** Tested initialization with custom parent directory paths; confirmed zero filesystem exceptions.
