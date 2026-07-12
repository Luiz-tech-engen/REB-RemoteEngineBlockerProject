/**
 * @file    nvm.c
 * @brief   Non-volatile memory implementation using CRC32-protected storage.
 *
 * Storage is backed by a static buffer for simulation environments.
 * The CRC32 implementation uses the IEEE 802.3 polynomial (0xEDB88320)
 * computed bit-by-bit without a lookup table.
 */

#include "nvm.h"
#include <string.h>

static nvm_data_t nvm_sim_buf;
static bool       nvm_sim_valid = false;

/**
 * @brief Computes a CRC32 checksum over a byte buffer.
 *
 * Uses the IEEE 802.3 reflected polynomial (0xEDB88320). The computation
 * is bit-serial and requires no lookup table.
 *
 * @param data  Pointer to the input byte array.
 * @param len   Number of bytes to process.
 * @return      32-bit CRC value.
 */
static uint32_t crc32_compute(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint8_t  bit;

    for (i = 0U; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (bit = 0U; bit < 8U; bit++) {
            if ((crc & 1UL) != 0UL) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

nvm_result_t nvm_write_state(const nvm_data_t *data)
{
    nvm_data_t buf;
    (void)memcpy(&buf, data, sizeof(buf));

    /**
     * @note CRC is computed over all fields except the trailing @c crc32
     *       member itself, which is then populated with the result.
     */
    buf.crc32 = crc32_compute((const uint8_t *)&buf,
                               (uint32_t)(sizeof(buf) - sizeof(buf.crc32)));

    (void)memcpy(&nvm_sim_buf, &buf, sizeof(nvm_sim_buf));
    nvm_sim_valid = true;

    return NVM_OK;
}

nvm_result_t nvm_read_state(nvm_data_t *data)
{
    uint32_t expected_crc;

    if (!nvm_sim_valid) {
        return NVM_ERR_EMPTY;
    }

    (void)memcpy(data, &nvm_sim_buf, sizeof(*data));

    expected_crc = crc32_compute((const uint8_t *)data,
                                  (uint32_t)(sizeof(*data) - sizeof(data->crc32)));
    if (expected_crc != data->crc32) {
        return NVM_ERR_CRC;
    }

    return NVM_OK;
}

void nvm_invalidate(void)
{
    (void)memset(&nvm_sim_buf, 0, sizeof(nvm_sim_buf));
    nvm_sim_valid = false;
}

bool nvm_is_valid(void)
{
    return nvm_sim_valid;
}
