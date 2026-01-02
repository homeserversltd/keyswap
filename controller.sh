#!/bin/bash

# keyswap Controller - Manage keyswap systemd services
# Handles loading, listing, and deleting keyswap configurations as systemd services

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KEYSWAP_BINARY="${KEYSWAP_BINARY:-$SCRIPT_DIR/keyswap}"
SYSTEMD_DIR="/etc/systemd/system"
SERVICE_PREFIX="keyswap"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

# Icons
ICON_SUCCESS="✓"
ICON_ERROR="✗"
ICON_INFO="ℹ"
ICON_WARNING="⚠"

# Function to print colored output
print_success() {
    echo -e "${GREEN}${ICON_SUCCESS}${NC} $1"
}

print_error() {
    echo -e "${RED}${ICON_ERROR}${NC} $1"
}

print_info() {
    echo -e "${BLUE}${ICON_INFO}${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}${ICON_WARNING}${NC} $1"
}

# Function to check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Function to validate keyswap config file
validate_config() {
    local config_file="$1"
    
    if [[ ! -f "$config_file" ]]; then
        print_error "Config file not found: $config_file"
        return 1
    fi
    
    # Check if file is valid JSON
    if ! python3 -m json.tool "$config_file" >/dev/null 2>&1; then
        print_error "Invalid JSON file: $config_file"
        return 1
    fi
    
    # Check for required keyswap schema elements
    local has_metadata=$(python3 -c "import json, sys; data = json.load(open('$config_file')); sys.exit(0 if 'metadata' in data else 1)" 2>/dev/null)
    local has_config=$(python3 -c "import json, sys; data = json.load(open('$config_file')); sys.exit(0 if 'config' in data else 1)" 2>/dev/null)
    
    if [[ $has_metadata -ne 0 ]] || [[ $has_config -ne 0 ]]; then
        print_error "Config file missing required keyswap schema elements (metadata, config)"
        return 1
    fi
    
    # Check if config has devices array
    local has_devices=$(python3 -c "import json, sys; data = json.load(open('$config_file')); sys.exit(0 if 'devices' in data.get('config', {}) else 1)" 2>/dev/null)
    
    if [[ $has_devices -ne 0 ]]; then
        print_error "Config file missing 'config.devices' array"
        return 1
    fi
    
    print_success "Config file validated: $config_file"
    return 0
}

# Function to sanitize service name (systemd service names must be valid)
sanitize_service_name() {
    local name="$1"
    # Replace invalid characters with hyphens, remove leading/trailing hyphens
    echo "$name" | sed 's/[^a-zA-Z0-9:_.-]/-/g' | sed 's/^-\+//' | sed 's/-\+$//' | tr '[:upper:]' '[:lower:]'
}

# Function to get service name from config file
get_service_name() {
    local config_file="$1"
    local basename=$(basename "$config_file" .json)
    local sanitized=$(sanitize_service_name "$basename")
    echo "${SERVICE_PREFIX}-${sanitized}"
}

# Function to find keyswap binary
find_keyswap_binary() {
    # Check if binary exists in script directory
    if [[ -f "$KEYSWAP_BINARY" ]] && [[ -x "$KEYSWAP_BINARY" ]]; then
        echo "$KEYSWAP_BINARY"
        return 0
    fi
    
    # Check if installed in system PATH
    if command -v keyswap >/dev/null 2>&1; then
        echo "keyswap"
        return 0
    fi
    
    print_error "keyswap binary not found. Please build keyswap first (make) or install it."
    return 1
}

# Function to create systemd service file
create_service_file() {
    local service_name="$1"
    local config_file="$2"
    local keyswap_bin="$3"
    local service_file="${SYSTEMD_DIR}/${service_name}.service"
    local config_path=$(realpath "$config_file")
    local config_dir=$(dirname "$config_path")
    local config_basename=$(basename "$config_file")
    
    # Create service file with proper quoting for paths with spaces
    cat > "$service_file" <<EOF
[Unit]
Description=keyswap Input Remapping Service - ${config_basename}
After=multi-user.target
Wants=multi-user.target

[Service]
Type=simple
User=root
WorkingDirectory=${config_dir}
ExecStart=${keyswap_bin} ${config_path}
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=/dev/input /dev/uinput

[Install]
WantedBy=multi-user.target
EOF
    
    chmod 644 "$service_file"
    print_success "Created systemd service file: $service_file"
}

