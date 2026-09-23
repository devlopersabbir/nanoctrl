#!/usr/bin/env bash
set -e

echo "=== NANOCTRL macOS Quick Setup ==="
if [ -d "/Applications/NANOCTRL.app" ]; then
    echo "[1/2] Removing macOS quarantine attribute from /Applications/NANOCTRL.app..."
    xattr -dr com.apple.quarantine /Applications/NANOCTRL.app 2>/dev/null || true
    echo "[2/2] Verifying application bundle..."
    codesign -v /Applications/NANOCTRL.app 2>/dev/null || true
    echo "SUCCESS: NANOCTRL.app is ready to open!"
else
    echo "NANOCTRL.app not found in /Applications. Drag NANOCTRL into Applications and run this script again."
fi
