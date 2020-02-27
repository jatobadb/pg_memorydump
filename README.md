# pg_contextdump
 
The extension is compatible with PostgreSQL 11.5. On the newer versions needs testing.
## Overview

All the sizes are in bytes.

| Column | Description |
| --------- |------|
| contextname | name of the context |
| contexttype | type of the context (M - T_MemoryContext, A - T_AllocSetContext, T - T_SlabContext, O - Other types (T_GenerationContext since 11.* versions))|
| id | unique autoincrement id (not a real part of the context source structure) |
| parent_id | all contexts are organized in hierarchical tree structure. This is the id of contexts parent|
| initBlockSize | every new created context has its own initial size of block, which is used to determine the size of blocks in the context. It will be multiplied by 2 each time the size of new allocated chunk will exceed allocchunklimit |
| nblocks | current count of blocks in the context |
| totalsize | total size of the context |
| freespace | total size of avaliable to allocate memory in the context |
| count |  i-value shows how many chunks in the context have the size between i and (i+1) tenth proportions of ( chunk size / allocchunklimit ). The last one shows count of chunks, which exceed allocchunklimit  |


For example:
```plpgsql
postgres=# SELECT * FROM pg_contextdump ORDER BY parent_id ASC; 
       contextname        | contexttype | id  | parent_id | initblocksize | maxblocksize | allocchunklimit | nblocks | totalsize | freespace |       histogramm        
--------------------------+-------------+-----+-----------+---------------+--------------+-----------------+---------+-----------+-----------+-------------------------
 TopMemoryContext         | A           |   1 |         0 |          8192 |      8388608 |            8192 |       6 |     79280 |      9992 | {0,0,0,0,0,0,0,0,0,1,5}
 RowDescriptionContext    | A           |   6 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |      6896 | {0,0,0,0,0,0,0,0,0,1,0}
 dynahash                 | A           | 108 |         1 |          8192 |      8388608 |            8192 |       2 |    103904 |      2624 | {0,0,0,0,0,0,0,0,0,1,1}
 dynahash                 | A           |   9 |         1 |          8192 |      8388608 |            8192 |       2 |     16168 |      4040 | {0,0,0,0,0,0,0,0,0,2,0}
 ErrorContext             | A           | 109 |         1 |          8192 |         8192 |            1024 |       1 |      7976 |      5696 | {0,0,0,0,0,0,0,0,0,0,1}
 WAL record construction  | A           | 104 |         1 |          8192 |      8388608 |            8192 |       2 |     49552 |      6368 | {0,0,0,0,0,0,0,0,0,1,1}
 CacheMemoryContext       | A           |  23 |         1 |          8192 |      8388608 |            8192 |       8 |   1048360 |    523760 | {0,0,0,0,0,0,0,0,0,2,6}
 dynahash                 | A           |  11 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |       560 | {0,0,0,0,0,0,0,0,0,1,0}
 MessageContext           | A           |   7 |         1 |          8192 |      8388608 |            8192 |       4 |     65320 |     17608 | {0,0,0,0,0,0,0,0,0,2,2}
 dynahash                 | A           |   4 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |       560 | {0,0,0,0,0,0,0,0,0,1,0}
 dynahash                 | A           |   3 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |      1584 | {0,0,0,0,0,0,0,0,0,1,0}
 dynahash                 | A           | 107 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |       560 | {0,0,0,0,0,0,0,0,0,1,0}
 TopTransactionContext    | A           |   2 |         1 |          8192 |      8388608 |            8192 |       1 |      7976 |      7744 | {0,0,0,0,0,0,0,0,0,1,0}
```
## Installation guide

To install `pg_contextdump`, execute in the module's directory:
```shell
make install USE_PGXS=1
```
Go to the the `postgresql.conf` and find **`shared_preload_libraries`** parameter. Add pg_contextdump to the list of libraries. Example:
```
shared_preload_libraries = 'pg_contextdump'
```
Restart the PostgreSQL instance:
```shell
pg_ctl restart
```

After that, execute the following query in psql:
```plpgsql
CREATE EXTENSION pg_contextdump;
```

## Feedback

Send any of your questions or ideas to the Issues on GitHub page.

## Authors

Robert Nazmiev <Nazmiev-R@gaz-is.ru>
