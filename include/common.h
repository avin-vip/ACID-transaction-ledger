#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define LEDGER_OK            0
#define LEDGER_ERR_NOMEM    -1
#define LEDGER_ERR_IO       -2
#define LEDGER_ERR_INVALID  -3
#define LEDGER_ERR_NOTFOUND -4
#define LEDGER_ERR_DEADLOCK -5
#define LEDGER_ERR_CONSTRAINT -6

#define MAX_ACCOUNTS         (1u << 20)
#define MAX_TX_ENTRIES       4096
#define WAL_PATH_MAX         256
#define CURRENCY_LEN         4
#define WAL_MAGIC             0xAC1D0001u

typedef int ledger_err_t;

uint32_t crc32(const void *data, size_t len);

#endif
