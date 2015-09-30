--
-- imess xdb schema
--
drop table if exists xdb_xgfid;
drop table if exists xdb_xfile;
drop table if exists xdb_xname;
drop table if exists xdb_xdata;

-- glusterfs gfids
create table xdb_xgfid (
	gid	integer not null,
	gfid	text not null,

	primary key (gid),
	unique (gfid)
);

-- glusterfs files
-- FIXME: this cannot support hard links. separate table into two.
create table xdb_xfile (
	fid	integer not null,
	gid	integer not null references xdb_xgfid (gid),
	path	text not null,
	name	text not null,

	primary key (fid),
	unique (path)
);

-- create index ix_xdb_xfile_path on xdb_xfile (path);
create index ix_xdb_xfile_name on xdb_xfile (name);

-- file attributes (only names), including stat(2) and xattrs
create table xdb_xname (
	nid	integer not null,
	name	text not null,

	primary key (nid)
);

-- stat(2) attributes
insert into xdb_xname (name) values ('st_dev');
insert into xdb_xname (name) values ('st_ino');
insert into xdb_xname (name) values ('st_mode');
insert into xdb_xname (name) values ('st_nlink');
insert into xdb_xname (name) values ('st_uid');
insert into xdb_xname (name) values ('st_gid');
insert into xdb_xname (name) values ('st_rdev');
insert into xdb_xname (name) values ('st_size');
insert into xdb_xname (name) values ('st_blksize');
insert into xdb_xname (name) values ('st_blocks');
insert into xdb_xname (name) values ('st_atime');
insert into xdb_xname (name) values ('st_mtime');
insert into xdb_xname (name) values ('st_ctime');

create index ix_xdb_xname_name on xdb_xname (name);

-- file/attributes/values relations
create table xdb_xdata (
	xid	integer not null,
	gid	integer not null references xdb_xgfid (gid),
	nid	integer not null references xdb_xname (nid),
	ival	integer,
	sval	text,

	unique (gid, nid),
	primary key (xid)
);

create index ix_xdb_xdata_ival on xdb_xdata (nid, ival);
create index ix_xdb_xdata_sval on xdb_xdata (nid, sval);

-- this is slow, leave the stale data and take care of them when we mount.
--create trigger trg_xdb_unlink after delete on xdb_xfile
--	begin
--		delete from xdb_xgfid where gid not in
--			(select distinct gid from xdb_xfile);
--		delete from xdb_xdata where gid not in
--			(select gid from xdb_xgfid);
--	end;
