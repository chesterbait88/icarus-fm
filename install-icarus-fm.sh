#!/bin/bash
#
# Icarus-FM - Interactive Installer
# https://github.com/chesterbait88/icarus-fm
#
# This script installs Icarus-FM file manager with preview pane support.
# Icarus-FM is completely independent and will NOT conflict with your system Nemo.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
GITHUB_REPO="chesterbait88/icarus-fm"
PACKAGE_NAME="icarus-fm"
VERSION="1.1.0"
PACKAGE_FILE="${PACKAGE_NAME}_${VERSION}_amd64.deb"
DOWNLOAD_URL="https://github.com/${GITHUB_REPO}/releases/download/v${VERSION}/${PACKAGE_FILE}"

# Temporary download directory
TMP_DIR="/tmp/icarus-fm-install"

#=============================================================================
# Helper Functions
#=============================================================================

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}     Icarus-FM - Interactive Installer     ${NC}"
    echo -e "${BLUE}============================================${NC}"
    echo ""
    echo -e "${GREEN}Icarus-FM is completely independent${NC}"
    echo -e "${GREEN}Will NOT conflict with system Nemo${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

#=============================================================================
# System Detection
#=============================================================================

detect_system() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS_NAME="$NAME"
        OS_ID="$ID"
        OS_VERSION="$VERSION_ID"
    else
        print_error "Cannot detect operating system"
        exit 1
    fi

    # Check if Debian-based
    if [[ "$OS_ID" != "ubuntu" && "$OS_ID" != "debian" && "$OS_ID" != "linuxmint" ]]; then
        print_error "This installer only supports Debian, Ubuntu, and Linux Mint"
        print_info "Detected: $OS_NAME"
        exit 1
    fi

    print_success "Detected: $OS_NAME $OS_VERSION"
}

check_root() {
    if [ "$EUID" -eq 0 ]; then
        print_error "Please do not run this script as root"
        print_info "The script will ask for sudo password when needed"
        exit 1
    fi
}

check_dependencies() {
    local missing_deps=()

    for cmd in wget whiptail dpkg; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done

    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_warning "Installing required tools: ${missing_deps[*]}"
        sudo apt-get update -qq
        sudo apt-get install -y whiptail wget 2>&1 | grep -v "^Reading" | grep -v "^Building" || true
    fi
}

#=============================================================================
# Feature Selection
#=============================================================================

select_features() {
    # Use whiptail for interactive selection
    FEATURES=$(whiptail --title "Icarus-FM - Feature Selection" \
        --checklist "\nChoose which preview features to enable:\n(Use SPACE to select, ENTER to confirm)" 20 70 5 \
        "IMAGES" "Images (JPEG, PNG, GIF, SVG) - Always included" ON \
        "TEXT" "Text files (code, logs, JSON, etc.) - Always included" ON \
        "VIDEO" "Video preview with playback controls (requires GStreamer)" ON \
        "AUDIO" "Audio preview with playback controls (requires GStreamer)" ON \
        "PDF" "PDF preview (requires Poppler)" ON \
        3>&1 1>&2 2>&3)

    # Check if user cancelled
    if [ $? -ne 0 ]; then
        print_info "Installation cancelled by user"
        exit 0
    fi

    # Parse selections
    ENABLE_VIDEO=false
    ENABLE_AUDIO=false
    ENABLE_PDF=false

    if echo "$FEATURES" | grep -q "VIDEO"; then
        ENABLE_VIDEO=true
    fi
    if echo "$FEATURES" | grep -q "AUDIO"; then
        ENABLE_AUDIO=true
    fi
    if echo "$FEATURES" | grep -q "PDF"; then
        ENABLE_PDF=true
    fi
}

#=============================================================================
# Dependency Installation
#=============================================================================

install_base_dependencies() {
    print_info "Installing base dependencies..."

    local base_deps=(
        "cinnamon-desktop-data"
        "cinnamon-l10n"
        "desktop-file-utils"
        "gsettings-desktop-schemas"
        "gvfs"
        "libglib2.0-data"
        "shared-mime-info"
        "libgtk-3-0"
        "libglib2.0-0"
        "libcinnamon-desktop4"
        "libxapp1"
    )

    sudo apt-get update -qq
    sudo apt-get install -y "${base_deps[@]}" 2>&1 | grep -v "^Reading" | grep -v "^Building" || true
    print_success "Base dependencies installed"
}

