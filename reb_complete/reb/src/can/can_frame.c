#include "can_frame.h"

#include <string.h>

static bool can_frame_is_valid_id_type(can_id_type_t id_type)
{
    return (id_type == CAN_ID_TYPE_STANDARD) ||
           (id_type == CAN_ID_TYPE_EXTENDED);
}

static bool can_frame_is_valid_frame_type(can_frame_type_t frame_type)
{
    return (frame_type == CAN_FRAME_TYPE_DATA) ||
           (frame_type == CAN_FRAME_TYPE_REMOTE);
}

static bool can_frame_is_valid_id(uint32_t id, can_id_type_t id_type)
{
    bool is_valid = false;

    if (id_type == CAN_ID_TYPE_STANDARD)
    {
        is_valid = (id <= CAN_STD_ID_MAX);
    }
    else if (id_type == CAN_ID_TYPE_EXTENDED)
    {
        is_valid = (id <= CAN_EXT_ID_MAX);
    }
    else
    {
        is_valid = false;
    }

    return is_valid;
}

void can_frame_init(can_frame_t *frame)
{
    if (frame != NULL)
    {
        frame->id = 0U;
        frame->id_type = CAN_ID_TYPE_STANDARD;
        frame->frame_type = CAN_FRAME_TYPE_DATA;
        frame->dlc = 0U;
        (void)memset(frame->data, 0, sizeof(frame->data));
    }
}

can_frame_status_t can_frame_validate(const can_frame_t *frame)
{
    can_frame_status_t status = CAN_FRAME_STATUS_OK;

    if (frame == NULL)
    {
        status = CAN_FRAME_STATUS_NULL_POINTER;
    }
    else if (!can_frame_is_valid_id_type(frame->id_type))
    {
        status = CAN_FRAME_STATUS_INVALID_ID_TYPE;
    }
    else if (!can_frame_is_valid_frame_type(frame->frame_type))
    {
        status = CAN_FRAME_STATUS_INVALID_FRAME_TYPE;
    }
    else if (!can_frame_is_valid_id(frame->id, frame->id_type))
    {
        status = CAN_FRAME_STATUS_INVALID_ID;
    }
    else if (frame->dlc > CAN_DLC_MAX)
    {
        status = CAN_FRAME_STATUS_INVALID_DLC;
    }
    else
    {
        status = CAN_FRAME_STATUS_OK;
    }

    return status;
}

bool can_frame_is_valid(const can_frame_t *frame)
{
    return (can_frame_validate(frame) == CAN_FRAME_STATUS_OK);
}

can_frame_status_t can_frame_set_data(
    can_frame_t *frame,
    const uint8_t *data,
    uint8_t dlc
)
{
    can_frame_status_t status = CAN_FRAME_STATUS_OK;

    if (frame == NULL)
    {
        status = CAN_FRAME_STATUS_NULL_POINTER;
    }
    else if (dlc > CAN_DLC_MAX)
    {
        status = CAN_FRAME_STATUS_INVALID_DLC;
    }
    else if ((dlc > 0U) && (data == NULL))
    {
        status = CAN_FRAME_STATUS_NULL_POINTER;
    }
    else
    {
        frame->dlc = dlc;

        if (dlc > 0U)
        {
            (void)memcpy(frame->data, data, dlc);
        }

        if (dlc < CAN_FRAME_MAX_DATA_LEN)
        {
            (void)memset(&frame->data[dlc], 0, CAN_FRAME_MAX_DATA_LEN - dlc);
        }
    }

    return status;
}