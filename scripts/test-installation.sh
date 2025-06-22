#!/bin/bash
# Test script for Git service installation and configuration

set -e

echo "=== DDE File Manager Git Service Installation Test ==="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test functions
test_pass() {
    echo -e "${GREEN}✓ $1${NC}"
}

test_fail() {
    echo -e "${RED}✗ $1${NC}"
    exit 1
}

test_warn() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

echo "1. Testing binary installation..."
if [ -x "/usr/bin/dde-file-manager-git-daemon" ]; then
    test_pass "Daemon binary installed"
else
    test_fail "Daemon binary not found"
fi

echo "2. Testing DBus service configuration..."
if [ -f "/usr/share/dbus-1/services/org.deepin.FileManager.Git.service" ]; then
    test_pass "DBus service file installed"
else
    test_fail "DBus service file not found"
fi

echo "3. Testing Systemd user service..."
if [ -f "/usr/lib/systemd/user/dde-file-manager-git-daemon.service" ]; then
    test_pass "Systemd service file installed"
else
    test_fail "Systemd service file not found"
fi

echo "4. Testing service configuration syntax..."
if systemctl --user cat dde-file-manager-git-daemon.service >/dev/null 2>&1; then
    test_pass "Systemd service configuration valid"
else
    test_warn "Systemd service configuration may have issues"
fi

echo "5. Testing DBus service activation..."
# Try to activate the service
if dbus-send --session --dest=org.deepin.FileManager.Git \
    --type=method_call /org/deepin/FileManager/Git \
    org.freedesktop.DBus.Introspectable.Introspect >/dev/null 2>&1; then
    test_pass "DBus service can be activated"
else
    test_warn "DBus service activation failed (may need manual start)"
fi

echo "6. Testing service startup time..."
start_time=$(date +%s%N)
if systemctl --user start dde-file-manager-git-daemon.service 2>/dev/null; then
    end_time=$(date +%s%N)
    startup_time=$(( (end_time - start_time) / 1000000 )) # Convert to milliseconds
    
    if [ $startup_time -lt 3000 ]; then
        test_pass "Service startup time: ${startup_time}ms (< 3s)"
    else
        test_warn "Service startup time: ${startup_time}ms (> 3s)"
    fi
else
    test_warn "Could not start service for timing test"
fi

echo "7. Testing service status..."
if systemctl --user is-active dde-file-manager-git-daemon.service >/dev/null 2>&1; then
    test_pass "Service is running"
    
    # Test resource usage
    pid=$(systemctl --user show dde-file-manager-git-daemon.service -p MainPID --value)
    if [ -n "$pid" ] && [ "$pid" != "0" ]; then
        memory_kb=$(ps -p "$pid" -o rss= 2>/dev/null || echo "0")
        memory_mb=$((memory_kb / 1024))
        
        if [ $memory_mb -lt 50 ]; then
            test_pass "Memory usage: ${memory_mb}MB (< 50MB)"
        else
            test_warn "Memory usage: ${memory_mb}MB (> 50MB)"
        fi
    fi
else
    test_warn "Service is not running"
fi

echo "8. Testing service cleanup..."
if systemctl --user stop dde-file-manager-git-daemon.service 2>/dev/null; then
    test_pass "Service can be stopped"
else
    test_warn "Could not stop service"
fi

echo "9. Testing plugin installation..."
plugin_path="/usr/lib/*/dde-file-manager/plugins/extensions/libdfm-extension-git2.so"
if ls $plugin_path >/dev/null 2>&1; then
    test_pass "Plugin library installed"
else
    test_fail "Plugin library not found"
fi

echo ""
echo "=== Installation Test Complete ==="
echo "If all tests passed, the Git service is properly installed and configured."
echo ""
echo "Manual verification steps:"
echo "1. Open DDE File Manager in a Git repository"
echo "2. Check if Git status emblems appear on files"
echo "3. Verify right-click Git menu options work"
echo "4. Test real-time status updates when files change" 