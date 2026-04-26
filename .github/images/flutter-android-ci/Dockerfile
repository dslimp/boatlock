FROM ubuntu:24.04

ARG FLUTTER_VERSION=3.41.7
ARG ANDROID_CMDLINE_TOOLS_VERSION=14742923
ARG ANDROID_PLATFORM_CURRENT=android-36
ARG ANDROID_BUILD_TOOLS_VERSION=36.0.0
ARG ANDROID_BUILD_TOOLS_FLUTTER_VERSION=35.0.0
ARG ANDROID_NDK_VERSION=28.2.13676358
ARG ANDROID_CMAKE_VERSION=4.1.2

ENV DEBIAN_FRONTEND=noninteractive
ENV ANDROID_HOME=/opt/android-sdk
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV FLUTTER_HOME=/opt/flutter
ENV PUB_CACHE=/opt/pub-cache
ENV GRADLE_USER_HOME=/opt/gradle-home
ENV PATH=/opt/flutter/bin:/opt/flutter/bin/cache/dart-sdk/bin:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:${PATH}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        curl \
        file \
        git \
        libglu1-mesa \
        openjdk-17-jdk \
        unzip \
        xz-utils \
        zip \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p "${ANDROID_HOME}/cmdline-tools" "${PUB_CACHE}" "${GRADLE_USER_HOME}" \
    && curl -fsSL "https://dl.google.com/android/repository/commandlinetools-linux-${ANDROID_CMDLINE_TOOLS_VERSION}_latest.zip" -o /tmp/android-commandline-tools.zip \
    && unzip -q /tmp/android-commandline-tools.zip -d /tmp/android-commandline-tools \
    && mv /tmp/android-commandline-tools/cmdline-tools "${ANDROID_HOME}/cmdline-tools/latest" \
    && rm -rf /tmp/android-commandline-tools /tmp/android-commandline-tools.zip \
    && set +o pipefail \
    && yes | sdkmanager --licenses >/dev/null \
    && set -o pipefail \
    && sdkmanager \
        "platform-tools" \
        "platforms;${ANDROID_PLATFORM_CURRENT}" \
        "build-tools;${ANDROID_BUILD_TOOLS_FLUTTER_VERSION}" \
        "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" \
        "ndk;${ANDROID_NDK_VERSION}" \
        "cmake;${ANDROID_CMAKE_VERSION}"

RUN curl -fsSL "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" -o /tmp/flutter.tar.xz \
    && tar -xJf /tmp/flutter.tar.xz -C /opt \
    && rm /tmp/flutter.tar.xz \
    && git config --global --add safe.directory "${FLUTTER_HOME}" \
    && git config --system --add safe.directory "${FLUTTER_HOME}" \
    && flutter config --no-analytics --android-sdk "${ANDROID_HOME}" \
    && flutter precache --android --web \
    && flutter --version \
    && rm -rf "${PUB_CACHE}" "${GRADLE_USER_HOME}" /root/.gradle /tmp/*

WORKDIR /workspace
