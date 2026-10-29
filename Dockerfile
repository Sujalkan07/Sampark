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

WORKDIR /app

# Copy compiled server binary from builder
COPY --from=builder /build/backend/server ./server

# Ensure upload directory exists with full read/write permissions
RUN mkdir -p uploads && chmod 777 uploads

# Default runtime configuration
ENV HOST=0.0.0.0
ENV PORT=8080
ENV DATABASE_PATH=connectx.db

EXPOSE 8080

CMD ["./server"]
