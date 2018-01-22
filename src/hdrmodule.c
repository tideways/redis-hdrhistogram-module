#include <limits.h>

#include "hdrmodule.h"

#include "hdr/hdr_histogram.h"
#include "hdr/hdr_histogram_log.h"

#include <stdlib.h>

static RedisModuleType *HdrType;

#define TYPE_NAME "hdr-be-00"
#define ENCODING_VER 0

int HdrNew_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 5) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);

    if (type != REDISMODULE_KEYTYPE_EMPTY) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    long long lowest_trackable_value, highest_trackable_value, significant_figures;

    if ((RedisModule_StringToLongLong(argv[2], &lowest_trackable_value) != REDISMODULE_OK)
            || lowest_trackable_value <= 0 || lowest_trackable_value > INT_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid lowest_trackable_value: must be a positive 32 bit integer");
    }

    if ((RedisModule_StringToLongLong(argv[3], &highest_trackable_value) != REDISMODULE_OK)
            || highest_trackable_value <= 0 || highest_trackable_value > INT_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid highest_trackable_value: must be a positive 32 bit integer");
    }

    if ((RedisModule_StringToLongLong(argv[4], &significant_figures) != REDISMODULE_OK)
            || significant_figures <= 0 || significant_figures > INT_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid significant_figures: must be a positive 32 bit integer");
    }

    struct hdr_histogram *hdr;
    int res = hdr_init(lowest_trackable_value, highest_trackable_value, significant_figures, &hdr);

    if (res != 0) {
        return RedisModule_ReplyWithError(ctx, "ERR failed creating hdrhistogram");
    }

    RedisModule_ModuleTypeSetValue(key, HdrType, hdr);

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

int HdrAddCount_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ | REDISMODULE_WRITE);
    int type = RedisModule_KeyType(key);
    if (RedisModule_ModuleTypeGetType(key) != HdrType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    long long value, count;

    if ((RedisModule_StringToLongLong(argv[2], &value) != REDISMODULE_OK)
            || value <= 0 || value > INT_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid value: must be a positive 32 bit integer");
    }

    if ((RedisModule_StringToLongLong(argv[3], &count) != REDISMODULE_OK)
            || count <= 0 || count > INT_MAX) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid count: must be a positive 32 bit integer");
    }

    struct hdr_histogram *hdr = RedisModule_ModuleTypeGetValue(key);

    if (hdr_record_values(hdr, value, count) == 0) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to add value to hdrhistogram");
    }

    RedisModule_ReplyWithSimpleString(ctx, "OK");
    RedisModule_ReplicateVerbatim(ctx);

    return REDISMODULE_OK;
}

int HdrPercentile_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (RedisModule_ModuleTypeGetType(key) != HdrType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    double *percentiles = RedisModule_PoolAlloc(ctx, sizeof(double) * argc - 2);
    int i;
    for (i = 2; i < argc; i++)
    {
        double percentile;
        if ((RedisModule_StringToDouble(argv[i], &percentile) != REDISMODULE_OK)
                || percentile < 0 || percentile > 100) {
            return RedisModule_ReplyWithError(ctx,
                    "ERR invalid percentile: must be a double between 0..100");
        }

        percentiles[i - 2] = percentile;
    }

    struct hdr_histogram *hdr = RedisModule_ModuleTypeGetValue(key);

    RedisModule_ReplyWithArray(ctx, argc - 2);
    for (i = 0; i < argc - 2; i++) {
        RedisModule_ReplyWithLongLong(ctx, hdr_value_at_percentile(hdr, percentiles[i]));
    }

    return REDISMODULE_OK;
}

