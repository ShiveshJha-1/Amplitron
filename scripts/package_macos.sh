#!/bin/bash
# macOS Release Packaging Script

VERSION=${1:-dev}

echo "Packaging Amplitron for macOS..."

# Create app bundle structure
APP_DIR="release/Amplitron.app"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

# Copy executable
cp build/guitar-amp "$APP_DIR/Contents/MacOS/Amplitron"
chmod +x "$APP_DIR/Contents/MacOS/Amplitron"

# Copy resources
cp -r assets "$APP_DIR/Contents/Resources/" 2>/dev/null || true
cp -r presets "$APP_DIR/Contents/Resources/" 2>/dev/null || true

# Create Info.plist
cat > "$APP_DIR/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Amplitron</string>
    <key>CFBundleIdentifier</key>
    <string>com.amplitron.app</string>
    <key>CFBundleName</key>
    <string>Amplitron</string>
    <key>CFBundleDisplayName</key>
    <string>Amplitron</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <string>Amplitron needs microphone access for guitar input.</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
</dict>
</plist>
EOF

# Create tarball
cd release
tar -czf "Amplitron-macOS-Universal.tar.gz" "Amplitron.app"
cd ..

echo "Package created: release/Amplitron-macOS-Universal.tar.gz"
