--
-- imess-view-db-schema.sql
--

drop table if exists imess_views;

create table imess_views (
	vid	integer not null,
	name	text not null,
	sql	text not null,

	primary key (vid)
);