# Function to load a keyswap config
load_config() {
    local config_file="$1"
    
    # Validate config
    if ! validate_config "$config_file"; then
        return 1
    fi
    
    # Get service name
    local service_name=$(get_service_name "$config_file")
    
    # Check if service already exists
    if systemctl list-unit-files | grep -q "^${service_name}.service"; then
        print_warning "Service ${service_name} already exists"
        read -p "Do you want to replace it? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Aborted"
            return 1
        fi
        
        # Stop and disable existing service
        systemctl stop "${service_name}.service" 2>/dev/null || true
        systemctl disable "${service_name}.service" 2>/dev/null || true
    fi
    
    # Find keyswap binary
    local keyswap_bin
    if ! keyswap_bin=$(find_keyswap_binary); then
        return 1
    fi
    
    # Create service file
    create_service_file "$service_name" "$config_file" "$keyswap_bin"
    
    # Reload systemd
    print_info "Reloading systemd daemon..."
    systemctl daemon-reload
    
    # Enable and start service
    print_info "Enabling and starting service: ${service_name}"
    systemctl enable "${service_name}.service"
    systemctl start "${service_name}.service"
    
    # Check status
    if systemctl is-active --quiet "${service_name}.service"; then
        print_success "Service ${service_name} is now running"
        print_info "Check status: systemctl status ${service_name}.service"
        print_info "View logs: journalctl -u ${service_name}.service -f"
    else
        print_error "Service ${service_name} failed to start"
        print_info "Check logs: journalctl -u ${service_name}.service"
        return 1
    fi
}

# Function to list all active keyswap services
list_services() {
    print_info "Active keyswap services:"
    echo ""
    
    # Get all keyswap services
    local services=$(systemctl list-unit-files --type=service | grep "^${SERVICE_PREFIX}-" | awk '{print $1}' | sed 's/\.service$//')
    
    if [[ -z "$services" ]]; then
        print_warning "No keyswap services found"
        return 0
    fi
    
    local count=1
    while IFS= read -r service_name; do
        if [[ -n "$service_name" ]]; then
            local status=$(systemctl is-active "${service_name}.service" 2>/dev/null || echo "inactive")
            local enabled=$(systemctl is-enabled "${service_name}.service" 2>/dev/null || echo "disabled")
            
            # Color code status
            if [[ "$status" == "active" ]]; then
                status_color="${GREEN}${status}${NC}"
            else
                status_color="${RED}${status}${NC}"
            fi
            
            if [[ "$enabled" == "enabled" ]]; then
                enabled_color="${GREEN}${enabled}${NC}"
            else
                enabled_color="${YELLOW}${enabled}${NC}"
            fi
            
            echo -e "${CYAN}$count${NC}) ${WHITE}${service_name}${NC}"
            echo -e "   Status: ${status_color} | Enabled: ${enabled_color}"
            
            # Get config file path from service file
            local service_file="${SYSTEMD_DIR}/${service_name}.service"
            if [[ -f "$service_file" ]]; then
                local config_path=$(grep "^ExecStart=" "$service_file" | sed 's/^ExecStart=.*keyswap[^ ]* //' | xargs)
                if [[ -n "$config_path" ]]; then
                    echo -e "   Config: ${BLUE}${config_path}${NC}"
                fi
            fi
            echo ""
            ((count++))
        fi
    done <<< "$services"
}

# Function to delete a keyswap service
delete_service() {
    local identifier="$1"
    
    if [[ -z "$identifier" ]]; then
        print_error "Please specify a service name or number"
        return 1
    fi
    
    local service_name=""
    
    # Check if identifier is a number
    if [[ "$identifier" =~ ^[0-9]+$ ]]; then
        # Get service by number
        local services=$(systemctl list-unit-files --type=service | grep "^${SERVICE_PREFIX}-" | awk '{print $1}' | sed 's/\.service$//')
        local count=1
        while IFS= read -r svc; do
            if [[ -n "$svc" ]]; then
                if [[ $count -eq $identifier ]]; then
                    service_name="$svc"
                    break
                fi
                ((count++))
            fi
        done <<< "$services"
        
        if [[ -z "$service_name" ]]; then
            print_error "No service found at number $identifier"
            return 1
        fi
    else
        # Assume it's a service name
        if [[ "$identifier" == "${SERVICE_PREFIX}-"* ]]; then
            service_name="$identifier"
        else
            service_name="${SERVICE_PREFIX}-${identifier}"
        fi
        
        # Verify service exists
        if ! systemctl list-unit-files | grep -q "^${service_name}.service"; then
            print_error "Service ${service_name} not found"
            return 1
        fi
    fi
    
    # Confirm deletion
    print_warning "This will stop, disable, and remove service: ${service_name}"
    read -p "Are you sure? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Aborted"
        return 0
    fi
    
    # Stop service
    print_info "Stopping service: ${service_name}"
    systemctl stop "${service_name}.service" 2>/dev/null || true
    
    # Disable service
    print_info "Disabling service: ${service_name}"
    systemctl disable "${service_name}.service" 2>/dev/null || true
    
    # Remove service file
    local service_file="${SYSTEMD_DIR}/${service_name}.service"
    if [[ -f "$service_file" ]]; then
        print_info "Removing service file: $service_file"
        rm -f "$service_file"
    fi
    
    # Reload systemd
    systemctl daemon-reload
    
    print_success "Service ${service_name} has been removed"
}

