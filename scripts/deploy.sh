#!/bin/bash
# Deployment script for DDE File Manager Git Plugin

set -e

# Configuration
BUILD_DIR="build"
BUILD_TYPE="Release"
INSTALL_PREFIX="/usr"
MAKE_JOBS=$(nproc)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() {
    echo -e "${BLUE}[INFO] $1${NC}"
}

print_success() {
    echo -e "${GREEN}[SUCCESS] $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

# Help function
show_help() {
    echo "DDE File Manager Git Plugin Deployment Script"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -c, --clean         Clean build directory before building"
    echo "  -d, --debug         Build in debug mode"
    echo "  -t, --test          Run tests after building"
    echo "  -i, --install       Install after building"
    echo "  -p, --package       Create debian packages"
    echo "  --prefix=DIR        Set installation prefix (default: /usr)"
    echo "  --jobs=N            Number of parallel build jobs (default: $(nproc))"
    echo ""
    echo "Examples:"
    echo "  $0 -c -i            Clean build and install"
    echo "  $0 -d -t            Debug build with tests"
    echo "  $0 -p               Create packages only"
}

# Parse command line arguments
CLEAN_BUILD=false
DEBUG_BUILD=false
RUN_TESTS=false
DO_INSTALL=false
CREATE_PACKAGES=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -d|--debug)
            DEBUG_BUILD=true
            BUILD_TYPE="Debug"
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -i|--install)
            DO_INSTALL=true
            shift
            ;;
        -p|--package)
            CREATE_PACKAGES=true
            shift
            ;;
        --prefix=*)
            INSTALL_PREFIX="${1#*=}"
            shift
            ;;
        --jobs=*)
            MAKE_JOBS="${1#*=}"
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

print_info "Starting deployment process..."
print_info "Build type: $BUILD_TYPE"
print_info "Install prefix: $INSTALL_PREFIX"
print_info "Parallel jobs: $MAKE_JOBS"

# Check dependencies
print_info "Checking dependencies..."
if ! command -v cmake >/dev/null 2>&1; then
    print_error "cmake is required but not installed"
    exit 1
fi

if ! command -v make >/dev/null 2>&1; then
    print_error "make is required but not installed"
    exit 1
fi

if ! pkg-config --exists Qt6Core; then
    print_error "Qt6 development packages are required"
    exit 1
fi

if ! pkg-config --exists dfm-extension; then
    print_error "dfm-extension development package is required"
    exit 1
fi

print_success "All dependencies found"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_info "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
print_info "Building project..."
make -j"$MAKE_JOBS"
print_success "Build completed successfully"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    print_info "Running tests..."
    if [ -f "Makefile" ] && make help | grep -q test; then
        make test
        print_success "Tests completed"
    else
        print_warning "No tests available"
    fi
fi

# Install if requested
if [ "$DO_INSTALL" = true ]; then
    print_info "Installing..."
    if [ "$EUID" -eq 0 ]; then
        make install
    else
        sudo make install
    fi
    print_success "Installation completed"
    
    # Run post-installation setup
    print_info "Running post-installation setup..."
    
    # Reload systemd user daemon
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user daemon-reload 2>/dev/null || true
        print_success "Systemd user daemon reloaded"
    fi
    
    # Reload DBus configuration
    if command -v dbus-send >/dev/null 2>&1; then
        dbus-send --system --type=method_call \
            --dest=org.freedesktop.DBus \
            /org/freedesktop/DBus \
            org.freedesktop.DBus.ReloadConfig 2>/dev/null || true
        print_success "DBus configuration reloaded"
    fi
fi

# Create packages if requested
if [ "$CREATE_PACKAGES" = true ]; then
    print_info "Creating debian packages..."
    cd ..
    
    if command -v dpkg-buildpackage >/dev/null 2>&1; then
        dpkg-buildpackage -us -uc -b
        print_success "Debian packages created"
    else
        print_error "dpkg-buildpackage not found. Install devscripts package."
        exit 1
    fi
fi

cd ..

print_success "Deployment completed successfully!"

# Show next steps
echo ""
print_info "Next steps:"
if [ "$DO_INSTALL" = true ]; then
    echo "1. Restart DDE File Manager to load the new plugin"
    echo "2. Open a Git repository in File Manager to test"
    echo "3. Run 'scripts/test-installation.sh' to verify installation"
else
    echo "1. Run '$0 -i' to install the plugin"
    echo "2. Or install manually with 'sudo make install' from build directory"
fi

if [ "$CREATE_PACKAGES" = true ]; then
    echo ""
    print_info "Package files created:"
    ls -la ../*.deb 2>/dev/null || true
fi 