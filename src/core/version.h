#ifndef NANO_VERSION_H
#define NANO_VERSION_H

#define NANO_VERSION_MAJOR 0
#define NANO_VERSION_MINOR 2
#define NANO_VERSION_PATCH 0

#ifndef NANO_VERSION_STR
  #define NANO_VERSION_STR "0.2.0"
#endif

#ifndef NANO_GIT_COMMIT
  #define NANO_GIT_COMMIT "unknown"
#endif

#if defined(__arm64__) || defined(__aarch64__)
  #define NANO_BUILD_ARCH "arm64"
#elif defined(__x86_64__) || defined(_M_X64)
  #define NANO_BUILD_ARCH "x86_64"
#else
  #define NANO_BUILD_ARCH "generic"
#endif

static inline const char *nano_version_string(void) {
    return NANO_VERSION_STR;
}

static inline const char *nano_git_commit(void) {
    return NANO_GIT_COMMIT;
}

static inline const char *nano_build_arch(void) {
    return NANO_BUILD_ARCH;
}

#endif /* NANO_VERSION_H */
