#include "account.h"
#include <stdlib.h>
#include <string.h>

struct account_slot {
    bool in_use;
    account_t account;
};

struct account_store {
    struct account_slot *slots;
    uint32_t capacity;
    uint32_t next_id;
    uint32_t count;
};

account_store_t *account_store_create(void) {
    account_store_t *s = calloc(1, sizeof(account_store_t));
    if (!s) return NULL;
    s->capacity = 4096;
    s->slots = calloc((size_t)s->capacity, sizeof(struct account_slot));
    if (!s->slots) {
        free(s);
        return NULL;
    }
    return s;
}

void account_store_destroy(account_store_t *s) {
    if (!s) return;
    free(s->slots);
    free(s);
}

static ledger_err_t grow(account_store_t *s) {
    if (s->capacity >= MAX_ACCOUNTS) return LEDGER_ERR_NOMEM;
    uint32_t new_cap = s->capacity * 2;
    if (new_cap > MAX_ACCOUNTS) new_cap = MAX_ACCOUNTS;
    struct account_slot *n = realloc(s->slots, (size_t)new_cap * sizeof(struct account_slot));
    if (!n) return LEDGER_ERR_NOMEM;
    memset(n + s->capacity, 0, (size_t)(new_cap - s->capacity) * sizeof(struct account_slot));
    s->slots = n;
    s->capacity = new_cap;
    return LEDGER_OK;
}

ledger_err_t account_create_with_id(account_store_t *s, uint32_t id, account_type_t type, const char *currency) {
    if (!s) return LEDGER_ERR_INVALID;
    if (s->count >= s->capacity && grow(s) != LEDGER_OK) return LEDGER_ERR_NOMEM;
    uint32_t idx = id % s->capacity;
    while (s->slots[idx].in_use) {
        idx = (idx + 1) % s->capacity;
        if (idx == (id % s->capacity)) return LEDGER_ERR_NOMEM;
    }
    s->slots[idx].in_use = true;
    s->slots[idx].account.id = id;
    s->slots[idx].account.type = type;
    s->slots[idx].account.balance_cents = 0;
    s->slots[idx].account.version = 0;
    memset(s->slots[idx].account.currency, 0, CURRENCY_LEN);
    if (currency)
        strncpy(s->slots[idx].account.currency, currency, CURRENCY_LEN - 1);
    s->count++;
    if (s->next_id <= id) s->next_id = id + 1;
    return LEDGER_OK;
}

ledger_err_t account_create(account_store_t *s, account_type_t type, const char *currency, uint32_t *out_id) {
    if (!s || !out_id) return LEDGER_ERR_INVALID;
    if (s->count >= s->capacity && grow(s) != LEDGER_OK) return LEDGER_ERR_NOMEM;
    uint32_t id = s->next_id++;
    uint32_t idx = id % s->capacity;
    while (s->slots[idx].in_use) {
        idx = (idx + 1) % s->capacity;
        if (idx == (id % s->capacity)) return LEDGER_ERR_NOMEM;
    }
    s->slots[idx].in_use = true;
    s->slots[idx].account.id = id;
    s->slots[idx].account.type = type;
    s->slots[idx].account.balance_cents = 0;
    s->slots[idx].account.version = 0;
    memset(s->slots[idx].account.currency, 0, CURRENCY_LEN);
    if (currency)
        strncpy(s->slots[idx].account.currency, currency, CURRENCY_LEN - 1);
    s->count++;
    *out_id = id;
    return LEDGER_OK;
}

static int slot_index(const account_store_t *s, uint32_t id) {
    uint32_t idx = id % s->capacity;
    for (uint32_t n = 0; n < s->capacity; n++) {
        if (!s->slots[idx].in_use) return -1;
        if (s->slots[idx].account.id == id) return (int)idx;
        idx = (idx + 1) % s->capacity;
    }
    return -1;
}

ledger_err_t account_get(account_store_t *s, uint32_t id, account_t *out) {
    if (!s || !out) return LEDGER_ERR_INVALID;
    int idx = slot_index(s, id);
    if (idx < 0) return LEDGER_ERR_NOTFOUND;
    *out = s->slots[(uint32_t)idx].account;
    return LEDGER_OK;
}

ledger_err_t account_apply_delta(account_store_t *s, uint32_t id, int64_t delta_cents, uint64_t version) {
    if (!s) return LEDGER_ERR_INVALID;
    int idx = slot_index(s, id);
    if (idx < 0) return LEDGER_ERR_NOTFOUND;
    struct account_slot *slot = &s->slots[(uint32_t)idx];
    int64_t new_bal = slot->account.balance_cents + delta_cents;
    if (new_bal < 0 && id != 0) return LEDGER_ERR_CONSTRAINT;
    slot->account.balance_cents = new_bal;
    slot->account.version = version;
    return LEDGER_OK;
}

ledger_err_t account_set_balance(account_store_t *s, uint32_t id, int64_t balance_cents, uint64_t version) {
    if (!s) return LEDGER_ERR_INVALID;
    if (balance_cents < 0) return LEDGER_ERR_CONSTRAINT;
    int idx = slot_index(s, id);
    if (idx < 0) return LEDGER_ERR_NOTFOUND;
    s->slots[(uint32_t)idx].account.balance_cents = balance_cents;
    s->slots[(uint32_t)idx].account.version = version;
    return LEDGER_OK;
}

uint32_t account_count(const account_store_t *s) {
    return s ? s->count : 0;
}

#define SNAPSHOT_ENTRY_SIZE (sizeof(uint32_t) + sizeof(uint8_t) + sizeof(int64_t) + sizeof(uint64_t) + CURRENCY_LEN)

ledger_err_t account_serialize(const account_store_t *s, uint32_t next_tx_id, void *buf, size_t cap, size_t *out_len) {
    if (!s || !buf || !out_len) return LEDGER_ERR_INVALID;
    if (cap < 8) return LEDGER_ERR_INVALID;
    uint8_t *p = (uint8_t *)buf;
    uint32_t count = account_count(s);
    memcpy(p, &next_tx_id, 4);
    memcpy(p + 4, &count, 4);
    p += 8;
    size_t used = 8;
    for (uint32_t i = 0; i < s->capacity && count > 0; i++) {
        if (!s->slots[i].in_use) continue;
        if (used + SNAPSHOT_ENTRY_SIZE > cap) return LEDGER_ERR_INVALID;
        account_t *a = &s->slots[i].account;
        memcpy(p, &a->id, 4);
        memcpy(p + 4, &a->type, 1);
        memcpy(p + 8, &a->balance_cents, 8);
        memcpy(p + 16, &a->version, 8);
        memcpy(p + 24, a->currency, CURRENCY_LEN);
        p += SNAPSHOT_ENTRY_SIZE;
        used += SNAPSHOT_ENTRY_SIZE;
        count--;
    }
    *out_len = used;
    return LEDGER_OK;
}
