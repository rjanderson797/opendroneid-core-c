#include <ctype.h>
#include <ncurses.h>
#include <opendroneid.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_OUTPUT 131072
#define MAX_DRONES 128
#define MAX_FRAME_BYTES 2048

struct scan_result {
    char mac[32];
    char uas_id[ODID_ID_SIZE + 1];
    float lat;
    float lon;
    bool has_location;
};

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static int hex_to_bytes(const char *in, uint8_t *out, size_t out_size)
{
    int hi = -1;
    size_t count = 0;

    for (size_t i = 0; in[i] != '\0'; i++) {
        int v = hex_value(in[i]);
        if (v < 0) continue;
        if (hi < 0) {
            hi = v;
            continue;
        }
        if (count >= out_size) return -1;
        out[count++] = (uint8_t)((hi << 4) | v);
        hi = -1;
    }

    return hi >= 0 ? -1 : (int)count;
}

static int decode_remoteid_vendor_data(const char *mac, const char *hex, struct scan_result *res)
{
    uint8_t vendor_data[MAX_FRAME_BYTES];
    ODID_UAS_Data uas;
    int payload_len = hex_to_bytes(hex, vendor_data, sizeof(vendor_data));
    if (payload_len < 3) return -1;

    if (vendor_data[0] != 0x0d) return -1;

    if (odid_message_process_pack(&uas, &vendor_data[2], (size_t)(payload_len - 2)) < 0) return -1;

    strncpy(res->mac, mac, sizeof(res->mac) - 1);
    res->mac[sizeof(res->mac) - 1] = '\0';

    if (uas.BasicIDValid[0]) {
        strncpy(res->uas_id, uas.BasicID[0].UASID, sizeof(res->uas_id) - 1);
        res->uas_id[sizeof(res->uas_id) - 1] = '\0';
    } else {
        strncpy(res->uas_id, "<unknown>", sizeof(res->uas_id) - 1);
        res->uas_id[sizeof(res->uas_id) - 1] = '\0';
    }

    if (uas.LocationValid) {
        res->lat = uas.Location.Latitude;
        res->lon = uas.Location.Longitude;
        res->has_location = true;
    } else {
        res->lat = 0.0f;
        res->lon = 0.0f;
        res->has_location = false;
    }

    return 0;
}

static int perform_scan(const char *iface, struct scan_result *results, size_t max_results, char *err, size_t err_size)
{
    char cmd[128];
    char *scan_output = malloc(MAX_OUTPUT);
    FILE *fp;
    size_t out_len = 0;
    char line[1024];
    char current_mac[32] = "";
    int found = 0;

    if (!scan_output) {
        snprintf(err, err_size, "allocation failed");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "iw dev %s scan 2>/dev/null", iface);
    fp = popen(cmd, "r");
    if (!fp) {
        free(scan_output);
        snprintf(err, err_size, "failed to launch scan command");
        return -1;
    }

    scan_output[0] = '\0';
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t n = strlen(line);
        if (out_len + n + 1 >= MAX_OUTPUT) break;
        memcpy(scan_output + out_len, line, n);
        out_len += n;
        scan_output[out_len] = '\0';
    }
    pclose(fp);

    char *save = NULL;
    for (char *l = strtok_r(scan_output, "\n", &save); l != NULL; l = strtok_r(NULL, "\n", &save)) {
        if (strncmp(l, "BSS ", 4) == 0) {
            char bssid[32];
            if (sscanf(l, "BSS %31[^ (]", bssid) == 1) {
                strncpy(current_mac, bssid, sizeof(current_mac) - 1);
                current_mac[sizeof(current_mac) - 1] = '\0';
            }
            continue;
        }

        char *p = strstr(l, "OUI fa:0b:bc, data:");
        if (!p) continue;

        if (found >= (int)max_results) break;
        p = strchr(p, ':');
        if (!p) continue;
        p++;

        if (decode_remoteid_vendor_data(current_mac[0] ? current_mac : "<unknown>", p, &results[found]) == 0)
            found++;
    }

    free(scan_output);
    err[0] = '\0';
    return found;
}

int main(int argc, char **argv)
{
    const char *iface = "wlan0";
    struct scan_result drones[MAX_DRONES];
    int ch;
    int count = 0;
    char err[256] = "";
    int interval_seconds = 2;

    if (argc > 1) iface = argv[1];

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    while (1) {
        count = perform_scan(iface, drones, MAX_DRONES, err, sizeof(err));

        erase();
        mvprintw(0, 0, "remoteidv1 Wi-Fi Remote ID scanner  | iface: %s", iface);
        mvprintw(1, 0, "Scanning every %d seconds. Press 'q' to quit.", interval_seconds);

        if (count < 0) {
            mvprintw(3, 0, "Scan error: %s", err);
        } else if (count == 0) {
            mvprintw(3, 0, "No Remote ID beacons found.");
        } else {
            mvprintw(3, 0, "Detected %d Remote ID transmitter(s):", count);
            for (int i = 0; i < count && i < LINES - 6; i++) {
                if (drones[i].has_location) {
                    mvprintw(5 + i, 0, "%2d) %s  UAS ID: %s  lat=%.6f lon=%.6f",
                             i + 1, drones[i].mac, drones[i].uas_id, drones[i].lat, drones[i].lon);
                } else {
                    mvprintw(5 + i, 0, "%2d) %s  UAS ID: %s  location unavailable",
                             i + 1, drones[i].mac, drones[i].uas_id);
                }
            }
        }

        refresh();

        for (int i = 0; i < interval_seconds * 10; i++) {
            ch = getch();
            if (ch == 'q' || ch == 'Q') {
                endwin();
                return 0;
            }
            usleep(100000);
        }
    }
}
