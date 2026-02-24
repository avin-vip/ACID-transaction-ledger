#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "common.h"

typedef enum {
    ACCT_CHECKING,
    ACCT_SAVINGS,
    ACCT_INVESTMENT
} account_type_t;

typedef struct {
    uint32_t id;
    account_type_t type;
    int64_t balance_cents;
    char currency[CURRENCY_LEN];
    uint64_t version;
} account_t;

typedef struct account_store account_store_t;

account_store_t *account_store_create(void);
void account_store_destroy(account_store_t *s);
ledger_err_t account_create(account_store_t *s, account_type_t type, const char *currency, uint32_t *out_id);
ledger_err_t account_create_with_id(account_store_t *s, uint32_t id, account_type_t type, const char *currency);
ledger_err_t account_get(account_store_t *s, uint32_t id, account_t *out);
ledger_err_t account_apply_delta(account_store_t *s, uint32_t id, int64_t delta_cents, uint64_t version);
ledger_err_t account_set_balance(account_store_t *s, uint32_t id, int64_t balance_cents, uint64_t version);
uint32_t account_count(const account_store_t *s);
ledger_err_t account_serialize(const account_store_t *s, uint32_t next_tx_id, void *buf, size_t cap, size_t *out_len);

#endif
