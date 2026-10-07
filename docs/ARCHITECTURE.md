# c — architecture and plan

Internal (for now) package/build driver for Silo’s C modules.  
**Binary name:** `c` · **Job:** make freestanding C feel like `go sync|test|build`.

Silo the product *composes* primitives. This tool *fetches, checks, and compiles* them. It is not the plant runtime.

**Design:** Go semantics (module, package, verbs, lock), C-native tree (`src/`, `include/<module>/`). Scales from tiny Silo libs to large C repos that adopt the conventions.

---

## 1. Goals

| Goal | Meaning |
|---|---|
| Fast | Tiny startup; exec clang/git; no libLLVM |
| Simple | Few verbs; convention over config; &lt; ~3k LOC for a long time |
| C-only workflow | Authors write C; tool is hosted C (libc OK) |
| Reproducible | `c.lock` pins `c`, clang, and every module commit |
| Layering | Honor module levels (ex-`prim`); refuse upward deps |
| Any repo size | Same UX for one-package libs and multi-package trees |

Non-goals (v0–v1): public registry, binary package CDN, CMake/Conan, freestanding build of `c` itself, incremental ninja DAG, plugins, parsing foreign build systems.

---

## 2. Relationship to Silo

```
[physics / bare port]          ← Silo product + ports
[freestanding modules]         ← plat, rec, log, ring, act, pipe, …
        ↑
   c sync / test / build       ← THIS TOOL (host)
        ↑
   pinned clang + git
```

- **Modules** (primitives): freestanding C libraries with `c.mod`
- **Product** (`silo`): composition + ports + `c.lock`
- **`c`**: host driver only — never required on the plant

Rename mapping from Silo monorepo:

| Old (silo repo) | New (c-native) |
|---|---|
| `prim` | `c.mod` |
| (none / Makefile) | `c.lock` + `c` verbs |
| `make test` | `c test` |

---

## 3. Go semantics → C

| Go | `c` |
|---|---|
| Module (`go.mod`) | Module = tree with `c.mod` |
| Package = directory | Package = directory of `.c` (build unit) |
| `require` / `go.sum` | `require` in `c.mod` / `c.lock` |
| `go build` / `go test` | `c build` / `c test` / `c sync` |
| Import path | `#include <module/foo.h>` + `include/<module>/` |
| Exported names | Public headers under `include/<module>/` |
| Unexported / `internal/` | Private headers beside `.c` in the package dir |

C has no language import graph — external link/include closure comes from module `require`, not from parsing `#include`.

---

## 4. Header convention

Not ISO-mandated; standard C library practice:

| Kind | Where | Use |
|---|---|---|
| **Public** | `include/<module>/*.h` | `#include <module/foo.h>` with `-Iinclude` |
| **Private** | next to `.c` (`src/usage.h`) | `#include "usage.h"` inside that package |

Every module (this tool and consumers) follows this. Compile always passes `-Iinclude` for the module and each required dep’s `include/`.

---

## 5. User-facing design

### Verbs

| Command | Behavior |
|---|---|
| `c sync` | Read `c.lock`; fetch/pin deps into `deps/`; verify hashes |
| `c test [path]` | Resolve graph; compile tests; run; exit status |
| `c build [path]` | Compile library or product binary |
| `c build bare` | Later: link `ports/bare` + linker script → image |
| `c version` | Print tool version |
| `c graph` | Optional: print deps / fail on level violations |

### Core nouns

| Noun | Meaning |
|---|---|
| **Module** | Tree rooted at `c.mod` |
| **Package** | One directory of `.c` files = one build unit |
| **Require** | External module dep in `c.mod` |
| **Lock** | Pins tool, clang, module versions (`c.lock`) |

### Package selection (no tree walk)

- **Default package:** `src/` if it has `.c`, else `.`  
- **`c build [pkg...]`:** only those directories (list `*.c` in each) — no recursive discovery  
- **`-o name`:** optional; default output is the module name from `c.mod`  
- **Skip / tests:** engineer does not pass `deps`, `include`, etc.; `tests/` is for `c test` later  

No file lists in `c.mod`.

### Layout patterns

**Small lib (Silo-style):**

```
log/
  c.mod
  include/log/*.h
  src/*.c
  tests/test_*.c
```

**Large repo:**

