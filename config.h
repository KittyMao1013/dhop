
#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef enum {
    _CCM_BIT_NONE         = 0,
    _CCM_BIT_DETECTREGION = (1 << 0),  // detect region
    _CCM_BIT_MIN_REGION   = (1 << 1),  // min region
    _CCM_BIT_SELECTED_OBJ = (1 << 2),  // select objects
    _CCM_BIT_LINKAGE      = (1 << 3),  // linkage event
    _CCM_BIT_NET_INFO     = (1 << 4),
    _CCM_BIT_LOG_LEVEL    = (1 << 5),
} config_change_mask_t;

struct app_config {
    struct {
        int     lt_x;
        int     lt_y;
        int     rb_x;
        int     rb_y;
    } detect_region;
    struct {
        int     lt_x;
        int     lt_y;
        int     rb_x;
        int     rb_y;
    } min_region;
    int         selected_objs[16];
    int         linkage_event;

    char        net_ip[16];
    int         net_port;

    int         log_level;

    int         mask;
};

extern int app_config_init(struct app_config * config);

extern int app_config_parse(char * buffer, struct app_config * config);

extern int app_config_save(struct app_config * config);

#endif