# Function to listen to device events
listen_device() {
    local identifier="$1"
    local config_file="$2"
    
    # Find keyswap binary
    local keyswap_bin
    if ! keyswap_bin=$(find_keyswap_binary); then
        return 1
    fi
    
    # Build listen command
    local listen_cmd=""
    
    if [[ -n "$identifier" ]]; then
        # Listen to specific device
        listen_cmd="$keyswap_bin --listen \"$identifier\""
    elif [[ -n "$config_file" ]]; then
        # Listen to devices from config file
        listen_cmd="$keyswap_bin --listen --run \"$config_file\""
    else
        # Listen to devices from default config
        listen_cmd="$keyswap_bin --listen"
    fi
    
    print_info "Starting listen mode (press Ctrl+C to stop)..."
    echo ""
    
    # Execute listen command (requires root for device access)
    eval "$listen_cmd"
}

# Function to show key reference mapping
show_key_reference() {
    local key_ref_file="${SCRIPT_DIR}/KEY-REFERENCE.md"
    
    if [[ ! -f "$key_ref_file" ]]; then
        print_error "Key reference file not found: $key_ref_file"
        return 1
    fi
    
    # Display the key reference file
    if command -v bat >/dev/null 2>&1; then
        bat "$key_ref_file"
    elif command -v less >/dev/null 2>&1; then
        less "$key_ref_file"
    else
        cat "$key_ref_file"
    fi
}

# Function to show usage
usage() {
    echo "keyswap Controller - Manage keyswap systemd services"
    echo ""
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  load <config_file>    Load a keyswap config file and start it as a systemd service"
    echo "  list                  List all active keyswap services"
    echo "  delete <name|number>   Delete a keyswap service by name or number"
    echo "  listen [ID|config]    Listen/monitor device events in real-time"
    echo "                        ID: vendor:product (e.g., 046d:c08b) or /dev/input/event8"
    echo "                        config: path to config file (monitors devices from config)"
    echo "                        If omitted, uses default config"
    echo "  keys                  Show key reference mapping (supported key names and aliases)"
    echo "  help                  Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 load /path/to/kensington.json"
    echo "  $0 list"
    echo "  $0 delete 1"
    echo "  $0 delete keyswap-kensington"
    echo "  $0 listen"
    echo "  $0 listen 046d:c08b"
    echo "  $0 listen /dev/input/event8"
    echo "  $0 listen /path/to/config.json"
    echo "  $0 keys"
    echo ""
}

# Main function
main() {
    case "${1:-}" in
        "load"|"l"|"-l")
            check_root
            if [[ -z "$2" ]]; then
                print_error "Please specify a config file"
                usage
                exit 1
            fi
            load_config "$2"
            ;;
        "list"|"ls"|"-ls")
            check_root
            list_services
            ;;
        "delete"|"del"|"rm"|"-d"|"-rm")
            check_root
            if [[ -z "$2" ]]; then
                print_error "Please specify a service name or number"
                usage
                exit 1
            fi
            delete_service "$2"
            ;;
        "listen"|"L"|"-L")
            # Listen mode - check if identifier is a config file or device identifier
            local identifier="$2"
            
            # Check if identifier looks like a config file (ends with .json or is a file path)
            if [[ -n "$identifier" ]] && [[ -f "$identifier" ]] && [[ "$identifier" == *.json ]]; then
                # It's a config file
                listen_device "" "$identifier"
            else
                # It's a device identifier (or empty)
                listen_device "$identifier" ""
            fi
            ;;
        "keys"|"key"|"reference"|"ref")
            show_key_reference
            ;;
        "help"|"-h"|"--help"|"")
            usage
            ;;
        *)
            print_error "Unknown command: $1"
            usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
