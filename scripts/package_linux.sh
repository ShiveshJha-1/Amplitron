#!/bin/bash
# Linux Release Packaging Script

VERSION=${1:-dev}

echo "Packaging Amplitron for Linux..."

# Create release directory
RELEASE_DIR="release/Amplitron-Linux"
mkdir -p "$RELEASE_DIR"

# Copy executable
cp build/guitar-amp "$RELEASE_DIR/amplitron"
chmod +x "$RELEASE_DIR/amplitron"

# Copy documentation
cp README.md "$RELEASE_DIR/"
cp LICENSE "$RELEASE_DIR/" 2>/dev/null || true

# Copy assets and presets
cp -r assets "$RELEASE_DIR/" 2>/dev/null || true
cp -r presets "$RELEASE_DIR/" 2>/dev/null || true

# Create launcher script
cat > "$RELEASE_DIR/amplitron.sh" << 'EOF'
#!/bin/bash
# Amplitron Launcher Script

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Check for dependencies
check_dependency() {
    if ! ldconfig -p | grep -q "$1"; then
        echo "Warning: $1 not found. Please install it using your package manager."
        return 1
    fi
    return 0
}

echo "Checking dependencies..."
MISSING=0

check_dependency "libportaudio.so" || MISSING=1
check_dependency "libSDL2" || MISSING=1

if [ $MISSING -eq 1 ]; then
    echo ""
    echo "Install missing dependencies:"
    echo "  Ubuntu/Debian: sudo apt install portaudio19-dev libsdl2-2.0-0"
    echo "  Fedora:        sudo dnf install portaudio SDL2"
    echo "  Arch:          sudo pacman -S portaudio sdl2"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Launch Amplitron
./amplitron
EOF

chmod +x "$RELEASE_DIR/amplitron.sh"

# Create README
cat > "$RELEASE_DIR/README.txt" << EOF
# Amplitron - Guitar Amp Simulator

Version: $VERSION

## Installation

1. Install dependencies:
   - Ubuntu/Debian: sudo apt install portaudio19-dev libsdl2-2.0-0
   - Fedora:        sudo dnf install portaudio SDL2
   - Arch:          sudo pacman -S portaudio sdl2

2. Run: ./amplitron.sh

## Features

- Real-time audio processing with ultra-low latency
- 9 studio-quality effects pedals
- Preset management system
- Multi-track recording

## Support

- GitHub: https://github.com/sudip-mondal-2002/Amplitron
- Email: sudmondal2002@gmail.com

EOF

# Create tarball
cd release
tar -czf "Amplitron-Linux-x64.tar.gz" "Amplitron-Linux"
cd ..

echo "Package created: release/Amplitron-Linux-x64.tar.gz"
