# Kdenlive Build Environment
# Usage:
#   cd docker && docker compose up builder
#   OR: just build (from repo root)

FROM fedora:42 AS base

# Set locale to avoid warnings
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Layer 1: System build tools (rarely changes)
RUN dnf install -y \
    cmake \
    ninja-build \
    gcc \
    gcc-c++ \
    git \
    clang-tools-extra \
    glibc-langpack-en \
    gdb \
    && dnf clean all

# Layer 2: Kdenlive build dependencies (changes rarely)
RUN dnf builddep -y kdenlive && dnf clean all

# Layer 3: Qt6 WebSockets (RPC dependency)
RUN dnf install -y qt6-qtwebsockets-devel && dnf clean all

# Layer 4: Build KDDockWidgets 2.4.0 (Fedora has 1.7.0, we need 2.4.0)
FROM base AS kddockwidgets-builder

# Remove old version installed by builddep
RUN dnf remove -y kddockwidgets-qt6 kddockwidgets-qt6-devel || true

# Build and install 2.4.0
WORKDIR /tmp
RUN git clone --depth 1 --branch v2.4.0 https://github.com/KDAB/KDDockWidgets.git && \
    cd KDDockWidgets && \
    cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DKDDockWidgets_QT6=ON \
        -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build build && \
    cmake --install build && \
    rm -rf /tmp/KDDockWidgets

# Layer 5: Final build environment
FROM kddockwidgets-builder AS builder

# Create build user (optional, for matching host UID)
ARG UID=1000
ARG GID=1000
RUN groupadd -g $GID builder 2>/dev/null || true && \
    useradd -u $UID -g $GID -m builder 2>/dev/null || true

WORKDIR /src

# Default build command
CMD ["sh", "-c", "\
    export LANG=C.UTF-8 LC_ALL=C.UTF-8 && \
    git config --global --add safe.directory /src && \
    cmake -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_INSTALL_PREFIX=/build/install && \
    cmake --build /build -j$(nproc) && \
    echo '=== Build complete! ===' \
"]
