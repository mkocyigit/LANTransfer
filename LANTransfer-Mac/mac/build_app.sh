#!/bin/bash
# LANTransfer.app paketini oluşturur.
set -e
cd "$(dirname "$0")"
swift build -c release
APP=LANTransfer.app
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp AppIcon.icns "$APP/Contents/Resources/"
cp .build/release/LANTransfer "$APP/Contents/MacOS/"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key><string>LAN Transfer</string>
  <key>CFBundleIdentifier</key><string>local.lantransfer</string>
  <key>CFBundleExecutable</key><string>LANTransfer</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSLocalNetworkUsageDescription</key><string>Aynı ağdaki bilgisayarları bulmak ve dosya aktarmak için.</string>
</dict></plist>
PLIST
codesign --force --sign - "$APP"
echo "Hazır: $(pwd)/$APP"
