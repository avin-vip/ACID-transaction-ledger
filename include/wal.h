#ifndef WAL_H
#define WAL_H

#include "common.h"
#include "account.h"

typedef enum {
    WAL_BEGIN_TX,
    WAL_DEBIT,
    WAL_CREDIT,
    WAL_COMMIT,
    WAL_ABORT,
    WAL_CHECKPOINT,
    WAL_CREATE_ACCOUNT
} wal_op_t;

typedef struct wal wal_t;

typedef int (*wal_replay_cb_t)(wal_op_t op, uint64_t tx_id, uint32_t account_id, int64_t amount,
                               account_type_t acct_type, const char *currency, void *ctx);
typedef int (*wal_checkpoint_restore_cb_t)(const void *snapshot, size_t len, void *ctx);

wal_t *wal_open(const char *path);
void wal_close(wal_t *w);
ledger_err_t wal_append(wal_t *w, wal_op_t op, uint64_t tx_id, uint32_t account_id, int64_t amount,
                        account_type_t acct_type, const char *currency);
ledger_err_t wal_begin_tx(wal_t *w, uint64_t tx_id);
ledger_err_t wal_commit(wal_t *w, uint64_t tx_id);
ledger_err_t wal_abort(wal_t *w, uint64_t tx_id);
ledger_err_t wal_checkpoint(wal_t *w, const void *snapshot, size_t len);
ledger_err_t wal_replay(wal_t *w, wal_replay_cb_t cb, wal_checkpoint_restore_cb_t checkpoint_cb, void *ctx);

#endif
