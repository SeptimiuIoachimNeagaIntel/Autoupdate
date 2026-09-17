#!/usr/bin/env bash
#
# Re-vendor the pruned OpenSSL source tree into third_party/openssl.
#
# This is the ONLY step in the project that needs network access, and it is a
# one-off: the resulting tree is committed, so builds stay fully offline.
#
# Usage:
#   scripts/vendor-openssl.sh              # re-vendor the pinned version
#   scripts/vendor-openssl.sh 3.5.9        # bump to a new version
#
# The tree is pruned to keep the repository small. See VENDORING.md in the
# vendored directory for what is removed and why.

set -euo pipefail

OPENSSL_VERSION="${1:-3.5.8}"

# sha256 of the upstream openssl-<version>.tar.gz. Only the pinned default
# version is verified against a known digest; when bumping, the script prints
# the digest it downloaded so you can check it against openssl.org and update
# this table.
declare -A KNOWN_SHA256=(
  ["3.5.8"]="a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2"
)

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${REPO_ROOT}/third_party/openssl"
TARBALL="openssl-${OPENSSL_VERSION}.tar.gz"
URL="https://www.openssl.org/source/${TARBALL}"

# Directories stripped from the upstream tree. The first four are large and
# unnecessary for a library-only build; 'fuzz' is small but is removed for the
# same reason (it only builds fuzzing harnesses).
#
# VMS is the build system for a platform this project does not target, and the
# seven trailing entries are empty submodule mount points for test-only
# external suites (git cannot track empty directories anyway).
#
# 'ms' is deliberately NOT pruned: it holds the applink/uplink shim that a
# Windows OpenSSL build needs. The Linux build never reads it, but keeping it
# leaves the tree usable if this project ever builds OpenSSL on Windows.
PRUNE_DIRS=(
  test doc demos fuzz apps
  VMS
  external/perl/Text-Template-1.56/t
  cloudflare-quiche oqs-provider pkcs11-provider python-ecdsa
  tlsfuzzer tlslite-ng wycheproof
)

# Individual files stripped from the upstream tree: release notes, porting
# notes, contributor docs and the VMS configure wrapper. Nothing in the build
# reads any of them -- Configure only mentions INSTALL.md inside error
# messages. LICENSE.txt and VERSION.dat are kept.
#
# The Text-Template files are that CPAN distribution's packaging metadata;
# Configure only ever adds its 'lib' directory to @INC.
PRUNE_FILES=(
  ACKNOWLEDGEMENTS.md AUTHORS.md CHANGES.md CODE-OF-CONDUCT.md
  CONTRIBUTING.md HACKING.md INSTALL.md NEWS.md
  NOTES-ANDROID.md NOTES-ANSI.md NOTES-DJGPP.md NOTES-NONSTOP.md
  NOTES-PERL.md NOTES-POSIX.md NOTES-UNIX.md NOTES-VALGRIND.md
  NOTES-VMS.md NOTES-WINDOWS.md
  README.md README-ENGINES.md README-FIPS.md README-PROVIDERS.md
  README-QUIC.md SUPPORT.md
  config.com funding.json
  external/perl/Text-Template-1.56/Changes
  external/perl/Text-Template-1.56/INSTALL
  external/perl/Text-Template-1.56/MANIFEST
  external/perl/Text-Template-1.56/META.json
  external/perl/Text-Template-1.56/META.yml
  external/perl/Text-Template-1.56/Makefile.PL
  external/perl/Text-Template-1.56/README
  external/perl/Text-Template-1.56/SIGNATURE
)

# Dot-directories stripped from the upstream tree (editor/ctags tooling).
PRUNE_DOTDIRS=(.ctags.d)

# The top-level build.info lists apps, doc and fuzz in SUBDIRS unconditionally,
# so Configure reads their build.info files even with the matching no-* flags.
# Empty stubs satisfy that read without contributing any build rules.
STUB_DIRS=(doc fuzz apps)

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

echo "==> Downloading ${URL}"
curl -fsSL -o "${WORK_DIR}/${TARBALL}" "${URL}"

ACTUAL_SHA256="$(sha256sum "${WORK_DIR}/${TARBALL}" | cut -d' ' -f1)"
EXPECTED_SHA256="${KNOWN_SHA256[${OPENSSL_VERSION}]:-}"

if [[ -n "${EXPECTED_SHA256}" ]]; then
  if [[ "${ACTUAL_SHA256}" != "${EXPECTED_SHA256}" ]]; then
    echo "ERROR: checksum mismatch for ${TARBALL}" >&2
    echo "  expected: ${EXPECTED_SHA256}" >&2
    echo "  actual:   ${ACTUAL_SHA256}" >&2
    exit 1
  fi
  echo "==> Checksum verified: ${ACTUAL_SHA256}"
