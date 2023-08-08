
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define APP_CONFIG_PATH  "/usr/data/itop_yolo.txt"

int app_config_init(struct app_config * config) {
    int             ret = -1;
    int             length;
    FILE            *fp = NULL;

    memset(config, 0, sizeof(struct app_config));

    ret = -1;

    fp = fopen(APP_CONFIG_PATH, "rb");
    if(fp != NULL) {
        length = fread(config, 1, sizeof(struct app_config), fp);
        fclose(fp);

        if (length == sizeof(struct app_config)) {
            ret = 0;
        }
    }

_err1:
    if (ret != 0) {
        config->detect_region.lt_x = 0;
        config->detect_region.lt_y = 0;
        config->detect_region.rb_x = 8191;
        config->detect_region.rb_y = 8191;

        config->min_region.lt_x = 10;
        config->min_region.lt_y = 10;
        config->min_region.rb_x = 100;
        config->min_region.rb_y = 100;

        strcpy(config->net_ip, "10.34.7.29");
        config->net_port = 3000;

        config->log_level = 3;

        config->linkage_event = 0;
    }

    return 0;
}

int app_config_save(struct app_config * config) {
    FILE            *fp = NULL;

    fp = fopen(APP_CONFIG_PATH, "wb");
    if(fp != NULL) {
        fwrite(config, 1, sizeof(struct app_config), fp);
        fclose(fp);

        return 0;
    }
    return -1;
}

