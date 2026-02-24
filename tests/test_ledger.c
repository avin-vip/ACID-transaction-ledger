#include "ledger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TMP_WAL "test_ledger.wal"

static void test_create_and_balance(void) {
    remove(TMP_WAL);
    ledger_t *l = ledger_open(TMP_WAL);
    assert(l);
    uint32_t id;
    assert(ledger_create_account(l, ACCT_CHECKING, "USD", &id) == LEDGER_OK);
    assert(id > 0);
    int64_t bal;
    assert(ledger_balance(l, id, &bal) == LEDGER_OK);
    assert(bal == 0);
    ledger_close(l);
    remove(TMP_WAL);
    printf("test_create_and_balance: OK\n");
}

static void test_deposit_withdraw(void) {
    remove(TMP_WAL);
    ledger_t *l = ledger_open(TMP_WAL);
    assert(l);
    uint32_t id;
    ledger_create_account(l, ACCT_CHECKING, "USD", &id);
    assert(ledger_deposit(l, id, 10000) == LEDGER_OK);
    int64_t bal;
    assert(ledger_balance(l, id, &bal) == LEDGER_OK);
    assert(bal == 10000);
    assert(ledger_withdraw(l, id, 3000) == LEDGER_OK);
    assert(ledger_balance(l, id, &bal) == LEDGER_OK);
    assert(bal == 7000);
    assert(ledger_withdraw(l, id, 8000) != LEDGER_OK);
    ledger_close(l);
    remove(TMP_WAL);
    printf("test_deposit_withdraw: OK\n");
}

static void test_transfer(void) {
    remove(TMP_WAL);
    ledger_t *l = ledger_open(TMP_WAL);
    assert(l);
    uint32_t id1, id2;
    ledger_create_account(l, ACCT_CHECKING, "USD", &id1);
    ledger_create_account(l, ACCT_SAVINGS, "USD", &id2);
    ledger_deposit(l, id1, 50000);
    assert(ledger_transfer(l, id1, id2, 20000) == LEDGER_OK);
    int64_t b1, b2;
    ledger_balance(l, id1, &b1);
    ledger_balance(l, id2, &b2);
    assert(b1 == 30000 && b2 == 20000);
    ledger_close(l);
    remove(TMP_WAL);
    printf("test_transfer: OK\n");
}

static void test_wal_recovery(void) {
    remove(TMP_WAL);
    ledger_t *l = ledger_open(TMP_WAL);
    assert(l);
    uint32_t id;
    ledger_create_account(l, ACCT_CHECKING, "USD", &id);
    ledger_deposit(l, id, 12345);
    int64_t bal;
    ledger_balance(l, id, &bal);
    assert(bal == 12345);
    ledger_close(l);

    l = ledger_open(TMP_WAL);
    assert(l);
    assert(ledger_balance(l, id, &bal) == LEDGER_OK);
    assert(bal == 12345);
    ledger_close(l);
    remove(TMP_WAL);
    printf("test_wal_recovery: OK\n");
}

int main(void) {
    test_create_and_balance();
    test_deposit_withdraw();
    test_transfer();
    test_wal_recovery();
    printf("All tests passed.\n");
    return 0;
}
