#include "ledger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAL_PATH "ledger.wal"
#define CMD_MAX 64

static void print_help(void) {
    puts("ACID Ledger - Transaction System");
    puts("  create <type> [currency]  - Create account (checking|savings|investment)");
    puts("  deposit <id> <cents>      - Deposit to account");
    puts("  withdraw <id> <cents>     - Withdraw from account");
    puts("  transfer <from> <to> <cents>");
    puts("  balance <id>              - Query balance");
    puts("  quit                      - Exit");
}

static account_type_t parse_type(const char *s) {
    if (strcmp(s, "checking") == 0) return ACCT_CHECKING;
    if (strcmp(s, "savings") == 0) return ACCT_SAVINGS;
    if (strcmp(s, "investment") == 0) return ACCT_INVESTMENT;
    return ACCT_CHECKING;
}

int main(int argc, char **argv) {
    const char *wal = argc > 1 ? argv[1] : WAL_PATH;
    ledger_t *l = ledger_open(wal);
    if (!l) {
        fprintf(stderr, "Failed to open ledger at %s\n", wal);
        return 1;
    }

    print_help();
    char buf[256];
    while (fputs("> ", stdout), fgets(buf, sizeof(buf), stdin)) {
        char cmd[CMD_MAX];
        int n = sscanf(buf, "%63s", cmd);
        if (n < 1) continue;
        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "q") == 0) break;
        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) { print_help(); continue; }

        if (strcmp(cmd, "create") == 0) {
            char type[32] = "checking", currency[8] = "USD";
            sscanf(buf + 7, "%31s %7s", type, currency);
            uint32_t id;
            ledger_err_t err = ledger_create_account(l, parse_type(type), currency, &id);
            if (err != LEDGER_OK) printf("Error %d\n", err);
            else printf("Created account %u\n", id);
            continue;
        }
        if (strcmp(cmd, "deposit") == 0) {
            uint32_t id; int64_t cents;
            if (sscanf(buf + 8, "%u %lld", &id, (long long *)&cents) < 2) { puts("Usage: deposit <id> <cents>"); continue; }
            ledger_err_t err = ledger_deposit(l, id, cents);
            if (err != LEDGER_OK) printf("Error %d\n", err);
            else printf("Deposited %lld cents\n", (long long)cents);
            continue;
        }
        if (strcmp(cmd, "withdraw") == 0) {
            uint32_t id; int64_t cents;
            if (sscanf(buf + 9, "%u %lld", &id, (long long *)&cents) < 2) { puts("Usage: withdraw <id> <cents>"); continue; }
            ledger_err_t err = ledger_withdraw(l, id, cents);
            if (err != LEDGER_OK) printf("Error %d\n", err);
            else printf("Withdrew %lld cents\n", (long long)cents);
            continue;
        }
        if (strcmp(cmd, "transfer") == 0) {
            uint32_t from, to; int64_t cents;
            if (sscanf(buf + 9, "%u %u %lld", &from, &to, (long long *)&cents) < 3) {
                puts("Usage: transfer <from> <to> <cents>");
                continue;
            }
            ledger_err_t err = ledger_transfer(l, from, to, cents);
            if (err != LEDGER_OK) printf("Error %d\n", err);
            else printf("Transferred %lld cents\n", (long long)cents);
            continue;
        }
        if (strcmp(cmd, "balance") == 0) {
            uint32_t id;
            if (sscanf(buf + 8, "%u", &id) < 1) { puts("Usage: balance <id>"); continue; }
            int64_t bal;
            ledger_err_t err = ledger_balance(l, id, &bal);
            if (err != LEDGER_OK) printf("Error %d\n", err);
            else printf("Balance: %lld cents\n", (long long)bal);
        } else
            printf("Unknown command: %s\n", cmd);
    }

    ledger_close(l);
    return 0;
}
