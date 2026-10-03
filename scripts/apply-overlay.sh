#!/usr/bin/env bash
# Apply the Reinhard OS overlay to a clone of your repository.
#   usage: scripts/apply-overlay.sh /path/to/Reinhard_OS      (default: current dir)
#   set SKIP_BUILD=1 to copy files without building.
set -euo pipefail

overlay="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${1:-.}"
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || { echo "Not a git repository: $PWD"; exit 1; }

echo "== 1/3 rename references to Reinhard OS"
bash "$overlay/scripts/rename-to-reinhard.sh"

echo "== 2/3 install Makefile, docs and scripts"
cp "$overlay/Makefile" Makefile
mkdir -p docs scripts
cp "$overlay"/docs/*.md docs/
cp "$overlay/scripts/rename-to-reinhard.sh" "$overlay/scripts/apply-overlay.sh" scripts/
git add -A

if [ "${SKIP_BUILD:-0}" = "1" ]; then
  echo "== 3/3 build skipped"
else
  echo "== 3/3 build"
  make clean && make
  echo "Run it with: make run"
fi
echo "Done. Review with: git status && git diff --staged"