int app_config_parse(char * buffer, struct app_config * config) {
    // detect_region=[[1183,2406],[3049,6696]]&min_region=[[5931,5502],[6113,5801]]&objectTypes=3&objectTypes=4&event_link_snapshot=on

    int             snap = -1;
    int             i, k, s;
    int             state = 0;
    char            word[32];

    // delete extra spaces
    for (i = 0, k = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] > ' ') {
            buffer[k++] = buffer[i];
        }
    }
    buffer[k++] = '&';
    buffer[k++] = '\0';

    // Start parsing
    for (i = 0, k = 0, s = 0; buffer[i] != '\0'; i++) {
        switch (state) {
        case 0:
            if ((buffer[i] >= 'a') && (buffer[i] <= 'z')) {
                word[k++] = buffer[i];
            }
            if ((buffer[i] >= 'A') && (buffer[i] <= 'Z')) {
                word[k++] = buffer[i];
            }
            if (buffer[i] == '_') {
                word[k++] = buffer[i];
            }
            if (buffer[i] == '='|| buffer[i] == ':') {
                word[k] = 0;
                k = 0;
                if (!strcmp(word, "detect_region")) {
                    state = 100;
                    config->mask |= _CCM_BIT_DETECTREGION;
                    if (snap == -1) {
                        snap = 0;
                    }
                }
                else if (!strcmp(word, "min_region")) {
                    state = 200;
                    config->mask |= _CCM_BIT_MIN_REGION;
                    if (snap == -1) {
                        snap = 0;
                    }
                }
                else if (!strcmp(word, "objectTypes")) {
                    state = 300;
                    config->mask |= _CCM_BIT_SELECTED_OBJ;
                    if (snap == -1) {
                        snap = 0;
                    }
                }
                else if (!strcmp(word, "event_link_snapshot")) {
                    state = 400;
                    snap  = 1;
                    config->mask |= _CCM_BIT_LINKAGE;
                }
                else if (!strcmp(word, "logLevel")) {
                    state = 500;
                    config->mask |= _CCM_BIT_LOG_LEVEL;
                }
                else if (!strcmp(word, "port")) {
                    state = 600;
                    config->mask |= _CCM_BIT_NET_INFO;
                }
                else if (!strcmp(word, "ip")) {
                    state = 700;
                    config->mask |= _CCM_BIT_NET_INFO;
                }
                else if (!strcmp(word, "min_size")) {
                    state = -1;
                }
                else {
                    return -1;
                }
            }
            break;

        case 100:
            if (buffer[i] == '[') {
                state = 101;
            }
            else {
                return -1;
            }
            break;

        case 101:
            if (buffer[i] == '[') {
                state = 102;
            }
            else {
                return -1;
            }
            break;

        case 102:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ',') {
                word[k] = 0;
                k = 0;
                state = 103;
                config->detect_region.lt_x = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 103:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ']') {
                word[k] = 0;
                k = 0;
                state = 104;
                config->detect_region.lt_y = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 104:
            if (buffer[i] == ',') {
                state = 105;
            }
            else {
                return -1;
            }
            break;

        case 105:
            if (buffer[i] == '[') {
                state = 106;
            }
            else {
                return -1;
            }
            break;

        case 106:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ',') {
                word[k] = 0;
                k = 0;
                state = 107;
                config->detect_region.rb_x = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 107:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ']') {
                word[k] = 0;
                k = 0;
                state = 108;
                config->detect_region.rb_y = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 108:
            if (buffer[i] == ']') {
                state = 109;
            }
            else {
                return -1;
            }
            break;

        case 109:
            if (buffer[i] == '&') {
                state = 0;
            }
            else {
                return -1;
            }
            break;

        case 200:
            if (buffer[i] == '[') {
                state = 201;
            }
            else {
                return -1;
            }
            break;

        case 201:
            if (buffer[i] == '[') {
                state = 202;
            }
            else {
                return -1;
            }
            break;

        case 202:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ',') {
                word[k] = 0;
                k = 0;
                state = 203;
                config->min_region.lt_x = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 203:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ']') {
                word[k] = 0;
                k = 0;
                state = 204;
                config->min_region.lt_y = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 204:
            if (buffer[i] == ',') {
                state = 205;
            }
            else {
                return -1;
            }
            break;

        case 205:
            if (buffer[i] == '[') {
                state = 206;
            }
            else {
                return -1;
            }
            break;

        case 206:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ',') {
                word[k] = 0;
                k = 0;
                state = 207;
                config->min_region.rb_x = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 207:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == ']') {
                word[k] = 0;
                k = 0;
                state = 208;
                config->min_region.rb_y = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 208:
            if (buffer[i] == ']') {
                state = 209;
            }
            else {
                return -1;
            }
            break;

        case 209:
            if (buffer[i] == '&') {
                state = 0;
            }
            else {
                return -1;
            }
            break;

        case 300:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '&') {
                word[k] = 0;
                k = 0;
                state = 0;
                if (s < 16) {
                    config->selected_objs[s++] = atoi(word);
                }
            }
            else {
                return -1;
            }
            break;

        case 400:
            if ((buffer[i] >= 'a') && (buffer[i] <= 'z')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '&') {
                word[k] = 0;
                k = 0;
                state = 0;
                if (0 == strcmp(word, "on")) {
                    config->linkage_event = 1;
                }
                else if (0 == strcmp(word, "true")) {
                    config->linkage_event = 1;
                }
                else if (0 == strcmp(word, "enable")) {
                    config->linkage_event = 1;
                }
                else {
                    config->linkage_event = 0;
                }
            }
            else {
                return -1;
            }
            break;

        case 500:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '&') {
                word[k] = 0;
                k = 0;
                state = 0;
                config->log_level = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 600:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '&') {
                word[k] = 0;
                k = 0;
                state = 0;
                config->net_port = atoi(word);
            }
            else {
                return -1;
            }
            break;

        case 700:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '.') {
                word[k++] = buffer[i];
                state = 701;
            }
            else {
                return -1;
            }
            break;

        case 701:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '.') {
                word[k++] = buffer[i];
                state = 702;
            }
            else {
                return -1;
            }
            break;

        case 702:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '.') {
                word[k++] = buffer[i];
                state = 703;
            }
            else {
                return -1;
            }
            break;

        case 703:
            if ((buffer[i] >= '0') && (buffer[i] <= '9')) {
                word[k++] = buffer[i];
            }
            else if (buffer[i] == '&') {
                word[k] = 0;
                state = 0;
                if (k < 16) {
                    strncpy(config->net_ip, word, 16);
                    config->net_ip[k] = 0;
                }
                else {
                    return -1;
                }
                k = 0;
            }
            else {
                return -1;
            }
            break;

        case -1:
            if (buffer[i] == '&'){
                state = 0;
            }
            break;
        }
    }

    if (snap == 0) {
        config->linkage_event = 0;
    }

    return 0;
}

