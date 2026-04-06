#pragma once

#ifndef HOST_VERSION_BUILD_ID
#define HOST_VERSION_BUILD_ID 163
#endif

#ifndef CLIENT_VERSION_BUILD_ID
#define CLIENT_VERSION_BUILD_ID 163
#endif

#define VERSION_STRINGIFY_IMPL(x) #x
#define VERSION_STRINGIFY(x) VERSION_STRINGIFY_IMPL(x)

constexpr char HOST_VERSION_SEMVER[] = "0.7.0";
constexpr char HOST_VERSION_NAME[] = "HOST 0.7.0 - Parameters and Display Timeout";
constexpr char HOST_VERSION_DATE[] = "2026-04-06";
constexpr char HOST_VERSION_BUILD_ID_STR[] = VERSION_STRINGIFY(HOST_VERSION_BUILD_ID);

constexpr char CLIENT_VERSION_SEMVER[] = "0.7.0";
constexpr char CLIENT_VERSION_NAME[] = "CLIENT 0.7.0 - Parameters and Display Timeout";
constexpr char CLIENT_VERSION_DATE[] = "2026-04-06";
constexpr char CLIENT_VERSION_BUILD_ID_STR[] = VERSION_STRINGIFY(CLIENT_VERSION_BUILD_ID);
