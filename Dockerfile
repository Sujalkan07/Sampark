# Multi-stage Dockerfile for Sampark C++ Backend Server
# Stage 1: Build stage
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    libsqlite3-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy backend files
COPY backend/ ./backend/

WORKDIR /build/backend

# Clean precompiled objects and build native Linux ELF binary
RUN rm -f src/*.o tests/*.o server server.exe test_suite test_suite.exe && make server

# Stage 2: Runtime stage
FROM debian:bookworm-slim AS runner

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create dedicated non-root application user and group
RUN groupadd -g 10001 sampark && \
    useradd -u 10001 -g sampark -m -s /bin/bash sampark

WORKDIR /app

# Copy compiled server binary from builder
COPY --from=builder /build/backend/server ./server

# Ensure upload directory and persistent mount points exist with secure permissions
RUN mkdir -p /var/data/uploads /app/uploads && \
    chown -R sampark:sampark /app /var/data && \
    chmod 755 /app && \
    chmod 750 ./server && \
    chmod 700 /var/data /var/data/uploads /app/uploads

# Default runtime configuration
ENV HOST=0.0.0.0
ENV PORT=8080
ENV DATABASE_PATH=/var/data/connectx.db
ENV UPLOAD_DIRECTORY=/var/data/uploads

# Run as dedicated unprivileged user
USER sampark

EXPOSE 8080

CMD ["./server"]
