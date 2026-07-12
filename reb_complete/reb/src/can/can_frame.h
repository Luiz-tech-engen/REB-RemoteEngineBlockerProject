#ifndef REB_CAN_FRAME_H
#define REB_CAN_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_FRAME_MAX_DATA_LEN    (8U)
#define CAN_STD_ID_MAX            (0x7FFU)
#define CAN_EXT_ID_MAX            (0x1FFFFFFFU)
#define CAN_DLC_MAX               (8U)

typedef enum
{
    CAN_ID_TYPE_STANDARD = 0,
    CAN_ID_TYPE_EXTENDED
} can_id_type_t;

typedef enum
{
    CAN_FRAME_TYPE_DATA = 0,
    CAN_FRAME_TYPE_REMOTE
} can_frame_type_t;

typedef enum
{
    CAN_FRAME_STATUS_OK = 0,
    CAN_FRAME_STATUS_NULL_POINTER,
    CAN_FRAME_STATUS_INVALID_ID_TYPE,
    CAN_FRAME_STATUS_INVALID_FRAME_TYPE,
    CAN_FRAME_STATUS_INVALID_ID,
    CAN_FRAME_STATUS_INVALID_DLC
} can_frame_status_t;

typedef struct
{
    uint32_t id;
    can_id_type_t id_type;
    can_frame_type_t frame_type;
    uint8_t dlc;
    uint8_t data[CAN_FRAME_MAX_DATA_LEN];
} can_frame_t;

void can_frame_init(can_frame_t *frame);

can_frame_status_t can_frame_validate(const can_frame_t *frame);

bool can_frame_is_valid(const can_frame_t *frame);

can_frame_status_t can_frame_set_data(
    can_frame_t *frame,
    const uint8_t *data,
    uint8_t dlc
);

#ifdef __cplusplus
}
#endif

#endif /* REB_CAN_FRAME_H */