# ACID Transaction Ledger System

A production-style ACID-compliant transaction ledger in C with double-entry bookkeeping, atomic commits, and write-ahead logging (WAL) for crash recovery. It supports account creation (checking, savings, investment), deposits, withdrawals, transfers, balance queries, and recovery from WAL after a crash.

## Features

- **Account creation** — Checking, savings, and investment accounts with optional currency (e.g. USD)
- **Deposit / withdrawal** — Double-entry transactions against a reserved cash account
- **Transfers** — Atomic transfer between any two accounts
- **Balance queries** — O(1) balance lookup by account id
- **Atomicity** — Each transaction either fully commits (debit + credit applied) or fully rolls back
- **Write-ahead logging** — All mutations logged before apply; CRC32 checksums for integrity
- **Crash recovery** — On open, WAL is replayed and optional checkpoints restore state without full replay
- **Checkpointing** — Periodic snapshots to limit replay length

## Build and run

### Compile

```bash
make
```

Builds the `ledger` binary in `build/`.

### Run

```bash
make run
# or
./build/ledger [path_to_wal]
```

Default WAL path is `ledger.wal` in the current directory.

### Test

```bash
make test
```

## Example usage

```
> create checking USD
Created account 1
> deposit 1 10000
Deposited 10000 cents
> balance 1
Balance: 10000 cents
> create savings USD
Created account 2
> transfer 1 2 3000
Transferred 3000 cents
> balance 1
Balance: 7000 cents
> balance 2
Balance: 3000 cents
> withdraw 1 2000
Withdrew 2000 cents
> quit
```

## Dependencies

- C99 compiler (e.g. GCC, Clang)
- Standard library only; no third-party dependencies

## Folder structure

```
ACID/
├── include/
│   ├── common.h
│   ├── account.h
│   ├── wal.h
│   ├── transaction.h
│   └── ledger.h
├── src/
│   ├── common.c
│   ├── account.c
│   ├── wal.c
│   ├── transaction.c
│   ├── ledger.c
│   └── main.c
├── tests/
│   └── test_ledger.c
├── build/
├── Makefile
└── README.md
```

## Design notes

- **Double-entry** — Every transaction records matched debits and credits; total debits must equal total credits before commit.
- **WAL** — Log records (begin tx, debit, credit, commit/abort, checkpoint) are appended with CRC32; replay verifies checksums and reapplies committed operations.
- **Checkpoints** — Snapshot of account store and next transaction id is written to the log; recovery can load the latest checkpoint then replay only subsequent records.