else
  echo "==> WARNING: no known checksum for ${OPENSSL_VERSION}."
  echo "    Downloaded digest: ${ACTUAL_SHA256}"
  echo "    Verify it against ${URL}.sha256, then add it to KNOWN_SHA256 in this script."
fi

echo "==> Extracting"
tar xzf "${WORK_DIR}/${TARBALL}" -C "${WORK_DIR}"
SRC="${WORK_DIR}/openssl-${OPENSSL_VERSION}"

echo "==> Pruning directories: ${PRUNE_DIRS[*]}"
for dir in "${PRUNE_DIRS[@]}" "${PRUNE_DOTDIRS[@]}"; do
  rm -rf "${SRC}/${dir}"
done

echo "==> Pruning files: ${#PRUNE_FILES[@]} docs and porting notes"
for file in "${PRUNE_FILES[@]}"; do
  rm -f "${SRC}/${file}"
done

echo "==> Writing empty build.info stubs: ${STUB_DIRS[*]}"
for dir in "${STUB_DIRS[@]}"; do
  mkdir -p "${SRC}/${dir}"
  : > "${SRC}/${dir}/build.info"
done

echo "==> Replacing ${DEST}"
rm -rf "${DEST}"
mkdir -p "$(dirname "${DEST}")"
mv "${SRC}" "${DEST}"

# Build the markdown bullet lists before the heredoc. Doing it inline via
# $(printf ...) would require escaping the backticks, and those escapes end up
# in printf's format string rather than being consumed by the shell.
PRUNE_LIST="$(printf -- '- `%s/`\n' "${PRUNE_DIRS[@]}" "${PRUNE_DOTDIRS[@]}")"
PRUNE_FILE_LIST="$(printf -- '- `%s`\n' "${PRUNE_FILES[@]}")"
STUB_LIST="$(printf -- '- `%s/build.info` (empty)\n' "${STUB_DIRS[@]}")"
DEST_SIZE="~$(du -sm "${DEST}" | cut -f1) MB"

cat > "${DEST}/VENDORING.md" <<EOF
# Vendored OpenSSL (pruned)

**Version:** ${OPENSSL_VERSION}
**Upstream:** ${URL}
**Upstream tarball sha256:** ${ACTUAL_SHA256}

This is **not** a pristine upstream tree. Do not verify it against the upstream
tarball checksum directly, and do not hand-edit it. Regenerate it with:

\`\`\`sh
scripts/vendor-openssl.sh ${OPENSSL_VERSION}
\`\`\`

## What was removed

These directories are deleted to keep the repository small (the full upstream
tree is ~140 MB; this one is ${DEST_SIZE}):

${PRUNE_LIST}

These individual files are deleted as well -- release notes, porting notes,
contributor docs, the VMS configure wrapper, and the packaging metadata of the
bundled Text::Template CPAN distribution:

${PRUNE_FILE_LIST}

None of the above is needed for a library-only build of libcrypto/libssl. The
check that proves it: configure the pruned tree for \`linux-x86_64\` and diff
the generated \`Makefile\` against one from an unpruned tree. They are
byte-identical, so no build rule references anything that was removed.

\`ms/\` is deliberately **kept** even though the Linux build never reads it: it
holds the applink/uplink shim a Windows OpenSSL build needs, and keeping it
leaves the door open to building this tree on Windows later.

## Why the empty build.info stubs

The top-level \`build.info\` declares:

    SUBDIRS=crypto ssl apps util tools fuzz providers doc

\`apps\`, \`doc\` and \`fuzz\` appear there **unconditionally**, so \`Configure\`
reads each directory's \`build.info\` even when \`no-apps\`, \`no-docs\` and
\`no-tests\` are passed. Deleting those directories outright makes Configure
fail with "Something went wrong with .../build.info: No such file or directory".
Empty stub files satisfy the read and contribute no build rules:

${STUB_LIST}

\`test\` and \`demos\` need no stub: both are guarded by \`IF[]\` blocks in the
top-level \`build.info\` and drop out of SUBDIRS entirely under \`no-tests\` /
\`no-demos\`.

## Consequences

- The \`openssl\` command-line tool cannot be built from this tree (\`no-apps\`).
  If you need it for debugging, use a system \`openssl\` binary or an unpruned
  checkout; the library build is unaffected.
- \`make test\` is unavailable (\`no-tests\`). Upstream release testing is
  relied upon instead.
- Man pages and HTML docs are unavailable (\`no-docs\`).

## Build flags

\`cmake/BuildBundledOpenSSL.cmake\` configures this tree with:

    no-shared no-tests no-docs no-apps no-demos

The \`no-*\` flags must stay in sync with the pruned directories above.
EOF

echo
echo "==> Done. Vendored OpenSSL ${OPENSSL_VERSION} at third_party/openssl ($(du -sh "${DEST}" | cut -f1))"
echo "    Notes written to third_party/openssl/VENDORING.md"
echo "    Remember to 'git add third_party/openssl' and rebuild."
