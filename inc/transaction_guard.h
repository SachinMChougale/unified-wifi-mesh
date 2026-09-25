#ifndef TRANSACTION_GUARD_H
#define TRANSACTION_GUARD_H

class DatabaseSession;

class TransactionGuard {
public:
    explicit TransactionGuard(DatabaseSession& session);
    ~TransactionGuard();

    TransactionGuard(const TransactionGuard&) = delete;
    TransactionGuard& operator=(const TransactionGuard&) = delete;
    TransactionGuard(TransactionGuard&&) = delete;
    TransactionGuard& operator=(TransactionGuard&&) = delete;

    bool active() const;
    int status() const;
    int commit();

private:
    DatabaseSession *m_session;
    bool m_active;
    int m_status;
};

#endif