#ifndef __OBJECT_OT_H__
#define __OBJECT_OT_H__

#include "itop_type_def.h"

enum {
    _OBJ_STATE_NEW  = 0,
    _OBJ_STATE_UPDATE,
    _OBJ_STATE_HIDE,
    _OBJ_STATE_REMOVE,
};

typedef struct {
    int32_t        id;
    int32_t        state;
    ITOP_Rect16       rect;
    ITOP_Rect16       actual;
    int32_t        classId;
} obj_info_t;

extern int object_ot_init();
extern int object_ot_deinit();

// Put new results in
extern int object_ot_update(int num, obj_info_t* list);

// Get the latest results
extern int object_ot_result(int* num, obj_info_t* list);

#endif

