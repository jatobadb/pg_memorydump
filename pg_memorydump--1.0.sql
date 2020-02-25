/* SQL main file  */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
--\echo Use "ALTER EXTENSION pg_memorydump" to load this file. \quit

CREATE OR REPLACE FUNCTION pg_memorydump(
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
	AS 'MODULE_PATHNAME', 'pg_memorydump'
	LANGUAGE C IMMUTABLE STRICT;

CREATE VIEW pg_memorydump AS
	SELECT * FROM pg_memorydump();

GRANT SELECT ON pg_memorydump TO PUBLIC;
REVOKE all ON FUNCTION pg_memorydump() FROM PUBLIC;