# Multi-stage ultra-lightweight Dockerfile for NANOCTRL Relay Server (nanosrv)
# Stage 1: Build pure C standalone server
FROM alpine:3.19 AS builder

RUN apk add --no-cache build-base clang

WORKDIR /build
COPY src/ src/
COPY VERSION VERSION

RUN mkdir -p bin && \
    clang -Oz -flto -Wall -Wextra -static \
    -DNANO_VERSION_STR=\"$(cat VERSION | tr -d '\n\r')\" \
    -DNANO_GIT_COMMIT=\"docker\" \
    -Isrc/core -Isrc/net -Isrc/crypto \
    src/server/nanosrv_main.c \
    src/net/server.c \
    src/net/socket.c \
    src/net/transport.c \
    src/core/protocol.c \
    src/crypto/random.c \
    src/crypto/auth.c \
    -lpthread -o bin/nanosrv && \
    strip bin/nanosrv

# Stage 2: Minimal scratch / alpine runtime image (< 5 MB)
FROM alpine:3.19

RUN addgroup -S nanoctrl && adduser -S nanoctrl -G nanoctrl
USER nanoctrl

COPY --from=builder /build/bin/nanosrv /usr/local/bin/nanosrv

EXPOSE 7443

HEALTHCHECK --interval=30s --timeout=3s \
  CMD nc -z 127.0.0.1 7443 || exit 1

ENTRYPOINT ["/usr/local/bin/nanosrv"]
CMD ["7443"]
