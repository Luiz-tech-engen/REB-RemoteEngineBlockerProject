/**
 * @file    nvm.h
 * @brief   Non-volatile memory persistence interface.
 *
 * Declares the result codes and API for reading, writing, and
 * invalidating persisted state. The underlying storage is abstracted;
 * the implementation may use static simulation or a hardware HAL.
 */

#ifndef NVM_H
#define NVM_H

#include "reb/reb_types.h"
#include <stdbool.h>

/**
 * @brief Return codes for NVM operations.
 */
typedef enum {
    NVM_OK         = 0U,
    NVM_ERR_CRC    = 1U, /**< CRC mismatch; stored data is corrupt.      */
    NVM_ERR_EMPTY  = 2U, /**< Storage has never been written.             */
    NVM_ERR_WRITE  = 3U  /**< Physical write operation failed.            */
} nvm_result_t;

/**
 * @brief Serialises @p data with a CRC32 checksum and commits it to storage.
 *
 * @param data  Pointer to the state structure to persist. Must not be NULL.
 * @return      @c NVM_OK on success, or an @c nvm_result_t error code.
 */
nvm_result_t nvm_write_state(const nvm_data_t *data);

/**
 * @brief Reads persisted state from storage and verifies its CRC32.
 *
 * @param data  Output buffer for the retrieved state. Must not be NULL.
 * @return      @c NVM_OK on success, @c NVM_ERR_EMPTY if no data has been
 *              written, or @c NVM_ERR_CRC if integrity verification fails.
 */
nvm_result_t nvm_read_state(nvm_data_t *data);

/**
 * @brief Clears stored data and marks the storage region as invalid.
 */
void         nvm_invalidate(void);

/**
 * @brief Returns whether the storage contains a previously committed record.
 *
 * @return @c true if a write has been performed since the last invalidation.
 */
bool         nvm_is_valid(void);

#endif /* NVM_H */
