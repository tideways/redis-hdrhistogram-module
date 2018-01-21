# Redis HDRHistogram Module

Redis Type and corresponding command to handle.

```
HDR.NEW key min max sf 
```

```
HDR.ADD key value [value ...]
```

```
HDR.ADDC key value count
```

```
HDR.IMPORT key bucket_count [bucket_count ...]
```

```
HDR.MERGE dest source [source ...]
```

```
HDR.GET key
```

Return histogram as a Redis map `{bucket: count}`

```
HDR.GETB key
```

Return histogram as a list of buckets
