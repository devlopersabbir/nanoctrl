#!/usr/bin/env bash
set -euo pipefail

# Usage: ./tools/bump_version.sh [patch|minor|major|<specific_version>] [--tag]

if [ $# -lt 1 ]; then
    echo "Usage: $0 [patch|minor|major|<X.Y.Z>] [--tag]"
    exit 1
fi

ACTION="$1"
CREATE_TAG=false
if [ "${2:-}" = "--tag" ]; then
    CREATE_TAG=true
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

CURRENT_VERSION="$(cat "${ROOT_DIR}/VERSION" | tr -d '\n\r')"

IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT_VERSION"

case "$ACTION" in
    patch)
        PATCH=$((PATCH + 1))
        NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
        ;;
    minor)
        MINOR=$((MINOR + 1))
        PATCH=0
        NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
        ;;
    major)
        MAJOR=$((MAJOR + 1))
        MINOR=0
        PATCH=0
        NEW_VERSION="${MAJOR}.${MINOR}.${PATCH}"
        ;;
    *)
        if [[ "$ACTION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?$ ]]; then
            NEW_VERSION="$ACTION"
            IFS='.' read -r MAJOR MINOR PATCH <<< "$NEW_VERSION"
        else
            echo "Error: Invalid version format '$ACTION'. Must be patch, minor, major, or X.Y.Z"
            exit 1
        fi
        ;;
esac

echo "Bumping version: $CURRENT_VERSION -> $NEW_VERSION"

# Update VERSION file
echo "$NEW_VERSION" > "${ROOT_DIR}/VERSION"

# Update src/core/version.h
VERSION_H="${ROOT_DIR}/src/core/version.h"
sed -i '' "s/#define NANO_VERSION_MAJOR [0-9]*/#define NANO_VERSION_MAJOR ${MAJOR}/" "$VERSION_H"
sed -i '' "s/#define NANO_VERSION_MINOR [0-9]*/#define NANO_VERSION_MINOR ${MINOR}/" "$VERSION_H"
sed -i '' "s/#define NANO_VERSION_PATCH [0-9]*/#define NANO_VERSION_PATCH ${PATCH}/" "$VERSION_H"
sed -i '' "s/#define NANO_VERSION_STR \".*\"/#define NANO_VERSION_STR \"${NEW_VERSION}\"/" "$VERSION_H"

echo "[OK] Updated VERSION and src/core/version.h"

if [ "$CREATE_TAG" = true ]; then
    git -C "$ROOT_DIR" add VERSION src/core/version.h
    git -C "$ROOT_DIR" commit -m "chore: bump version to v${NEW_VERSION}"
    git -C "$ROOT_DIR" tag -a "v${NEW_VERSION}" -m "Release v${NEW_VERSION}"
    echo "[OK] Created commit and git tag v${NEW_VERSION}"
    echo "Run 'git push origin main --tags' to trigger automated release."
fi
