plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

fun boatlockSigningValue(name: String): String? =
    (System.getenv(name) ?: project.findProperty(name) as String?)
        ?.trim()
        ?.takeIf { it.isNotEmpty() }

val releaseKeystorePath = boatlockSigningValue("BOATLOCK_ANDROID_KEYSTORE_PATH")
val releaseKeystorePassword =
    boatlockSigningValue("BOATLOCK_ANDROID_KEYSTORE_PASSWORD")
val releaseKeyAlias = boatlockSigningValue("BOATLOCK_ANDROID_KEY_ALIAS")
val releaseKeyPassword = boatlockSigningValue("BOATLOCK_ANDROID_KEY_PASSWORD")
val requireReleaseSigning =
    boatlockSigningValue("BOATLOCK_ANDROID_REQUIRE_RELEASE_SIGNING") == "1"
val hasReleaseSigning =
    releaseKeystorePath != null &&
        releaseKeystorePassword != null &&
        releaseKeyAlias != null &&
        releaseKeyPassword != null

android {
    namespace = "com.example.boatlock_ui"
    compileSdk = flutter.compileSdkVersion
    //ndkVersion = flutter.ndkVersion
    ndkVersion = "28.2.13676358"

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "com.example.boatlock_ui"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("boatlockRelease") {
                storeFile = file(releaseKeystorePath!!)
                storePassword = releaseKeystorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }

    buildTypes {
        release {
            signingConfig =
                if (hasReleaseSigning) {
                    signingConfigs.getByName("boatlockRelease")
                } else {
                    if (requireReleaseSigning) {
                        throw GradleException(
                            "BoatLock Android release signing is required. " +
                                "Set BOATLOCK_ANDROID_KEYSTORE_PATH, " +
                                "BOATLOCK_ANDROID_KEYSTORE_PASSWORD, " +
                                "BOATLOCK_ANDROID_KEY_ALIAS, and " +
                                "BOATLOCK_ANDROID_KEY_PASSWORD."
                        )
                    }
                    signingConfigs.getByName("debug")
                }
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_11)
    }
}

flutter {
    source = "../.."
}
