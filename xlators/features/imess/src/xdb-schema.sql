--
-- imess xdb schema
--
drop table if exists xdb_xfile;
drop table if exists xdb_xname;
drop table if exists xdb_xdata;

-- glusterfs files
create table xdb_xfile (
	fid	integer not null,
	gfid	text not null,
	path	text not null,
	name	text not null,

	primary key (fid),
	unique (gfid)
);

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
	fid	integer not null references xdb_xfile (fid),
	nid	integer not null references xdb_attr (nid),
	ival	integer,
	sval	text,

	unique (fid, nid),
	primary key (xid)
);

-- upon unlinking the file entry, remove all xdata entries.
-- keep the xname entries for later (possible) hits.
-- this should be a before trigger, however, using after trigger according to
-- the recommendation of the sqlite documenentation.
create trigger tr_xdb_xfile_unlink after delete on xdb_xfile
	begin
		delete from xdb_xdata where xdb_xdata.fid = OLD.fid;
	end;

create index ix_xdb_xdata_ival on xdb_xdata (nid, ival);
create index ix_xdb_xdata_sval on xdb_xdata (nid, sval);

