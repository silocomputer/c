# c

Hosted C module driver: **sync · test · build**.

Go-like workflow for C packages (`c.mod`, `c.lock`, `deps/`).  
**Architecture:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

---

## Install

```bash
curl -fsSL https://github.com/silocomputer/c/releases/latest/download/install.sh | sh -
```

Installs `~/.local/bin/c`. If needed:

```bash
export PATH="$HOME/.local/bin:$PATH"
c version
```

First release (from this repo):

```bash
git tag v0.0.6 && git push origin v0.0.6
```

GitHub Actions attaches `install.sh` plus `c-darwin-*` / `c-linux-*`.

## Bootstrap from source

```bash
git clone https://github.com/silocomputer/c.git && cd c
clang -std=c23 -O2 -Wall -Wextra -Iinclude -o c src/*.c
./c build && ./c test
cp c ~/.local/bin/c
```

## Commands

```bash
c version
c sync                          # materialize deps/ from c.lock
c test [pkg...]                 # build+run tests/test_*.c (hosted)
c build [-o name] [pkg...]      # src/ (or .) → ./<module>
```

`c.mod` fields used: `module`, `level`, `freestanding`, `require`.  
With `require`, run `c sync` first; build/test pull sources from `deps/<name>/` and enforce levels.

## Layout

```
c.mod
c.lock             # optional until sync needed
include/c/
src/
tests/test_*.c
deps/              # from c sync (gitignored)
install.sh         # curl-friendly installer
```
