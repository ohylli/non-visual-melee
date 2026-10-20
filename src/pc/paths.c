/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paths.h"
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "pc.h"
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include <stdlib.h>
#include <string.h>

bool pc_is_portable(void) {
#if defined(__ANDROID__) || defined(__APPLE__) && TARGET_OS_IPHONE
    return false;
#else
    const char* base = SDL_GetBasePath();
    if (!base)
        return false;
    char* marker = NULL;
    SDL_asprintf(&marker, "%sportable.txt", base);
    SDL_PathInfo info;
    bool portable = marker && SDL_GetPathInfo(marker, &info) && info.type == SDL_PATHTYPE_FILE;
    SDL_free(marker);
    return portable;
#endif
}

/* MELEE_USER_DIR: a test run, or two instances on one machine, each with a
 * settings folder of its own, ahead of portable.txt. The shader caches keep
 * their own switch, MELEE_CACHE_DIR. */
static char* user_dir_from_env(const char* env) {
#if defined(_WIN32)
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    size_t n = strlen(env);
    char* dir = SDL_malloc(n + 2);
    if (dir == NULL) {
        return NULL;
    }
    memcpy(dir, env, n);
    if (env[n - 1] != '/' && env[n - 1] != sep) {
        dir[n++] = sep; /* callers append file names as SDL_GetPrefPath's */
    }
    dir[n] = '\0';
    if (!SDL_CreateDirectory(dir)) {
        /* Callers then fall back or go without; say so, since a test run
         * meant to leave the player's settings alone may not. */
        pc_log_line("MELEE_USER_DIR: cannot make %s: %s", dir, SDL_GetError());
        SDL_free(dir);
        return NULL;
    }
    return dir;
}

char* pc_get_pref_path(void) {
    const char* env = getenv("MELEE_USER_DIR");
    if (env != NULL && env[0] != '\0')
        return user_dir_from_env(env);
    if (!pc_is_portable())
        return SDL_GetPrefPath(NULL, "melee-pc");
    char* path = NULL;
    SDL_asprintf(&path, "%suser/", SDL_GetBasePath());
    if (path && !SDL_CreateDirectory(path)) {
        SDL_free(path);
        return NULL;
    }
    return path;
}
