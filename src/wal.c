#include "wal.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WAL_RECORD_PAYLOAD_SIZE 32
#define WAL_RECORD_SIZE         (WAL_RECORD_PAYLOAD_SIZE + 4)

#pragma pack(push, 1)
typedef struct {
    uint8_t op;
    uint8_t pad1[3];
    uint64_t tx_id;
    uint32_t account_id;
    int64_t amount;
    uint32_t acct_type;
    char currency[CURRENCY_LEN];
    uint8_t pad2[4];
} wal_record_t;
#pragma pack(pop)

struct wal {
    FILE *fp;
    char path[WAL_PATH_MAX];
};

static void encode_record(uint8_t *buf, wal_op_t op, uint64_t tx_id, uint32_t account_id,
                          int64_t amount, account_type_t acct_type, const char *currency) {
    wal_record_t *r = (wal_record_t *)buf;
    memset(r, 0, WAL_RECORD_PAYLOAD_SIZE);
    r->op = (uint8_t)op;
    r->tx_id = tx_id;
    r->account_id = account_id;
    r->amount = amount;
    r->acct_type = (uint32_t)acct_type;
    if (currency) memcpy(r->currency, currency, CURRENCY_LEN);
}

wal_t *wal_open(const char *path) {
    if (!path || strlen(path) >= WAL_PATH_MAX) return NULL;
    wal_t *w = calloc(1, sizeof(wal_t));
    if (!w) return NULL;
    strncpy(w->path, path, WAL_PATH_MAX - 1);
    w->fp = fopen(path, "ab");
    if (!w->fp) {
        free(w);
        return NULL;
    }
    return w;
}

void wal_close(wal_t *w) {
    if (!w) return;
    if (w->fp) fclose(w->fp);
    w->fp = NULL;
    free(w);
}

static ledger_err_t append_record(wal_t *w, const uint8_t *payload) {
    uint32_t crc = crc32(payload, WAL_RECORD_PAYLOAD_SIZE);
    if (fwrite(payload, 1, WAL_RECORD_PAYLOAD_SIZE, w->fp) != WAL_RECORD_PAYLOAD_SIZE)
        return LEDGER_ERR_IO;
    if (fwrite(&crc, 1, 4, w->fp) != 4) return LEDGER_ERR_IO;
    if (fflush(w->fp) != 0) return LEDGER_ERR_IO;
    return LEDGER_OK;
}

ledger_err_t wal_append(wal_t *w, wal_op_t op, uint64_t tx_id, uint32_t account_id, int64_t amount,
                        account_type_t acct_type, const char *currency) {
    if (!w || !w->fp) return LEDGER_ERR_INVALID;
    uint8_t buf[WAL_RECORD_PAYLOAD_SIZE];
    encode_record(buf, op, tx_id, account_id, amount, acct_type, currency);
    return append_record(w, buf);
}

ledger_err_t wal_begin_tx(wal_t *w, uint64_t tx_id) {
    return wal_append(w, WAL_BEGIN_TX, tx_id, 0, 0, ACCT_CHECKING, NULL);
}

ledger_err_t wal_commit(wal_t *w, uint64_t tx_id) {
    return wal_append(w, WAL_COMMIT, tx_id, 0, 0, ACCT_CHECKING, NULL);
}

ledger_err_t wal_abort(wal_t *w, uint64_t tx_id) {
    return wal_append(w, WAL_ABORT, tx_id, 0, 0, ACCT_CHECKING, NULL);
}

ledger_err_t wal_checkpoint(wal_t *w, const void *snapshot, size_t len) {
    if (!w || !w->fp) return LEDGER_ERR_INVALID;
    uint8_t buf[WAL_RECORD_PAYLOAD_SIZE];
    memset(buf, 0, sizeof(buf));
    ((wal_record_t *)buf)->op = (uint8_t)WAL_CHECKPOINT;
    ((wal_record_t *)buf)->tx_id = (uint64_t)len;
    ledger_err_t err = append_record(w, buf);
    if (err != LEDGER_OK) return err;
    if (snapshot && len > 0 && fwrite(snapshot, 1, len, w->fp) != len) return LEDGER_ERR_IO;
    if (fflush(w->fp) != 0) return LEDGER_ERR_IO;
    return LEDGER_OK;
}

static ledger_err_t read_record(FILE *fp, uint8_t *payload, uint32_t *crc_out) {
    if (fread(payload, 1, WAL_RECORD_PAYLOAD_SIZE, fp) != WAL_RECORD_PAYLOAD_SIZE)
        return feof(fp) ? LEDGER_ERR_NOTFOUND : LEDGER_ERR_IO;
    uint32_t stored;
    if (fread(&stored, 1, 4, fp) != 4) return LEDGER_ERR_IO;
    uint32_t computed = crc32(payload, WAL_RECORD_PAYLOAD_SIZE);
    if (stored != computed) return LEDGER_ERR_IO;
    if (crc_out) *crc_out = stored;
    return LEDGER_OK;
}

ledger_err_t wal_replay(wal_t *w, wal_replay_cb_t cb, wal_checkpoint_restore_cb_t checkpoint_cb, void *ctx) {
    if (!w || !cb) return LEDGER_ERR_INVALID;
    if (fclose(w->fp) != 0) return LEDGER_ERR_IO;
    w->fp = fopen(w->path, "rb");
    if (!w->fp) return LEDGER_ERR_IO;
    uint8_t buf[WAL_RECORD_PAYLOAD_SIZE];
    ledger_err_t err;
    while ((err = read_record(w->fp, buf, NULL)) == LEDGER_OK) {
        wal_record_t *r = (wal_record_t *)buf;
        wal_op_t op = (wal_op_t)r->op;
        if (op == WAL_CHECKPOINT) {
            size_t snap_len = (size_t)r->tx_id;
            if (snap_len > 0 && checkpoint_cb) {
                void *snap = malloc(snap_len);
                if (!snap) return LEDGER_ERR_NOMEM;
                if (fread(snap, 1, snap_len, w->fp) != snap_len) {
                    free(snap);
                    return LEDGER_ERR_IO;
                }
                int rc = checkpoint_cb(snap, snap_len, ctx);
                free(snap);
                if (rc != 0) return (ledger_err_t)rc;
            } else if (snap_len > 0) {
                if (fseek(w->fp, (long)snap_len, SEEK_CUR) != 0) return LEDGER_ERR_IO;
            }
            continue;
        }
        int rc = cb(op, r->tx_id, r->account_id, r->amount, (account_type_t)r->acct_type, r->currency, ctx);
        if (rc != 0) return (ledger_err_t)rc;
    }
    if (err != LEDGER_OK && err != LEDGER_ERR_NOTFOUND) return err;
    if (fclose(w->fp) != 0) return LEDGER_ERR_IO;
    w->fp = fopen(w->path, "ab");
    if (!w->fp) return LEDGER_ERR_IO;
    return LEDGER_OK;
}
