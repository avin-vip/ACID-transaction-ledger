#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "common.h"
#include "account.h"

typedef struct journal_entry {
    uint32_t account_id;
    int64_t amount_cents;
    bool is_debit;
} journal_entry_t;

typedef struct transaction transaction_t;

transaction_t *transaction_begin(account_store_t *store, uint64_t tx_id);
ledger_err_t transaction_debit(transaction_t *tx, uint32_t account_id, int64_t amount_cents);
ledger_err_t transaction_credit(transaction_t *tx, uint32_t account_id, int64_t amount_cents);
ledger_err_t transaction_commit(transaction_t *tx);
void transaction_abort(transaction_t *tx);
void transaction_destroy(transaction_t *tx);
bool transaction_is_committed(const transaction_t *tx);
bool transaction_is_aborted(const transaction_t *tx);

#endif
