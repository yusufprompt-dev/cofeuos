/*
 * scnw - Secure Network Wrapper (CofeuOS)
 *
 * Network scanning and configuration utility.
 *
 * Usage:
 *   scnw scan                 - Scan for available networks
 *   scnw connect <ssid> <pwd> - Connect to network
 *   scnw status               - Show current connection status
 *   scnw disconnect           - Disconnect from current network
 *   scnw list                 - List saved networks
 *   scnw forget <ssid>        - Forget saved network
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../include/string.h"
#include "../include/network.h"
#include "../include/sched.h"

#define SCNW_VERSION "0.1.0"
#define MAX_NETWORKS 32
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

typedef struct {
    char ssid[MAX_SSID_LEN + 1];
    char password[MAX_PASS_LEN + 1];
    int priority;
    int saved;
} network_config_t;

static network_config_t saved_networks[MAX_NETWORKS];
static int network_count = 0;

/* Mock network interface - in real implementation this would call hardware driver */
static int net_scan_results[] = {0}; /* placeholder */
static int mock_scan_count = 3;
static const char *mock_ssids[] = {"HomeWiFi", "OfficeNet", "GuestNetwork"};
static int mock_signals[] = {-45, -65, -78}; /* dBm */
static int mock_securities[] = {1, 1, 0}; /* 1=WPA2, 0=Open */

static void print_usage(const char *prog) {
    printf("scnw v%s - Secure Network Wrapper\n", SCNW_VERSION);
    printf("Usage: %s <command> [args]\n", prog);
    printf("\nCommands:\n");
    printf("  scan                    Scan for available networks\n");
    printf("  connect <ssid> <pwd>    Connect to network\n");
    printf("  status                  Show connection status\n");
    printf("  disconnect              Disconnect from network\n");
    printf("  list                    List saved networks\n");
    printf("  forget <ssid>           Forget saved network\n");
    printf("  version                 Show version\n");
}

static int cmd_scan(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Scanning for networks...\n");
    
    /* In real implementation, this would trigger hardware scan */
    printf("Found %d networks:\n", mock_scan_count);
    printf("%-4s %-32s %6s %s\n", "#", "SSID", "Signal", "Security");
    printf("%-4s %-32s %6s %s\n", "---", "--------------------------------", "------", "--------");
    
    for (int i = 0; i < mock_scan_count; i++) {
        const char *sec = mock_securities[i] ? "WPA2" : "Open";
        printf("%-4d %-32s %4d dBm %s\n", i + 1, mock_ssids[i], mock_signals[i], sec);
    }
    return 0;
}

static int cmd_connect(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: scnw connect <ssid> <password>\n");
        return 1;
    }
    
    const char *ssid = argv[1];
    const char *password = argv[2];
    
    if (strlen(ssid) > 32) {
        printf("Error: SSID too long (max 32 chars)\n");
        return 1;
    }
    if (strlen(password) > 63) {
        printf("Error: Password too long (max 63 chars)\n");
        return 1;
    }
    
    printf("Connecting to '%s'...\n", ssid);
    
    /* Check if network exists in scan results */
    int found = 0;
    for (int i = 0; i < mock_scan_count; i++) {
        if (strcmp(mock_ssids[i], ssid) == 0) {
            found = 1;
            break;
        }
    }
    
    if (!found) {
        printf("Warning: Network '%s' not found in recent scan\n", ssid);
    }
    
    /* Simulate connection */
    printf("Authenticating...\n");
    printf("Connected to '%s'\n", ssid);
    printf("IP assigned: 192.168.1.100\n");
    
    /* Save to config */
    if (network_count < 32) {
        strncpy(saved_networks[network_count].ssid, ssid, 32);
        strncpy(saved_networks[network_count].password, password, 63);
        saved_networks[network_count].priority = network_count;
        saved_networks[network_count].saved = 1;
        network_count++;
        printf("Network saved to configuration\n");
    }
    
    return 0;
}

static int cmd_status(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Network Status\n");
    printf("==============\n");
    printf("Interface: enp0s3\n");
    printf("Status: Connected\n");
    printf("SSID: HomeWiFi\n");
    printf("Signal: -45 dBm\n");
    printf("IP: 192.168.1.100/24\n");
    printf("Gateway: 192.168.1.1\n");
    printf("DNS: 192.168.1.1\n");
    return 0;
}

static int cmd_disconnect(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("Disconnecting...\n");
    printf("Disconnected\n");
    return 0;
}

static int cmd_list(int argc, char *argv[]) {
    (void)argc; (void)argv;
    if (network_count == 0) {
        printf("No saved networks\n");
        return 0;
    }
    printf("Saved Networks:\n");
    printf("%-4s %-32s %s\n", "#", "SSID", "Priority");
    printf("%-4s %-32s %s\n", "---", "--------------------------------", "--------");
    for (int i = 0; i < network_count; i++) {
        if (saved_networks[i].saved) {
            printf("%-4d %-32s %d\n", i + 1, saved_networks[i].ssid, saved_networks[i].priority);
        }
    }
    return 0;
}

static int cmd_forget(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: scnw forget <ssid>\n");
        return 1;
    }
    const char *ssid = argv[1];
    int found = 0;
    for (int i = 0; i < network_count; i++) {
        if (saved_networks[i].saved && strcmp(saved_networks[i].ssid, ssid) == 0) {
            saved_networks[i].saved = 0;
            found = 1;
            break;
        }
    }
    if (found) {
        printf("Forgot network '%s'\n", ssid);
    } else {
        printf("Network '%s' not found in saved networks\n", ssid);
    }
    return 0;
}

static int cmd_version(int argc, char *argv[]) {
    (void)argc; (void)argv;
    printf("scnw v0.1.0\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *cmd = argv[1];
    
    if (strcmp(cmd, "scan") == 0) {
        return cmd_scan(argc - 1, argv + 1);
    } else if (strcmp(cmd, "connect") == 0) {
        return cmd_connect(argc - 1, argv + 1);
    } else if (strcmp(cmd, "status") == 0) {
        return cmd_status(argc - 1, argv + 1);
    } else if (strcmp(cmd, "disconnect") == 0) {
        return cmd_disconnect(argc - 1, argv + 1);
    } else if (strcmp(cmd, "list") == 0) {
        return cmd_list(argc - 1, argv + 1);
    } else if (strcmp(cmd, "forget") == 0) {
        return cmd_forget(argc - 1, argv + 1);
    } else if (strcmp(cmd, "version") == 0) {
        return cmd_version(argc - 1, argv + 1);
    } else {
        printf("Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}