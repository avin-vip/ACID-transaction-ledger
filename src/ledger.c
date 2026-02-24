#include "ledger.h"
#include "wal.h"
#include "transaction.h"
#include <stdlib.h>
#include <string.h>

#define CHECKPOINT_INTERVAL 100
#define SNAPSHOT_ENTRY_SIZE (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(int64_t) + sizeof(uint64_t) + CURRENCY_LEN)
#define CASH_ACCOUNT_ID 0u

struct ledger {
    account_store_t *store;
    wal_t *wal;
    uint64_t next_tx_id;
    uint64_t ops_since_checkpoint;
};

struct replay_ctx {
    account_store_t **store_ptr;
    uint64_t *next_tx_id;
};

static int replay_cb(wal_op_t op, uint64_t tx_id, uint32_t account_id, int64_t amount,
                     account_type_t acct_type, const char *currency, void *ctx) {
    struct replay_ctx *rctx = (struct replay_ctx *)ctx;
    account_store_t *s = *rctx->store_ptr;
    switch (op) {
        case WAL_BEGIN_TX:
            if (*rctx->next_tx_id <= tx_id) *rctx->next_tx_id = tx_id + 1;
            break;
        case WAL_CREATE_ACCOUNT: {
            uint32_t id;
            if (account_create(s, (account_type_t)acct_type, currency ? currency : "USD", &id) != LEDGER_OK)
                return LEDGER_ERR_IO;
            break;
        }
        case WAL_DEBIT:
            account_apply_delta(s, account_id, -amount, tx_id);
            break;
        case WAL_CREDIT:
            account_apply_delta(s, account_id, amount, tx_id);
            break;
        case WAL_COMMIT:
        case WAL_ABORT:
            break;
        default:
            break;
    }
    return 0;
}

static int checkpoint_restore_cb(const void *snapshot, size_t len, void *ctx) {
    struct replay_ctx *rctx = (struct replay_ctx *)ctx;
    account_store_destroy(*rctx->store_ptr);
    *rctx->store_ptr = account_store_create();
    if (!*rctx->store_ptr) return LEDGER_ERR_NOMEM;
    account_store_t *s = *rctx->store_ptr;
    const uint8_t *p = (const uint8_t *)snapshot;
    uint32_t next_id, count;
    if (len < 8) return LEDGER_ERR_IO;
    memcpy(&next_id, p, 4);
    memcpy(&count, p + 4, 4);
    p += 8;
    for (uint32_t i = 0; i < count && (size_t)(p - (const uint8_t *)snapshot) + SNAPSHOT_ENTRY_SIZE <= len; i++) {
        uint32_t id;
        uint8_t type;
        int64_t balance;
        uint64_t version;
        char currency[CURRENCY_LEN];
        memcpy(&id, p, 4);
        memcpy(&type, p + 4, 1);
        memcpy(&balance, p + 8, 8);
        memcpy(&version, p + 16, 8);
        memcpy(currency, p + 24, CURRENCY_LEN);
        p += SNAPSHOT_ENTRY_SIZE;
        if (account_create_with_id(s, id, (account_type_t)type, currency) != LEDGER_OK) return LEDGER_ERR_IO;
        account_set_balance(s, id, balance, version);
    }
    *rctx->next_tx_id = next_id;
    return 0;
}

static ledger_err_t maybe_checkpoint(ledger_t *l) {
    l->ops_since_checkpoint++;
    if (l->ops_since_checkpoint < CHECKPOINT_INTERVAL) return LEDGER_OK;
    size_t cap = 8 + (size_t)account_count(l->store) * SNAPSHOT_ENTRY_SIZE;
    void *buf = malloc(cap);
    if (!buf) return LEDGER_OK;
    size_t len;
    if (account_serialize(l->store, (uint32_t)l->next_tx_id, buf, cap, &len) == LEDGER_OK && len > 0)
        wal_checkpoint(l->wal, buf, len);
    free(buf);
    l->ops_since_checkpoint = 0;
    return LEDGER_OK;
}

static ledger_err_t ensure_cash_account(ledger_t *l) {
    account_t a;
    if (account_get(l->store, CASH_ACCOUNT_ID, &a) == LEDGER_OK) return LEDGER_OK;
    ledger_err_t err = account_create_with_id(l->store, CASH_ACCOUNT_ID, ACCT_CHECKING, "USD");
    if (err != LEDGER_OK) return err;
    wal_append(l->wal, WAL_CREATE_ACCOUNT, 0, 0, 0, ACCT_CHECKING, "USD");
    return LEDGER_OK;
}

