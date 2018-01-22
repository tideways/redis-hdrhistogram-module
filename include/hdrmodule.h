#ifndef HDRMODULE_H
#define HDRMODULE_H

#include "redismodule.h"

extern int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif /* HDRMODULE_H */