```
plant/
  c.mod
  c.lock
  include/plant/*.h
  runtime/*.c
  sched/*.c
  cmd/plant/main.c
  deps/
```

Dependents use `#include <log/log.h>` — never another module’s `src/`.

### `c.mod` (manifest — replaces `prim`)

Line-oriented, human-editable. Metadata only — not a file manifest:

```
module log
level  1
freestanding 1

require plat
require rec
require crc32c
```

Notes:

- `level` enforces DAG across modules — refuse upward deps
- `freestanding 1` → `-ffreestanding` / no host libc for that module
- This tool’s own `c.mod` is **hosted** (`freestanding 0`)
- `kind` / `thread` / `alloc` and other keys are **ignored** (ok as human notes)

### `c.lock` (pins — product / workspace)

```
c      0.1.0  h1:<hash>
clang  19.1.7 h1:<hash>

plat   v0.3.1 h1:<hash>  git+https://github.com/…/silo-plat@v0.3.1
rec    v0.2.0 h1:<hash>  git+https://github.com/…/silo-rec@v0.2.0
log    v0.4.0 h1:<hash>  git+https://github.com/…/silo-log@v0.4.0
```

`c sync` materializes `deps/<name>/` at those commits.  
Exact hash scheme TBD (git commit SHA is enough for v0).

### Build unit (large repos)

- Compile **one package at a time** in steady state.
- **v0 shortcut:** single-package modules may one-shot clang (Silo fast path).
- Cache later under `.c/cache/`.
- Link mains/tests against package + require closure.

---

## 6. Tool layout (this repo)

Flat `src/` + public `include/c/`. No Go `cmd/` / `internal/` directories. Add a `.c`/`.h` pair only when a phase needs it.

```
c/
  c.mod
  include/c/c.h          # public: version + exit codes
  src/
    main.c               # argv + dispatch only
    usage.c / usage.h
    mod.c / mod.h
    pkg.c / pkg.h        # package discovery
    lock.c / lock.h      # later
    …
  examples/
```

Bootstrap:

```bash
clang -std=c23 -O2 -Wall -Wextra -Iinclude -o c src/*.c
```

### Internal architecture

```
main
  ├─ cmd_version
  ├─ cmd_sync      → lock_parse → fetch_git → write deps/
  ├─ cmd_test      → mod_load → compile → run
  └─ cmd_build     → mod_load → pkg_list_c → clang -o
         │
         ├─ mod.c      c.mod (module/level/freestanding/require)
         ├─ pkg.c      default dir + list *.c
         ├─ build.c    c build
         ├─ run.c      fork/exec/wait
         └─ …
```

### Principles

1. **Exec, don’t link LLVM** — build a clang argv, spawn it  
2. **Exec git** for fetch — don’t embed libgit2 in v0  
3. **Package = build unit**; single-package one-shot allowed in v0  
4. **No plugin ABI**  
5. **Fail loud** on missing lock entry, level violation, clang missing  
6. **Prefix by package:** `mod_load`, `pkg_list_c`, `run_cmd`  
7. Exit codes: `0` ok, `1` failure, `2` usage  

### Freestanding vs hosted

| Target | Flags (sketch) |
|---|---|
| Freestanding module | `-std=c23 -ffreestanding -nostdlib` (+ allowed stubs) |
| Hosted tool / tests | `-std=c23` with libc |
| Tests | Hosted runner linking freestanding objs — detail at `c test` |

---

## 7. Distribution

Documented in [README](../README.md). Preferred path for users:

1. Push a version tag (`v*`) → GitHub Actions builds `c-<os>-<arch>` release assets  
2. Users run `install.sh` via `curl` (sets `C_GITHUB_REPO=owner/c`) → `~/.local/bin/c`  
3. Or `curl` the asset URL directly into `~/.local/bin/c`  
4. Fallback: clone and bootstrap with clang, then `cp c ~/.local/bin/c`  

Do not install over system tools. Default install prefix is `~/.local`, not a product-specific directory.

---

## 8. Implementation plan (line by line)

Do these in order. Each step should leave a working binary.

### Phase 0 — scaffold (done)

- [x] Repo layout, README, this doc  
- [x] Stub `main` + `c version`  
- [x] Example `c.mod` / `c.lock`  

