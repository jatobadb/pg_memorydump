/* SQL main file  */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
--\echo Use "ALTER EXTENSION pg_contextdump" to load this file. \quit

CREATE OR REPLACE FUNCTION pg_contextdump(
    OUT contextname varchar(128), 
    OUT contexttype char(1),
    OUT id integer, 
    OUT parent_id integer, 
    OUT initBlockSize integer, 
    OUT maxBlockSize integer, 
    OUT allocChunkLimit integer, 
    OUT nblocks integer, 
    OUT totalsize integer, 
    OUT freespace integer, 
    OUT histogramm integer[]
)
	RETURNS SETOF record
	AS 'MODULE_PATHNAME', 'pg_contextdump'
	LANGUAGE C IMMUTABLE STRICT;

CREATE VIEW pg_contextdump AS
	SELECT * FROM pg_contextdump();

GRANT SELECT ON pg_contextdump TO PUBLIC;
REVOKE all ON FUNCTION pg_contextdump() FROM PUBLIC;