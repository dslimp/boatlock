FROM ubuntu:24.04

ARG FLUTTER_VERSION=3.32.4
ARG ANDROID_CMDLINE_TOOLS_VERSION=13114758
ARG ANDROID_PLATFORM_CURRENT=android-35
ARG ANDROID_PLATFORM_LEGACY=android-33
ARG ANDROID_BUILD_TOOLS_VERSION=35.0.0
ARG ANDROID_NDK_VERSION=27.0.12077973
ARG ANDROID_CMAKE_VERSION=3.22.1

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
    && yes | sdkmanager --licenses >/dev/null \
    && sdkmanager \
        "platform-tools" \
        "platforms;${ANDROID_PLATFORM_CURRENT}" \
        "platforms;${ANDROID_PLATFORM_LEGACY}" \
        "build-tools;${ANDROID_BUILD_TOOLS_VERSION}" \
        "ndk;${ANDROID_NDK_VERSION}" \
        "cmake;${ANDROID_CMAKE_VERSION}"

RUN curl -fsSL "https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz" -o /tmp/flutter.tar.xz \
    && tar -xJf /tmp/flutter.tar.xz -C /opt \
    && rm /tmp/flutter.tar.xz \
    && git config --global --add safe.directory "${FLUTTER_HOME}" \
    && flutter config --no-analytics --android-sdk "${ANDROID_HOME}" \
    && flutter precache --android --web \
    && flutter doctor -v

WORKDIR /opt/boatlock_ui_warmup

COPY boatlock_ui/pubspec.yaml boatlock_ui/pubspec.lock ./
COPY boatlock_ui/android/gradle ./android/gradle
COPY boatlock_ui/android/gradlew boatlock_ui/android/settings.gradle.kts boatlock_ui/android/build.gradle.kts ./android/
COPY boatlock_ui/android/app/build.gradle.kts ./android/app/build.gradle.kts

RUN chmod +x android/gradlew \
    && flutter pub get \
    && printf 'sdk.dir=%s\nflutter.sdk=%s\n' "${ANDROID_HOME}" "${FLUTTER_HOME}" > android/local.properties \
    && (cd android && ./gradlew --no-daemon --build-cache help)

WORKDIR /workspace
