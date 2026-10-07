#include "graph.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int graph_seen(const Graph *g, const char *name) {
    for (int i = 0; i < g->nmods; i++) {
        if (strcmp(g->mods[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int graph_add_include(Graph *g, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return 0;
    }
    if (g->ninclude >= GRAPH_MAX_INCLUDES) {
        fprintf(stderr, "c: graph: too many include paths\n");
        return -1;
    }
    if (snprintf(g->include[g->ninclude], PKG_MAX_PATH, "%s", path) >= PKG_MAX_PATH) {
        return -1;
    }
    g->ninclude++;
    return 0;
}

static int graph_add_sources(Graph *g, const char *modroot) {
    char pkgdir[PKG_MAX_PATH];
    if (pkg_default_in(modroot, pkgdir, sizeof(pkgdir)) != 0) {
        return -1;
    }
    char chunk[PKG_MAX_SOURCES][PKG_MAX_PATH];
    int nchunk = 0;
    if (pkg_list_c(pkgdir, chunk, PKG_MAX_SOURCES, &nchunk) != 0) {
        return -1;
    }
    for (int i = 0; i < nchunk; i++) {
        const char *base = strrchr(chunk[i], '/');
        base = base ? base + 1 : chunk[i];
        if (strcmp(base, "main.c") == 0) {
            continue;
        }
        if (g->nsources >= PKG_MAX_SOURCES) {
            fprintf(stderr, "c: graph: too many sources\n");
            return -1;
        }
        memcpy(g->sources[g->nsources], chunk[i], PKG_MAX_PATH);
        g->nsources++;
    }
    return 0;
}

static int graph_visit(Graph *g, const char *name, const char *depender, int depender_level) {
    if (graph_seen(g, name)) {
        return 0;
    }
    if (g->nmods >= GRAPH_MAX_MODS) {
        fprintf(stderr, "c: graph: too many modules\n");
        return -1;
    }

    char modpath[PKG_MAX_PATH];
    char root[PKG_MAX_PATH];
    if (snprintf(root, sizeof(root), "deps/%s", name) >= (int)sizeof(root) ||
        snprintf(modpath, sizeof(modpath), "deps/%s/c.mod", name) >= (int)sizeof(modpath)) {
        return -1;
    }

    struct stat st;
    if (stat(modpath, &st) != 0) {
        fprintf(stderr, "c: graph: missing %s (run c sync)\n", modpath);
        return -1;
    }

    Mod dep;
    if (mod_load(&dep, modpath) != 0) {
        return -1;
    }
    if (strcmp(dep.name, name) != 0) {
        fprintf(stderr, "c: graph: %s: module name is %s, expected %s\n", modpath, dep.name, name);
        return -1;
    }
    if (dep.level > depender_level) {
        fprintf(stderr, "c: graph: upward require: %s (level %d) -> %s (level %d)\n", depender,
                depender_level, name, dep.level);
        return -1;
    }

    int idx = g->nmods++;
    g->mods[idx] = dep;
    snprintf(g->roots[idx], PKG_MAX_PATH, "%s", root);

    char inc[PKG_MAX_PATH];
    if (snprintf(inc, sizeof(inc), "%s/include", root) < (int)sizeof(inc)) {
        if (graph_add_include(g, inc) != 0) {
            return -1;
        }
    }
    if (graph_add_sources(g, root) != 0) {
        return -1;
    }

    for (int i = 0; i < dep.nrequire; i++) {
        if (graph_visit(g, dep.require[i], dep.name, dep.level) != 0) {
            return -1;
        }
    }
    return 0;
}

int graph_resolve(Graph *g, const Mod *root) {
    memset(g, 0, sizeof(*g));
    for (int i = 0; i < root->nrequire; i++) {
        if (graph_visit(g, root->require[i], root->name, root->level) != 0) {
            return -1;
        }
    }
    return 0;
}
