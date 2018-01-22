# Redis HDRHistogram Module

HdrHistogram Redis Type and corresponding commands based on the
[HdrHistogram_C](https://github.com/HdrHistogram/HdrHistogram_c) by [Michael
Barker](https://github.com/mikeb01).

This Redis Module is Work in Progress / Prototype. Not all documented commands
are implemented yet. The module is not used in production and currently under
development to serve as a potential replacement for our current Elasticsearch
based storage.

## Todos

- Fix rdb unserialize read problem, probably bug in code.
- Implement `HdrTypeAofRewrite` correctly
- Implement missing commands.
- Add installation documentation

## Commands

```
HDR.NEW key min max sf 
```

Create a new HDR histogram with given key as name, a range between min and max
with significant figures 1, 2 or 3.

```
HDR.ADD key value [value ...]
```

Record multiple different values into the histogram at given key.


```
HDR.ADDC key value count
```

Record multiple counts of the same value into the histogram at given key.


```
HDR.IMPORT key lo hi sf bucket_count [bucket_count ...]
```

Import a serialized histogram from outside of Redis into the key, either
creating a new histogram with lo, hi, sf or merging into an existing one.
This function is useful if you are collecting the histogram data outside
of Redis and import it for storage.


```
HDR.MERGE dest source [source ...]
```

Merge one or more histograms together into a new or existing destination key.
If the destination does not exist, lo, hi and sf from the first histogram in the
source list are used for the initialization.

```
HDR.PERCENTILE key percentile [percentile ...]
```

Get the value of a queried percentile (or more than one) for the given
histogram.  The percentile values are doubles in the range from 0.0 to 100.0
This command allows you answer the question which latency is the 50, 95, 99s or
any other percentile?


```
HDR.CDF key value [value ...]
```

Calculate the cummulative distribution function's value for given bucket
values. This command allows you to answer the question at which percentile is
the latency $x?

```
HDR.GET key [key ...]
```

Return histogram as a Redis list with pairs of values for bucket and number of
entries. Merge histograms if multiple are queried.

```
HDR.GETB key
```

Return histogram as a list of buckets, starting with 3 metadata values for hi, lo and sf.