static ledger_err_t do_transfer(ledger_t *l, uint32_t from_id, uint32_t to_id, int64_t amount_cents) {
    if (amount_cents <= 0) return LEDGER_ERR_INVALID;
    uint64_t tx_id = l->next_tx_id++;
    wal_begin_tx(l->wal, tx_id);
    wal_append(l->wal, WAL_DEBIT, tx_id, from_id, amount_cents, ACCT_CHECKING, NULL);
    wal_append(l->wal, WAL_CREDIT, tx_id, to_id, amount_cents, ACCT_CHECKING, NULL);
    transaction_t *tx = transaction_begin(l->store, tx_id);
    if (!tx) {
        wal_abort(l->wal, tx_id);
        return LEDGER_ERR_NOMEM;
    }
    transaction_credit(tx, from_id, amount_cents);
    transaction_debit(tx, to_id, amount_cents);
    ledger_err_t err = transaction_commit(tx);
    transaction_destroy(tx);
    if (err != LEDGER_OK) {
        wal_abort(l->wal, tx_id);
        return err;
    }
    wal_commit(l->wal, tx_id);
    maybe_checkpoint(l);
    return LEDGER_OK;
}

ledger_t *ledger_open(const char *wal_path) {
    if (!wal_path) return NULL;
    ledger_t *l = calloc(1, sizeof(ledger_t));
    if (!l) return NULL;
    l->store = account_store_create();
    if (!l->store) {
        free(l);
        return NULL;
    }
    l->wal = wal_open(wal_path);
    if (!l->wal) {
        account_store_destroy(l->store);
        free(l);
        return NULL;
    }
    struct replay_ctx rctx = { .store_ptr = &l->store, .next_tx_id = &l->next_tx_id };
    ledger_err_t err = wal_replay(l->wal, replay_cb, checkpoint_restore_cb, &rctx);
    if (err != LEDGER_OK) {
        wal_close(l->wal);
        account_store_destroy(l->store);
        free(l);
        return NULL;
    }
    err = ensure_cash_account(l);
    if (err != LEDGER_OK) {
        wal_close(l->wal);
        account_store_destroy(l->store);
        free(l);
        return NULL;
    }
    return l;
}

void ledger_close(ledger_t *l) {
    if (!l) return;
    wal_close(l->wal);
    account_store_destroy(l->store);
    free(l);
}

ledger_err_t ledger_create_account(ledger_t *l, account_type_t type, const char *currency, uint32_t *out_id) {
    if (!l || !out_id) return LEDGER_ERR_INVALID;
    ledger_err_t err = account_create(l->store, type, currency ? currency : "USD", out_id);
    if (err != LEDGER_OK) return err;
    wal_append(l->wal, WAL_CREATE_ACCOUNT, 0, 0, 0, type, currency ? currency : "USD");
    maybe_checkpoint(l);
    return LEDGER_OK;
}

ledger_err_t ledger_deposit(ledger_t *l, uint32_t account_id, int64_t amount_cents) {
    if (!l || amount_cents <= 0) return LEDGER_ERR_INVALID;
    ledger_err_t err = ensure_cash_account(l);
    if (err != LEDGER_OK) return err;
    return do_transfer(l, CASH_ACCOUNT_ID, account_id, amount_cents);
}

ledger_err_t ledger_withdraw(ledger_t *l, uint32_t account_id, int64_t amount_cents) {
    if (!l || amount_cents <= 0) return LEDGER_ERR_INVALID;
    ledger_err_t err = ensure_cash_account(l);
    if (err != LEDGER_OK) return err;
    return do_transfer(l, account_id, CASH_ACCOUNT_ID, amount_cents);
}

ledger_err_t ledger_transfer(ledger_t *l, uint32_t from_id, uint32_t to_id, int64_t amount_cents) {
    if (!l) return LEDGER_ERR_INVALID;
    return do_transfer(l, from_id, to_id, amount_cents);
}

ledger_err_t ledger_balance(ledger_t *l, uint32_t account_id, int64_t *balance_cents) {
    if (!l || !balance_cents) return LEDGER_ERR_INVALID;
    account_t a;
    ledger_err_t err = account_get(l->store, account_id, &a);
    if (err != LEDGER_OK) return err;
    *balance_cents = a.balance_cents;
    return LEDGER_OK;
}

ledger_err_t ledger_history(ledger_t *l, uint32_t account_id, int64_t *out_credits, int64_t *out_debits, size_t *count) {
    (void)l;
    (void)account_id;
    (void)out_credits;
    (void)out_debits;
    if (count) *count = 0;
    return LEDGER_OK;
}

uint64_t ledger_next_tx_id(ledger_t *l) {
    return l ? l->next_tx_id : 0;
}
