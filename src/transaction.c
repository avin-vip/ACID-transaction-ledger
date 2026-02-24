#include "transaction.h"
#include <stdlib.h>
#include <string.h>

struct journal_entry_node {
    journal_entry_t entry;
    struct journal_entry_node *next;
};

struct transaction {
    account_store_t *store;
    uint64_t tx_id;
    struct journal_entry_node *entries;
    int64_t total_debits;
    int64_t total_credits;
    bool committed;
    bool aborted;
};

transaction_t *transaction_begin(account_store_t *store, uint64_t tx_id) {
    if (!store) return NULL;
    transaction_t *tx = calloc(1, sizeof(transaction_t));
    if (!tx) return NULL;
    tx->store = store;
    tx->tx_id = tx_id;
    return tx;
}

static void free_entries(struct journal_entry_node *n) {
    while (n) {
        struct journal_entry_node *next = n->next;
        free(n);
        n = next;
    }
}

static ledger_err_t append_entry(transaction_t *tx, uint32_t account_id, int64_t amount_cents, bool is_debit) {
    struct journal_entry_node *n = calloc(1, sizeof(struct journal_entry_node));
    if (!n) return LEDGER_ERR_NOMEM;
    n->entry.account_id = account_id;
    n->entry.amount_cents = amount_cents;
    n->entry.is_debit = is_debit;
    n->next = tx->entries;
    tx->entries = n;
    if (is_debit)
        tx->total_debits += amount_cents;
    else
        tx->total_credits += amount_cents;
    return LEDGER_OK;
}

ledger_err_t transaction_debit(transaction_t *tx, uint32_t account_id, int64_t amount_cents) {
    if (!tx || tx->committed || tx->aborted) return LEDGER_ERR_INVALID;
    if (amount_cents <= 0) return LEDGER_ERR_INVALID;
    return append_entry(tx, account_id, amount_cents, true);
}

ledger_err_t transaction_credit(transaction_t *tx, uint32_t account_id, int64_t amount_cents) {
    if (!tx || tx->committed || tx->aborted) return LEDGER_ERR_INVALID;
    if (amount_cents <= 0) return LEDGER_ERR_INVALID;
    return append_entry(tx, account_id, amount_cents, false);
}

ledger_err_t transaction_commit(transaction_t *tx) {
    if (!tx || tx->committed || tx->aborted) return LEDGER_ERR_INVALID;
    if (tx->total_debits != tx->total_credits) return LEDGER_ERR_CONSTRAINT;
    for (struct journal_entry_node *n = tx->entries; n; n = n->next) {
        int64_t delta = n->entry.is_debit ? n->entry.amount_cents : -(int64_t)n->entry.amount_cents;
        ledger_err_t err = account_apply_delta(tx->store, n->entry.account_id, delta, tx->tx_id);
        if (err != LEDGER_OK) return err;
    }
    tx->committed = true;
    return LEDGER_OK;
}

void transaction_abort(transaction_t *tx) {
    if (!tx) return;
    tx->aborted = true;
}

void transaction_destroy(transaction_t *tx) {
    if (!tx) return;
    free_entries(tx->entries);
    free(tx);
}

bool transaction_is_committed(const transaction_t *tx) {
    return tx && tx->committed;
}

bool transaction_is_aborted(const transaction_t *tx) {
    return tx && tx->aborted;
}
