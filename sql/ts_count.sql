--
-- first, load the function.  Turn off echoing so that expected file
-- does not depend on contents of ts_count.sql.
--
SET client_min_messages = warning;
\set ECHO none
\i ts_count.sql
RESET client_min_messages;
\set ECHO all

select ts_count(to_tsvector('managing managers manage peons managerially'),
                to_tsquery('managers'));
select ts_count(to_tsvector('managing managers manage peons managerially'),
                to_tsquery('managers | peon'));

