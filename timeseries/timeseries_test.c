#include "timeseries.h"

char *fmt = DEFAULT_TIMEFMT;

cJSON *ts_object(char *field, char *agg) {
    cJSON *ts = cJSON_CreateObject();
    cJSON_AddStringToObject(ts, "field", field);
    cJSON_AddStringToObject(ts, "aggregation", agg);
    return ts;
}

cJSON *testConfBase(cJSON *conf, char *avg, char *interval) {
    if (conf)
        cJSON_Delete(conf);

    const char *key_fields[] = {"userId", "accountId"};
    const char *ts_fields[] = {"pagesVisited", "storageUsed", "trafficUsed"};
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "interval", interval);
    cJSON_AddStringToObject(root, "timeformat", fmt);
    cJSON_AddItemToObject(root, "key_fields", cJSON_CreateStringArray(key_fields, 2));
    cJSON_AddItemToObject(root, "ts_fields", cJSON_CreateStringArray(ts_fields, 3));

    return root;
}

cJSON *testConf(cJSON *conf) {
    return testConfBase(conf, AVG, DAY);
}

cJSON *testConfInvalidInterval(cJSON *conf) {
    return testConfBase(conf, DAY, "xxx");
}

cJSON *dataJson(double s, double a) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "userId", "userId1");
    cJSON_AddStringToObject(data, "accountId", "accountId1");
    cJSON_AddNumberToObject(data, "pagesVisited", s);
    cJSON_AddNumberToObject(data, "storageUsed", 111);
    cJSON_AddNumberToObject(data, "trafficUsed", a);
    return data;
}

int testTSApi(RedisModuleCtx *ctx) {
    long count;
    double val;
    char *eptr;

    RedisModuleCallReply *r = NULL;

    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "TS.CREATE", "ccc", "tstestapi", "day", "2016:01:01 00:00:00"));

    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "TS.INSERT", "ccc", "tstestapi", "10.5", "2016:01:02 00:00:00"));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "TS.INSERT", "ccc", "tstestapi", "11.5", "2016:01:02 00:01:00"));

    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "TS.GET", "ccc", "tstestapi", "sum", "2016:01:02 00:01:00"));
    count = strtol(RedisModule_CallReplyStringPtr(RedisModule_CallReplyArrayElement(r, 0), NULL), &eptr, 10);
    RMUtil_Assert(count == 22);

    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "TS.GET", "ccc", "tstestapi", "avg", "2016:01:02 00:01:00"));
    val = strtod(RedisModule_CallReplyStringPtr(RedisModule_CallReplyArrayElement(r, 0), NULL), &eptr);
    RMUtil_Assert(val == 11);

    return 0;
}

