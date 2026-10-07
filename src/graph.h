#ifndef C_GRAPH_H
#define C_GRAPH_H

#include "mod.h"
#include "pkg.h"

enum { GRAPH_MAX_MODS = 64, GRAPH_MAX_INCLUDES = 64 };

typedef struct Graph {
    /* Closure of required modules (not including the root). */
    Mod mods[GRAPH_MAX_MODS];
    char roots[GRAPH_MAX_MODS][PKG_MAX_PATH]; /* deps/<name> */
    int nmods;

    char include[GRAPH_MAX_INCLUDES][PKG_MAX_PATH]; /* -I paths for deps */
    int ninclude;

    char sources[PKG_MAX_SOURCES][PKG_MAX_PATH]; /* dep .c files (no main.c) */
    int nsources;
} Graph;

/*
 * Resolve require closure under deps/<name>/.
 * Level check on each edge: required.level must not be > depender.level.
 * Returns 0 on success.
 */
int graph_resolve(Graph *g, const Mod *root);

#endif
