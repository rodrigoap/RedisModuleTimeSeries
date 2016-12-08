#ifndef _TIMESERIES_H_
#define _TIMESERIES_H_

#define _XOPEN_SOURCE // For the use of strptime
#include <time.h>

#include <strings.h>

#include "../redismodule.h"
#include "../rmutil/util.h"
#include "../rmutil/strings.h"
#include "../rmutil/test_util.h"
#include "../cJSON/cJSON.h"

#define SECOND "second"
#define MINUTE "minute"
#define HOUR "hour"
#define DAY "day"
#define MONTH "month"
#define YEAR "year"

#define SUM "sum"
#define AVG "avg"
#define COUNT "count"

#define DEFAULT_TIMEFMT "%Y:%m:%d %H:%M:%S"

typedef enum {
  none = 0,
  second = 1,
  minute = 60,
  hour = 3600,
  day = 86400,
  month = 2628000,
  year = 31536000
} Interval;

#define TS_MAX_ENTRIES 1000000

#define RMCALL(reply, call) \
  if (reply) \
    RedisModule_FreeCallReply(reply); \
  reply = call

#define RMCALL_Assert(reply, call, expr) \
  RMCALL(reply, call); \
  RMUtil_Assert(expr);

#define RMCALL_AssertNoErr(reply, call) RMCALL_Assert(reply, call, RedisModule_CallReplyType(r) != REDISMODULE_REPLY_ERROR)

#define VALIDATE_STRING_TYPE(k) \
  if (k->type != cJSON_String) \
    return "Invalid json: key is not a string"; \
  if (!strlen(k->valuestring)) \
    return "Invalid json: empty string is not allowed";

#define VALIDATE_KEY(j, key) \
  cJSON_GetObjectItem(j, #key); \
  if (!key) \
    return "Invalid json: missing " #key;

#define VALIDATE(j, key, Type) \
  VALIDATE_KEY(j, key) \
  if (key->type != Type) \
    return "Invalid json: " #key  "is not " #Type;

#define VALIDATE_STRING(j, key) \
  VALIDATE_KEY(j, key) \
  VALIDATE_STRING_TYPE(key)

#define VALIDATE_ARRAY(j, key) \
  VALIDATE(j, key, cJSON_Array) \
  sz = cJSON_GetArraySize(key); \
  if (!sz) \
    return "Invalid json: " #key " is empty";

#define VALIDATE_ENUM(js, key, validValues, msg) \
  VALIDATE(js, key, cJSON_String) \
  valid = 0; \
  esz = sizeof(validValues) / sizeof(validValues[0]); \
  for (j=0; !valid && j < esz; j++) \
    valid = strcmp( validValues[j], key->valuestring) == 0; \
  if (!valid) \
    return "Invalid json: " #key " is not one of: " msg;

#define RMUTIL_ASSERT_NONULL(r, entry, cleanup) RMUTIL_ASSERT_NOERROR(r, cleanup) \
    else if (RedisModule_CallReplyType(r) == REDISMODULE_REPLY_NULL) { \
        char msg[1000] = "No such entry: "; \
        cleanup(); \
        return RedisModule_ReplyWithError(ctx, strcat(msg, entry)); \
    }

#define TS_CLEANUP(data, conf, confRep) \
    cJSON_Delete(data); \
    cJSON_Delete(conf); \
    RedisModule_FreeCallReply(confRep); \


time_t interval_timestamp(const char *interval, const char *timestamp, const char *format);

size_t idx_timestamp(time_t init_timestamp, size_t cur_timestamp, Interval interval);

Interval str2interval(const char *interval);

const char *interval2str(Interval interval);

// Test function
int TestModule(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif
