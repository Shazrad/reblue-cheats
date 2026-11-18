#!/usr/bin/env bash
# In: REPO, TAG, GH_TOKEN
set -euo pipefail

# The fixed header (what this is, credits) first, then GitHub's change list.
cat .github/release-header.md > notes.md
echo >> notes.md
gh api --method POST "repos/${REPO}/releases/generate-notes" \
  -f tag_name="${TAG}" \
  -q .body >> notes.md

echo "--- notes.md ---"
cat notes.md