int HdrDebug_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    RedisModule_AutoMemory(ctx); /* Use automatic memory management. */

    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    RedisModuleKey *key = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
    int type = RedisModule_KeyType(key);
    if (RedisModule_ModuleTypeGetType(key) != HdrType) {
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
    }

    struct hdr_histogram *hdr = RedisModule_ModuleTypeGetValue(key);

    struct hdr_iter *iterator;
    iterator = malloc(sizeof(struct hdr_iter));

    hdr_iter_init(iterator, hdr);

    int size = 1;
    while (hdr_iter_next(iterator)) {
        size++;
    }

    RedisModule_ReplyWithArray(ctx, size);

    char buf[1024];
    sprintf(buf, "HDR (%d, %d, %d, %d) 95p: %d, 50p: %d",
            (int)hdr->lowest_trackable_value,
            (int)hdr->highest_trackable_value,
            hdr->significant_figures,
            (int)hdr->total_count,
            (int)hdr_value_at_percentile(hdr, 0.95),
            (int)hdr_value_at_percentile(hdr, 0.5)
        );
    RedisModule_ReplyWithSimpleString(ctx, buf);

    hdr_iter_init(iterator, hdr);

    while (hdr_iter_next(iterator)) {
        sprintf(buf, " BUCKET %d: %d", (int)iterator->value, (int)iterator->count);
        RedisModule_ReplyWithSimpleString(ctx, buf);
    }

    return REDISMODULE_OK;
}

static void *HdrTypeRdbLoad(RedisModuleIO *rdb, int encver) {
    if (encver != ENCODING_VER) {
        return NULL;
    }

    struct hdr_histogram *hdr;
    uint64_t lowest_trackable_value = RedisModule_LoadUnsigned(rdb);
    uint64_t highest_trackable_value = RedisModule_LoadUnsigned(rdb);
    uint64_t significant_figures = RedisModule_LoadUnsigned(rdb);

    int res = hdr_init(lowest_trackable_value, highest_trackable_value, significant_figures, &hdr);

    if (res != 0) {
        return NULL;
    }

    int assigned = 0;
    int i = 0;
    while (i < hdr->counts_len) {
        int64_t value = RedisModule_LoadSigned(rdb);

        if (value == 0) {
            // 0 is a magic number that marks the end of the snapshot
            break;
        } else if (value < 0) {
            // negative values measure the number of empty buckets, skip them
            // since they are already set to 0 in hdr_init()
            i = i + (value * -1);
        } else {
            hdr->counts[i] = value;
            i++;
        }
    }

    hdr_reset_internal_counters(hdr);
    hdr->normalizing_index_offset = 0;
    hdr->conversion_ratio = 1.0;

    return hdr;
}

static void HdrTypeRdbSave(RedisModuleIO *rdb, void *value) {
    struct hdr_histogram *hdr = value;

    RedisModule_SaveUnsigned(rdb, hdr->lowest_trackable_value);
    RedisModule_SaveUnsigned(rdb, hdr->highest_trackable_value);
    RedisModule_SaveUnsigned(rdb, hdr->significant_figures);

    int i, skip = 0, found = 0;

    for (i = 0; i < hdr->counts_len; i++) {
        if (found >= hdr->total_count) {
            break;
        }

        if (hdr->counts[i] == 0) {
            skip--;
        } else {
            if (skip < 0) {
                RedisModule_SaveSigned(rdb, skip);
                skip = 0;
            }
            RedisModule_SaveSigned(rdb, hdr->counts[i]);
            found += hdr->counts[i];
        }
    }
    RedisModule_SaveSigned(rdb, 0);
}

static void HdrTypeAofRewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    struct hdr_histogram *hdr = value;

    RedisModule_EmitAOF(
        aof,
        "HDR.NEW",
        "%s %ll",
        key,
        hdr->lowest_trackable_value,
        hdr->highest_trackable_value,
        hdr->significant_figures
    );
}

static size_t HdrTypeMemUsage(const void *value) {
    return 0;
}

static void HdrTypeFree(void *value) {
    free(value);
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx, "hdr", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods tm = { 
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = HdrTypeRdbLoad,
        .rdb_save = HdrTypeRdbSave,
        .aof_rewrite = HdrTypeAofRewrite,
        .mem_usage = HdrTypeMemUsage,
        .free = HdrTypeFree };

    HdrType = RedisModule_CreateDataType(ctx, TYPE_NAME, ENCODING_VER, &tm);

    if (HdrType == NULL) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "hdr.new",
             HdrNew_RedisCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "hdr.addc",
             HdrAddCount_RedisCommand, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "hdr.percentile",
             HdrPercentile_RedisCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "hdr.debug",
             HdrDebug_RedisCommand, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    return REDISMODULE_OK;
}
