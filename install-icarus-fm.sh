#!/bin/bash
#
# Icarus-FM - Interactive Source Installer
# https://github.com/chesterbait88/icarus-fm
#
# This script builds and installs Icarus-FM file manager from source.
# Icarus-FM is completely independent and will NOT conflict with your system Nemo.
#

set -e

# Set to false on error to preserve build dir for debugging
CLEANUP_ON_EXIT=true

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
GITHUB_REPO="chesterbait88/icarus-fm"
ICARUS_VERSION="1.1.0"
INSTALL_DIR="/tmp/icarus-fm-build"

#=============================================================================
# Helper Functions
#=============================================================================

print_header() {
    clear
    echo -e "${CYAN}"
    echo "  ╔═══════════════════════════════════════════════════════════╗"
    echo "  ║                                                           ║"
    echo "  ║   ██╗ ██████╗ █████╗ ██████╗ ██╗   ██╗███████╗           ║"
    echo "  ║   ██║██╔════╝██╔══██╗██╔══██╗██║   ██║██╔════╝           ║"
    echo "  ║   ██║██║     ███████║██████╔╝██║   ██║███████╗           ║"
    echo "  ║   ██║██║     ██╔══██║██╔══██╗██║   ██║╚════██║           ║"
    echo "  ║   ██║╚██████╗██║  ██║██║  ██║╚██████╔╝███████║           ║"
    echo "  ║   ╚═╝ ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝           ║"
    echo "  ║                     FILE MANAGER                          ║"
    echo "  ║                                                           ║"
    echo "  ╚═══════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "${GREEN}  Icarus-FM is completely independent${NC}"
    echo -e "${GREEN}  Will NOT conflict with your system Nemo${NC}"
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

print_step() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
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
    if [[ "$OS_ID" != "ubuntu" && "$OS_ID" != "debian" && "$OS_ID" != "linuxmint" && "$ID_LIKE" != *"ubuntu"* && "$ID_LIKE" != *"debian"* ]]; then
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

    for cmd in git whiptail; do
        if ! command -v "$cmd" &> /dev/null; then
            missing_deps+=("$cmd")
        fi
    done

    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_warning "Installing required tools: ${missing_deps[*]}"
        sudo apt-get update -qq
        sudo apt-get install -y whiptail git 2>&1 | grep -v "^Reading" | grep -v "^Building" || true
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
        "VIDEO" "Video preview with playback controls" ON \
        "AUDIO" "Audio preview with playback controls" ON \
        "PDF" "PDF preview (first page)" ON \
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
# Build Dependencies
#=============================================================================

install_build_dependencies() {
    print_step "Installing Build Dependencies"

    print_info "This may take a few minutes..."

    local build_deps=(
        # Build tools
        "meson"
        "ninja-build"
        "gcc"
        "g++"
        "pkg-config"
        "gettext"
        "intltool"
        # GTK and GLib
        "libgtk-3-dev"
        "libgail-3-dev"
        "libglib2.0-dev"
        "libjson-glib-dev"
        # X11
        "libx11-dev"
        # Cinnamon/GNOME libraries
        "libcinnamon-desktop-dev"
        "libxapp-dev"
        "libnotify-dev"
        # Media metadata
        "libexempi-dev"
        "libexif-dev"
        # XML and document parsing
        "libxml2-dev"
        "libgsf-1-dev"
        # GObject Introspection
        "gobject-introspection"
        "libgirepository1.0-dev"
        # Runtime dependencies
        "cinnamon-desktop-data"
        "gsettings-desktop-schemas"
        "gvfs"
        "gvfs-backends"
        "shared-mime-info"
        "desktop-file-utils"
        "librsvg2-common"
    )

    sudo apt-get update -qq

    # Install with progress
    local total=${#build_deps[@]}
    local count=0

    for dep in "${build_deps[@]}"; do
        count=$((count + 1))
        echo -ne "\r${BLUE}[$count/$total]${NC} Installing $dep...                    "
        sudo apt-get install -y "$dep" >/dev/null 2>&1 || true
    done
    echo ""

    print_success "Build dependencies installed"
}

install_optional_dependencies() {
    print_step "Installing Optional Dependencies"

    local optional_deps=()

    if [ "$ENABLE_VIDEO" = true ] || [ "$ENABLE_AUDIO" = true ]; then
        print_info "Installing GStreamer for video/audio preview..."
        optional_deps+=(
            "libgstreamer1.0-dev"
            "libgstreamer-plugins-base1.0-dev"
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
            "libpoppler-glib-dev"
            "libpoppler-glib8"
            "poppler-utils"
        )
    fi

    if [ ${#optional_deps[@]} -gt 0 ]; then
        for dep in "${optional_deps[@]}"; do
            echo -ne "\r${BLUE}→${NC} Installing $dep...                    "
            sudo apt-get install -y "$dep" >/dev/null 2>&1 || true
        done
        echo ""
        print_success "Optional dependencies installed"
    else
        print_info "No optional dependencies selected"
    fi
}

#=============================================================================
# Download and Build
#=============================================================================

download_source() {
    print_step "Downloading Icarus-FM Source"

    # Clean up any previous build
    if [ -d "$INSTALL_DIR" ]; then
        rm -rf "$INSTALL_DIR"
    fi
    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"

    print_info "Cloning from GitHub..."
    print_info "URL: https://github.com/${GITHUB_REPO}.git"

    # Clone with visible output
    set +e  # Temporarily disable exit on error
    git clone --depth 1 "https://github.com/${GITHUB_REPO}.git" icarus-fm 2>&1
    clone_result=$?
    set -e  # Re-enable exit on error

    if [ $clone_result -eq 0 ] && [ -d "$INSTALL_DIR/icarus-fm" ]; then
        print_success "Source code downloaded"
        cd icarus-fm
    else
        print_error "Failed to clone repository (exit code: $clone_result)"
        print_info "Check your internet connection and try again"
        print_info "Or try manually: git clone https://github.com/${GITHUB_REPO}.git"
        CLEANUP_ON_EXIT=false
        exit 1
    fi
}

build_icarus_fm() {
    print_step "Building Icarus-FM"

    cd "$INSTALL_DIR/icarus-fm"

    print_info "Configuring build..."
    if meson setup builddir 2>&1 | tail -5; then
        print_success "Build configured"
    else
        print_error "Meson configuration failed"
        exit 1
    fi

    print_info "Compiling (this may take a few minutes)..."

    # Get total number of build targets for progress
    if ninja -C builddir 2>&1 | while read -r line; do
        if [[ "$line" =~ ^\[([0-9]+)/([0-9]+)\] ]]; then
            current="${BASH_REMATCH[1]}"
            total="${BASH_REMATCH[2]}"
            percent=$((current * 100 / total))
            printf "\r${BLUE}[%3d%%]${NC} Building... [%s/%s]" "$percent" "$current" "$total"
        fi
    done; then
        echo ""
        print_success "Build completed successfully"
    else
        echo ""
        print_error "Build failed"
        exit 1
    fi
}

install_icarus_fm() {
    print_step "Installing Icarus-FM"

    cd "$INSTALL_DIR/icarus-fm"

    print_info "Installing to system..."

    if sudo ninja -C builddir install 2>&1 | grep -v "^Installing" | tail -3; then
        print_success "Icarus-FM installed successfully"
    else
        print_error "Installation failed"
        exit 1
    fi

    # Update caches
    print_info "Updating system caches..."
    sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true
    sudo gtk-update-icon-cache /usr/local/share/icons/hicolor 2>/dev/null || true

    # Update shared library cache (critical for finding libicarus-fm-extension.so)
    print_info "Updating library cache..."
    echo "/usr/local/lib/x86_64-linux-gnu" | sudo tee /etc/ld.so.conf.d/icarus-fm.conf >/dev/null
    sudo ldconfig

    print_success "System caches updated"
}

#=============================================================================
# Post-Installation
#=============================================================================

create_uninstall_script() {
    print_info "Creating uninstall script..."

    sudo tee /usr/local/bin/icarus-fm-uninstall.sh > /dev/null << 'UNINSTALL_EOF'
#!/bin/bash
# Icarus-FM Uninstaller

echo "Uninstalling Icarus-FM..."

# Remove binaries
sudo rm -f /usr/local/bin/icarus-fm
sudo rm -f /usr/local/bin/icarus-fm-desktop
sudo rm -f /usr/local/bin/icarus-fm-autorun-software
sudo rm -f /usr/local/bin/icarus-fm-connect-server
sudo rm -f /usr/local/bin/icarus-fm-open-with
sudo rm -f /usr/local/bin/icarus-fm-extensions-list
sudo rm -f /usr/local/bin/icarus-fm-action-layout-editor
sudo rm -f /usr/local/bin/icarus-fm-uninstall.sh

# Remove data
sudo rm -rf /usr/local/share/icarus-fm
sudo rm -rf /usr/local/share/doc/icarus-fm
sudo rm -f /usr/local/share/applications/icarus-fm*.desktop
sudo rm -f /usr/local/share/man/man1/icarus-fm*.1
sudo rm -f /usr/local/share/dbus-1/services/org.IcarusFM.service
sudo rm -f /usr/local/share/dbus-1/services/icarus-fm.FileManager1.service
sudo rm -f /usr/local/share/polkit-1/actions/org.icarus-fm.root.policy

# Remove icons
for size in 16 22 24 32 48 96 scalable; do
    sudo rm -f /usr/local/share/icons/hicolor/${size}x${size}/apps/icarus-fm.png 2>/dev/null
    sudo rm -f /usr/local/share/icons/hicolor/${size}/apps/icarus-fm.svg 2>/dev/null
done

# Remove libraries
sudo rm -f /usr/local/lib/x86_64-linux-gnu/libicarus-fm-extension*
sudo rm -rf /usr/local/lib/x86_64-linux-gnu/girepository-1.0/IcarusFM*

# Remove schemas
sudo rm -f /usr/local/share/glib-2.0/schemas/org.icarus-fm*.xml
sudo glib-compile-schemas /usr/local/share/glib-2.0/schemas/ 2>/dev/null || true

# Remove search helpers
sudo rm -f /usr/local/libexec/icarus-fm-*

# Remove library config and update cache
sudo rm -f /etc/ld.so.conf.d/icarus-fm.conf
sudo ldconfig

# Update caches
sudo update-desktop-database /usr/local/share/applications 2>/dev/null || true
sudo gtk-update-icon-cache /usr/local/share/icons/hicolor 2>/dev/null || true

echo "Icarus-FM has been uninstalled."
UNINSTALL_EOF

    sudo chmod +x /usr/local/bin/icarus-fm-uninstall.sh
    print_success "Uninstall script created at /usr/local/bin/icarus-fm-uninstall.sh"
}

launch_icarus_fm() {
    print_info "Launching Icarus-FM..."

    # Launch icarus-fm in the background
    nohup icarus-fm >/dev/null 2>&1 &
    sleep 2

    if pgrep -x "icarus-fm" >/dev/null; then
        print_success "Icarus-FM launched successfully"
    else
        print_success "Icarus-FM installed (launch from applications menu or run 'icarus-fm')"
    fi
}

show_completion_message() {
    local enabled_features="• Images (JPEG, PNG, GIF, SVG)\n• Text files (code, logs, configs)"

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
• Launch from your applications menu\n\
• Or run 'icarus-fm' in terminal\n\
• Press F7 to toggle the preview pane\n\
• Drag the divider to resize preview\n\n\
To uninstall:\n\
  sudo icarus-fm-uninstall.sh\n\n\
Note: Icarus-FM is independent and\nwill NOT affect your system Nemo." \
        25 55

    echo ""
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "  ${CYAN}Launch:${NC}     icarus-fm"
    echo -e "  ${CYAN}Toggle:${NC}     F7 for preview pane"
    echo -e "  ${CYAN}Uninstall:${NC}  sudo icarus-fm-uninstall.sh"
    echo ""
}

cleanup() {
    # Only cleanup on successful exit or explicit cleanup call
    # Don't cleanup on error so user can debug
    if [ "${CLEANUP_ON_EXIT:-true}" = "true" ] && [ -d "$INSTALL_DIR" ]; then
        rm -rf "$INSTALL_DIR"
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
    install_build_dependencies
    install_optional_dependencies
    download_source
    build_icarus_fm
    install_icarus_fm

    # Post-installation
    create_uninstall_script
    cleanup
    launch_icarus_fm
    show_completion_message
}

# Trap cleanup on exit
trap cleanup EXIT

# Run main installation
main
