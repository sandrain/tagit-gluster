--
-- imess xdb schema
--
drop table if exists xdb_file;
drop table if exists xdb_attr;
drop table if exists xdb_xdata;

-- glusterfs files
create table xdb_file (
	fid	integer not null,
	gfid	text not null,
	path	text not null,

	primary key (fid),
	unique (gfid)
);

-- file attributes (only names), including stat(2) and xattrs
create table xdb_attr (
	aid	integer not null,
	name	text not null,

	primary key (aid)
);

-- stat(2) attributes
insert into xdb_attr (name) values ('st_dev');
insert into xdb_attr (name) values ('st_ino');
insert into xdb_attr (name) values ('st_mode');
insert into xdb_attr (name) values ('st_nlink');
insert into xdb_attr (name) values ('st_uid');
insert into xdb_attr (name) values ('st_gid');
insert into xdb_attr (name) values ('st_rdev');
insert into xdb_attr (name) values ('st_size');
insert into xdb_attr (name) values ('st_blksize');
insert into xdb_attr (name) values ('st_blocks');
insert into xdb_attr (name) values ('st_atime');
insert into xdb_attr (name) values ('st_mtime');
insert into xdb_attr (name) values ('st_ctime');

create index ix_xdb_attr_name on xdb_attr (name);

-- file/attributes/values relations
create table xdb_xdata (
	xid	integer not null,
	fid	integer not null references xdb_file (fid),
	aid	integer not null references xdb_attr (aid),
	ival	integer,
	sval	text,
	val	blob not null,

	primary key (xid)
);

create index ix_xdb_xdata_ival on xdb_xdata (aid, ival);
create index ix_xdb_xdata_sval on xdb_xdata (aid, sval);

