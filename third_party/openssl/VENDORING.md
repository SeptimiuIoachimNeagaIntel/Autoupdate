# Vendored OpenSSL (pruned)

**Version:** 3.5.8
**Upstream:** https://www.openssl.org/source/openssl-3.5.8.tar.gz
**Upstream tarball sha256:** a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2

This is **not** a pristine upstream tree. Do not verify it against the upstream
tarball checksum directly, and do not hand-edit it. Regenerate it with:

```sh
scripts/vendor-openssl.sh 3.5.8
```

## What was removed

These directories are deleted to keep the repository small (the full upstream
tree is ~140 MB; this one is ~36 MB):

- `test/`
- `doc/`
- `demos/`
- `fuzz/`
- `apps/`

None of them are needed for a library-only build of libcrypto/libssl.

## Why the empty build.info stubs

The top-level `build.info` declares:

    SUBDIRS=crypto ssl apps util tools fuzz providers doc

`apps`, `doc` and `fuzz` appear there **unconditionally**, so `Configure`
reads each directory's `build.info` even when `no-apps`, `no-docs` and
`no-tests` are passed. Deleting those directories outright makes Configure
fail with "Something went wrong with .../build.info: No such file or directory".
Empty stub files satisfy the read and contribute no build rules:

- `doc/build.info` (empty)
- `fuzz/build.info` (empty)
- `apps/build.info` (empty)

`test` and `demos` need no stub: both are guarded by `IF[]` blocks in the
top-level `build.info` and drop out of SUBDIRS entirely under `no-tests` /
`no-demos`.

## Consequences

- The `openssl` command-line tool cannot be built from this tree (`no-apps`).
  If you need it for debugging, use a system `openssl` binary or an unpruned
  checkout; the library build is unaffected.
- `make test` is unavailable (`no-tests`). Upstream release testing is
  relied upon instead.
- Man pages and HTML docs are unavailable (`no-docs`).

## Build flags

`cmake/BuildBundledOpenSSL.cmake` configures this tree with:

    no-shared no-tests no-docs no-apps no-demos

The `no-*` flags must stay in sync with the pruned directories above.