int testTSAggData(RedisModuleCtx *ctx) {
    long timestamp = interval_timestamp(DAY, NULL, NULL);
    char timestamp_key[100], count_key[100];
    char *aggdatakey = "aggdata";

    sprintf(timestamp_key, "%li", timestamp);
    sprintf(count_key, "%s:count", timestamp_key);

    RedisModuleCallReply *r = NULL;
    cJSON *confJson = NULL, *data1 = dataJson(10.5, 10), *data2 = dataJson(2.5, 20);

    // Remove old data (previous tests)
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "DEL", "c", aggdatakey));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "DEL", "c", "ts.doc:userId1:accountId1:storageUsed:sum"));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "DEL", "c", "ts.doc:userId1:accountId1:trafficUsed:avg"));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "DEL", "c", "ts.doc:userId1:accountId1:pagesVisited:sum"));
    // Validate interval values
    confJson = testConfInvalidInterval(confJson);
    RMCALL(r, RedisModule_Call(ctx, "ts.createdoc", "cc", aggdatakey, cJSON_Print_static(confJson)));
    RMUtil_Assert(RedisModule_CallReplyType(r) == REDISMODULE_REPLY_ERROR);
    RMUtil_Assert(!strcmp(RedisModule_CallReplyStringPtr(r, NULL),
            "Invalid json: interval is not one of: second, minute, hour, day, month, year\r\n"));
    // Validate add before conf fails
    RMCALL(r, RedisModule_Call(ctx, "ts.insertdoc", "cc", aggdatakey, cJSON_Print_static(data1)));
    RMUtil_Assert(RedisModule_CallReplyType(r) == REDISMODULE_REPLY_ERROR);
    // Add conf
    confJson = testConf(confJson);
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.createdoc", "cc", aggdatakey, cJSON_Print_static(confJson)));
    // 1st Add succeed
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.insertdoc", "cc", aggdatakey, cJSON_Print_static(data1)));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.insertdoc", "cc", aggdatakey, cJSON_Print_static(data2)));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.insertdoc", "cc", aggdatakey, cJSON_Print_static(data1)));
    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.insertdoc", "cc", aggdatakey, cJSON_Print_static(data1)));
    // Verify count is 1
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:pagesVisited:sum", count_key));
    //    count = strtol(RedisModule_CallReplyStringPtr(r, NULL), &eptr, 10);
    //    RMUtil_Assert(count == 1);
    //
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:trafficUsed:avg", count_key));
    //    count = strtol(RedisModule_CallReplyStringPtr(r, NULL), &eptr, 10);
    //    RMUtil_Assert(count == 1);

    // Verify value
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:pagesVisited:sum", timestamp_key));
    //    val = strtod(RedisModule_CallReplyStringPtr(r, NULL), &eptr);
    //    RMUtil_Assert(val == 10.5);
    //
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:trafficUsed:avg", timestamp_key));
    //    val = strtod(RedisModule_CallReplyStringPtr(r, NULL), &eptr);
    //    RMUtil_Assert(val == 10);

    // 2nd Add
    //RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.add", "cc", "testts", cJSON_Print_static(data2)));

    // Verify count is 2
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:pagesVisited:sum", count_key));
    //    count = strtol(RedisModule_CallReplyStringPtr(r, NULL), &eptr, 10);
    //    RMUtil_Assert(count == 2);
    //
    //    // Verify sum value is aggregated
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:pagesVisited:sum", timestamp_key));
    //    val = strtod(RedisModule_CallReplyStringPtr(r, NULL), &eptr);
    //    RMUtil_Assert(val == 13);
    //
    //    // Verify avg value is aggregated
    //    RMCALL(r, RedisModule_Call(ctx, "HGET", "cc", "ts.agg:userId1:accountId1:trafficUsed:avg", timestamp_key));
    //    val = strtod(RedisModule_CallReplyStringPtr(r, NULL), &eptr);
    //    RMUtil_Assert(val == 15);
    //
    //    // Add with timestamp
    //    cJSON_AddStringToObject(data2, "timestamp", "2016:10:05 06:40:01");
    //    RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.add", "cc", "testts", cJSON_Print_static(data2)));
    //
    //    // Add with wrong timestamp
    //    cJSON_AddStringToObject(data1, "timestamp", "2016-10-05 06:40:01");
    //    //RMCALL_AssertNoErr(r, RedisModule_Call(ctx, "ts.add", "cc", "testts", cJSON_Print_static(data1)));
    //    RMCALL(r, RedisModule_Call(ctx, "ts.add", "cc", "testts", cJSON_Print_static(data1)));
    //    RMUtil_Assert(RedisModule_CallReplyType(r) == REDISMODULE_REPLY_ERROR);
    //    RMUtil_Assert(!strcmp(RedisModule_CallReplyStringPtr(r, NULL),
    //                          "Invalid json: timestamp format and data mismatch\r\n"));

    cJSON_Delete(data1);
    cJSON_Delete(data2);
    cJSON_Delete(confJson);
    RedisModule_FreeCallReply(r);
    return 0;
}

#define EQ(interval, t1, t2) \
        RMUtil_Assert( interval_timestamp(interval, t1, fmt) == interval_timestamp(interval, t2, fmt))

#define NEQ(interval, t1, t2) \
        RMUtil_Assert( interval_timestamp(interval, t1, fmt) != interval_timestamp(interval, t2, fmt))

