-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_contextdump" to load this file. \quit

ALTER EXTENSION pg_contextdump ADD function pg_contextdump();
ALTER EXTENSION pg_contextdump ADD view pg_contextdump;
















