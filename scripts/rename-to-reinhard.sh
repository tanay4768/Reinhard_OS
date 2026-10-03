#!/usr/bin/env bash
# Normalise every project reference to "Reinhard OS".
# Run from the repository root.  Review with `git diff` afterwards.
#
# The overlay itself (prompts/) is skipped: it documents the old names on
# purpose.
set -euo pipefail

# Text files tracked by git (skips binaries, build/, .git/, prompts/).
files=$(git ls-files -z -- . ':!prompts' | xargs -0 grep -Il . 2>/dev/null || true)

for f in $files; do
  sed -i \
    -e 's/reinhard  *_os/reinhard_os/g' \
    -e 's/Reinhard  *_OS/Reinhard_OS/g' \
    -e 's/Reinhard  *OS/Reinhard OS/g' \
    -e 's/Reinhard  *_Os/Reinhard_Os/g' \
    -e 's/Akira  *OS/Reinhard OS/g' \
    -e 's/Akira_OS/Reinhard_OS/g' \
    -e 's/AKIRA_OS/REINHARD_OS/g' \
    -e 's/AKIRA/REINHARD/g' \
    -e 's/Akira/Reinhard/g' \
    -e 's/akira_os/reinhard_os/g' \
    -e 's/akira/reinhard/g' \
    "$f"
done

# Stray artefacts created by the old "reinhard _os.bin" Makefile bug.
rm -rf build _os.bin _os.iso akira_os.iso reinhard_os.iso

echo "Remaining mentions of the old names (should be empty outside prompts/):"
remaining=$( { git grep -n -i -E "akira|reinhard +_os" -- . ':!prompts' || true
               git grep -n -F "Reinhard  " -- . ':!prompts' || true
               git grep -n -F "reinhard  " -- . ':!prompts' || true; } )
if [ -z "$remaining" ]; then
  echo "  none"
else
  printf '%s\n' "$remaining"
fi
echo
echo "Not handled automatically: the ASCII-art boot banner (regenerate it, e.g. 'figlet Reinhard')."