install_optional_dependencies() {
    local optional_deps=()

    if [ "$ENABLE_VIDEO" = true ] || [ "$ENABLE_AUDIO" = true ]; then
        print_info "Installing GStreamer for video/audio preview..."
        optional_deps+=(
            "libgstreamer1.0-0"
            "gstreamer1.0-plugins-base"
            "gstreamer1.0-plugins-good"
            "gstreamer1.0-gtk3"
            "gstreamer1.0-libav"
        )
    fi

    if [ "$ENABLE_PDF" = true ]; then
        print_info "Installing Poppler for PDF preview..."
        optional_deps+=(
            "libpoppler-glib8"
        )
    fi

    if [ ${#optional_deps[@]} -gt 0 ]; then
        sudo apt-get install -y "${optional_deps[@]}" 2>&1 | grep -v "^Reading" | grep -v "^Building" || true
        print_success "Optional dependencies installed"
    fi
}

#=============================================================================
# Package Download and Installation
#=============================================================================

download_package() {
    print_info "Downloading Icarus-FM packages..."

    mkdir -p "$TMP_DIR"
    cd "$TMP_DIR"

    # Define all required packages
    local packages=(
        "icarus-fm_${VERSION}_amd64.deb"
        "icarus-fm-data_${VERSION}_all.deb"
        "libicarus-fm-extension1_${VERSION}_amd64.deb"
        "gir1.2-icarus-fm-3.0_${VERSION}_amd64.deb"
    )

    # Download each package
    for pkg in "${packages[@]}"; do
        local url="https://github.com/${GITHUB_REPO}/releases/download/v${VERSION}/${pkg}"
        if ! wget -q --show-progress "$url" -O "$pkg"; then
            print_error "Failed to download $pkg from GitHub"
            print_info "URL: $url"
            print_warning "Please check if the release exists on GitHub"
            exit 1
        fi
    done

    print_success "All packages downloaded successfully"
}

install_package() {
    print_info "Installing Icarus-FM..."

    cd "$TMP_DIR"

    # Install all packages using apt
    if sudo apt-get install -y \
        ./icarus-fm_${VERSION}_amd64.deb \
        ./icarus-fm-data_${VERSION}_all.deb \
        ./libicarus-fm-extension1_${VERSION}_amd64.deb \
        ./gir1.2-icarus-fm-3.0_${VERSION}_amd64.deb 2>&1 | tee /tmp/install.log | grep -qE "(Setting up|Unpacking)"; then
        print_success "Icarus-FM installed successfully"
    else
        print_error "Package installation failed"
        print_info "Check /tmp/install.log for details"
        sudo apt-get install -f -y >/dev/null 2>&1 || true
        exit 1
    fi
}

#=============================================================================
# Post-Installation
#=============================================================================

launch_icarus_fm() {
    print_info "Launching Icarus-FM..."

    # Launch icarus-fm in the background
    nohup icarus-fm >/dev/null 2>&1 &
    sleep 1

    if pgrep -x "icarus-fm" >/dev/null; then
        print_success "Icarus-FM launched successfully"
    else
        print_success "Icarus-FM installed (you can launch it from your applications menu)"
    fi
}

show_completion_message() {
    local enabled_features="• Images (JPEG, PNG, GIF, SVG)\n• Text files (code, logs, JSON, etc.)"

    if [ "$ENABLE_VIDEO" = true ]; then
        enabled_features="$enabled_features\n• Video preview with playback"
    fi
    if [ "$ENABLE_AUDIO" = true ]; then
        enabled_features="$enabled_features\n• Audio preview with playback"
    fi
    if [ "$ENABLE_PDF" = true ]; then
        enabled_features="$enabled_features\n• PDF preview (first page)"
    fi

    whiptail --title "Installation Complete!" --msgbox \
        "Icarus-FM has been installed successfully!\n\n\
Enabled features:\n\
$enabled_features\n\n\
Usage:\n\
• Launch Icarus-FM from your applications menu\n\
• Or run 'icarus-fm' from the terminal\n\
• Press F7 to toggle the preview pane\n\
• Drag the divider to resize the preview pane\n\n\
Note: Icarus-FM is independent and will NOT affect your system Nemo." \
        25 70

    echo ""
    print_success "Installation complete!"
    echo ""
    print_info "Launch Icarus-FM from your applications menu or run 'icarus-fm'"
    print_info "Press F7 in Icarus-FM to toggle the preview pane"
    echo ""
}

cleanup() {
    if [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}

#=============================================================================
# Main Installation Flow
#=============================================================================

main() {
    print_header

    # Preflight checks
    check_root
    detect_system
    check_dependencies

    # Feature selection
    select_features

    # Installation
    install_base_dependencies
    install_optional_dependencies
    download_package
    install_package

    # Post-installation
    launch_icarus_fm
    cleanup
    show_completion_message
}

# Trap cleanup on exit
trap cleanup EXIT

# Run main installation
main
