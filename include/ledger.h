#ifndef LEDGER_H
#define LEDGER_H

#include "common.h"
#include "account.h"

typedef struct ledger ledger_t;

ledger_t *ledger_open(const char *wal_path);
void ledger_close(ledger_t *l);
ledger_err_t ledger_create_account(ledger_t *l, account_type_t type, const char *currency, uint32_t *out_id);
ledger_err_t ledger_deposit(ledger_t *l, uint32_t account_id, int64_t amount_cents);
ledger_err_t ledger_withdraw(ledger_t *l, uint32_t account_id, int64_t amount_cents);
ledger_err_t ledger_transfer(ledger_t *l, uint32_t from_id, uint32_t to_id, int64_t amount_cents);
ledger_err_t ledger_balance(ledger_t *l, uint32_t account_id, int64_t *balance_cents);
ledger_err_t ledger_history(ledger_t *l, uint32_t account_id, int64_t *out_credits, int64_t *out_debits, size_t *count);
uint64_t ledger_next_tx_id(ledger_t *l);

#endif
