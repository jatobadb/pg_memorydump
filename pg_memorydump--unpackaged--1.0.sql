-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_memorydump" to load this file. \quit

ALTER EXTENSION pg_memorydump ADD function pg_memorydump();
ALTER EXTENSION pg_memorydump ADD view pg_memorydump;
