### Phase 1 — CLI skeleton

- [x] Argv dispatch: `version`, unknown → usage  
- [x] Usage text listing future verbs  
- [x] Exit codes: 0 ok, 2 usage, 1 failure (`C_EXIT_*` in `include/c/c.h`)  
- [x] `usage.c` / `usage.h`; thin `main.c`  

### Phase 2 — `c.mod` parser

- [x] Load file from cwd  
- [x] Parse `module`, `level`, `require`, `freestanding` (ignore other keys)
- [x] No `c mod` verb  

### Phase 2b — package dirs (no walker)

- [x] Default `src` / `.`; `pkg_list_c` only  
- [x] `c build [-o name] [pkg...]`  

### Phase 3 — `c.lock` parser

- [x] Parse pin lines (`name`, version, `h1:…`, `git+url[@ref]`)  
- [ ] Resolve clang path / version check (string compare v0)  

### Phase 4 — `c sync`

- [x] Create `deps/`  
- [x] `git clone` / `git fetch` + checkout ref per lock entry  
- [x] Skip if already at pin  
- [x] No `c.lock` / no git pins → no-op ok  

### Phase 5 — `c build` (single module, no deps)

- [x] Discover/default package; collect `.c` sources  
- [x] Spawn clang `-o <name>` (default = module name)  
- [x] Honor `freestanding` from `c.mod`  
- [ ] path multi-pkg → `.a` then link (later)  

### Phase 6 — `c test`

- [x] Each `tests/test_*.c` + package sources (skip `main.c`)  
- [x] Link hosted test binary under `.c/test/`  
- [x] Run and forward exit code  
- [x] deps/`require` via graph in test  

### Phase 7 — deps in graph

- [x] Close `require` over `deps/*/c.mod` (transitive)  
- [x] Level check (refuse required.level > depender.level)  
- [x] Include dep sources + `-Ideps/<name>/include` in build/test  

### Phase 8 — product mode

- [ ] `c.lock` required  
- [ ] Build composition root (Silo later)  
- [ ] `c build bare` stub  

### Phase 9 — self-host

- [x] `c build` builds this repo’s `c`  
- [x] Document bootstrap one-liner remains for empty machines  

### Phase 10 — polish (only when needed)

- [ ] `.c/cache` content hash  
- [ ] `c graph`  
- [ ] `./...` patterns  
- [ ] Release scripts  

---

## 9. Source map

| File | Responsibility |
|---|---|
| `src/main.c` | argv, dispatch |
| `src/usage.c` | help text |
| `src/mod.c` | `c.mod` |
| `src/pkg.c` | default package dir + list `*.c` |
| `src/build.c` | `c build` |
| `src/sync.c` | `c sync` |
| `src/test.c` | `c test` |
| `src/lock.c` | `c.lock` |
| `src/run.c` | fork/exec/wait |
| `src/fs.c` | mkdir -p |
| `src/graph.c` | require closure + levels + dep sources |
| `include/c/c.h` | version + exit codes |

Add files when a phase needs them — don’t pre-create empty shells.

---

## 10. Invariants

1. `src/` = sources + private headers; `include/<module>/` = public headers  
2. Tool stays **hosted C**; modules may be freestanding  
3. Lockfile is law for CI  
4. No upward module level deps  
5. Clang is an external pinned binary  
6. Tool tree stays flat and small regardless of consumer size  
7. Simplicity &gt; generality — refuse features that don’t serve the model  

---

## 11. Decisions recorded

| Choice | Decision |
|---|---|
| Include path | `include/<module>/` (not `include/silo/` for all) |
| Tool headers | Public in `include/c/`; private beside `.c` in `src/` |
| Tool tree | Flat `src/` (C-native); Go semantics without `cmd/`/`internal/` |
| Dep link model | Per-package compile steady state; one-shot OK for single-package v0 |
| Lock hash | git SHA enough for v0 |
| Legacy CMake | Opt-in only via `c.mod` — no foreign build parsing |

---

## 12. Scalability (C)

Works for repos that adopt conventions. Not a drop-in for arbitrary legacy CMake/Autotools without layout cleanup.

Hard limit vs Go: no import graph in C — module `require` + layout discipline substitute. Extend later with cache, per-package flags, `./...`, without redesigning module/package/`include/`.