int testTimeInterval(RedisModuleCtx *ctx) {
    EQ  (SECOND, "2016:11:05 06:40:00.001", "2016:11:05 06:40:00.002");
    NEQ (SECOND, "2016:11:05 06:40:01", "2016:11:05 06:40:02");

    EQ  (MINUTE, "2016:11:05 06:40:01", "2016:11:05 06:40:02");
    NEQ (MINUTE, "2016:11:05 06:41:01", "2016:11:05 06:42:02");

    EQ  (HOUR, "2016:11:05 06:41:01", "2016:11:05 06:42:02");
    NEQ (HOUR, "2016:11:05 07:41:01", "2016:11:05 06:42:02");

    EQ  (DAY, "2016:11:05 07:41:01", "2016:11:05 06:42:02");
    NEQ (DAY, "2016:11:06 07:41:01", "2016:11:05 06:42:02");

    EQ  (MONTH, "2016:11:06 07:41:01", "2016:11:05 06:42:02");
    NEQ (MONTH, "2016:10:06 07:41:01", "2016:11:05 06:42:02");

    EQ  (YEAR, "2016:10:06 07:41:01", "2016:11:05 06:42:02");
    NEQ (YEAR, "2016:10:06 07:41:01", "2015:11:05 06:42:02");

    // Wrong format
    RMUtil_Assert(interval_timestamp(DAY, "2016-10-06 07:41:01", fmt) == 0)

    return 0;
}

#define IDX(interval, t1, t2, idx) RMUtil_Assert( idx_timestamp( \
        interval_timestamp(interval, t1, fmt), interval_timestamp(interval, t2, fmt), str2interval(interval)) == idx)

#define IDXGT(interval, t1, t2, idx) RMUtil_Assert( idx_timestamp( \
        interval_timestamp(interval, t1, fmt), interval_timestamp(interval, t2, fmt), str2interval(interval)) > idx)

int testTimestampIdx(RedisModuleCtx *ctx) {

    IDX (SECOND, "2016:11:05 06:40:00.001", "2016:11:05 06:40:00.002", 0);

    IDX (SECOND, "2016:11:05 06:40:01", "2016:11:05 06:40:02", 1);
    IDX (SECOND, "2016:11:05 06:40:01", "2016:11:05 06:40:03", 2);
    IDX (SECOND, "2016:11:05 06:40:01", "2016:11:06 06:40:03", 86402);

    IDX (MINUTE, "2016:11:05 06:40:01", "2016:11:05 06:40:02", 0);
    IDX (MINUTE, "2016:11:05 06:41:01", "2016:11:05 06:42:02", 1);

    IDX (HOUR, "2016:11:05 06:41:01", "2016:11:05 06:42:02", 0);
    IDX (HOUR, "2016:11:05 07:41:01", "2016:11:05 08:42:02", 1);

    IDX (DAY, "2016:11:05 07:41:01", "2016:11:05 06:42:02", 0);
    IDX (DAY, "2016:11:06 07:41:01", "2016:11:07 06:42:02", 1);
    IDX (DAY, "2016:11:06 07:41:01", "2016:11:07 06:42:02", 1);

    IDX (MONTH, "2016:11:06 07:41:01", "2016:11:05 06:42:02", 0);
    IDX (MONTH, "2016:10:06 07:41:01", "2016:11:05 06:42:02", 1);

    IDX (YEAR, "2016:10:06 07:41:01", "2016:11:05 06:42:02", 0);
    IDX (YEAR, "2015:10:06 07:41:01", "2016:11:05 06:42:02", 1);

    IDX (YEAR, "2016:10:06 07:41:01", "2015:11:05 06:42:02", -1);

    IDXGT (YEAR, "2016:10:06 07:41:01", "2015:11:05 06:42:02", TS_MAX_ENTRIES);

    return 0;
}

// Unit test entry point for the timeseries module
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx);

    RMUtil_Test(testTSApi);

    RMUtil_Test(testTimeInterval);

    RMUtil_Test(testTimestampIdx);

    RMUtil_Test(testTSAggData);

    RedisModule_ReplyWithSimpleString(ctx, "PASS");
    return REDISMODULE_OK;
}
