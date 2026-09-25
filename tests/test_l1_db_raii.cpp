#include <cstdio>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "database_session.h"
#include "query_result.h"
#include "statement.h"
#include "transaction_guard.h"

namespace {

const char *test_database_path = "/tmp/unified-wifi-mesh-raii-test.db";

class DatabaseRaiiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        std::remove(test_database_path);
    }

    void TearDown() override
    {
        std::remove(test_database_path);
    }
};

} // namespace

TEST_F(DatabaseRaiiTest, SessionOwnsConnectionAndResult)
{
    DatabaseSession db(test_database_path);
    ASSERT_TRUE(db.valid());

    QueryResult create = db.execute("CREATE TABLE values_table (value INTEGER);");
    EXPECT_FALSE(create.valid());

    QueryResult insert = db.execute("INSERT INTO values_table VALUES (7);");
    EXPECT_FALSE(insert.valid());

    QueryResult result = db.execute("SELECT value FROM values_table;");
    ASSERT_TRUE(result.valid());
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.number(1), 7);
    EXPECT_FALSE(result.next());
}

TEST_F(DatabaseRaiiTest, QueryResultMoveTransfersOwnership)
{
    DatabaseSession db(test_database_path);
    ASSERT_TRUE(db.valid());
    db.execute("CREATE TABLE values_table (value TEXT);");
    db.execute("INSERT INTO values_table VALUES ('owned');");

    QueryResult first = db.execute("SELECT value FROM values_table;");
    QueryResult second(std::move(first));

    EXPECT_FALSE(first.valid());
    ASSERT_TRUE(second.next());
    std::string value;
    ASSERT_TRUE(second.text(1, value));
    EXPECT_EQ(value, "owned");
}

TEST_F(DatabaseRaiiTest, StatementFinalizesAndSupportsMove)
{
    DatabaseSession db(test_database_path);
    ASSERT_TRUE(db.valid());

    Statement create(db, "CREATE TABLE values_table (value INTEGER);");
    ASSERT_TRUE(create.valid());
    EXPECT_EQ(create.step(), SQLITE_DONE);

    Statement first(db, "INSERT INTO values_table VALUES (11);");
    ASSERT_TRUE(first.valid());
    Statement second(std::move(first));
    EXPECT_FALSE(first.valid());
    EXPECT_EQ(second.step(), SQLITE_DONE);
}

TEST_F(DatabaseRaiiTest, TransactionRollsBackWhenNotCommitted)
{
    DatabaseSession db(test_database_path);
    ASSERT_TRUE(db.valid());
    db.execute("CREATE TABLE values_table (value INTEGER);");

    {
        TransactionGuard transaction(db);
        ASSERT_TRUE(transaction.active());
        EXPECT_EQ(transaction.status(), 0);
        db.execute("INSERT INTO values_table VALUES (19);");
    }

    QueryResult result = db.execute("SELECT COUNT(*) FROM values_table;");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.number(1), 0);
}

TEST_F(DatabaseRaiiTest, TransactionCommitDisablesRollback)
{
    DatabaseSession db(test_database_path);
    ASSERT_TRUE(db.valid());
    db.execute("CREATE TABLE values_table (value INTEGER);");

    TransactionGuard transaction(db);
    ASSERT_TRUE(transaction.active());
    db.execute("INSERT INTO values_table VALUES (23);");
    EXPECT_EQ(transaction.commit(), 0);
    EXPECT_FALSE(transaction.active());

    QueryResult result = db.execute("SELECT COUNT(*) FROM values_table;");
    ASSERT_TRUE(result.next());
    EXPECT_EQ(result.number(1), 1);
}

TEST(DatabaseRaiiUninitializedTest, TransactionGuardReportsFailure)
{
    DatabaseSession db("/proc/unified-wifi-mesh.db");
    TransactionGuard transaction(db);
    EXPECT_FALSE(transaction.active());
    EXPECT_NE(transaction.status(), 0);
}