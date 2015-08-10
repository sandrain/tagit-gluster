const char *xdb_schema_sqlstr = 
"--\n"
"-- imess xdb schema\n"
"--\n"
"drop table if exists xdb_file;\n"
"drop table if exists xdb_attr;\n"
"drop table if exists xdb_xdata;\n"
"\n"
"-- glusterfs files\n"
"create table xdb_file (\n"
"	fid	integer not null,\n"
"	gfid	text not null,\n"
"	path	text not null,\n"
"\n"
"	primary key (fid),\n"
"	unique (gfid)\n"
");\n"
"\n"
"-- file attributes (only names), including stat(2) and xattrs\n"
"create table xdb_attr (\n"
"	aid	integer not null,\n"
"	name	text not null,\n"
"\n"
"	primary key (aid),\n"
"	unique (name)\n"
");\n"
"\n"
"-- stat(2) attributes\n"
"insert into xdb_attr (name) values ('st_dev');		-- 1\n"
"insert into xdb_attr (name) values ('st_ino');		-- 2\n"
"insert into xdb_attr (name) values ('st_mode');		-- 3\n"
"insert into xdb_attr (name) values ('st_nlink');	-- 4\n"
"insert into xdb_attr (name) values ('st_uid');		-- 5\n"
"insert into xdb_attr (name) values ('st_gid');		-- 6\n"
"insert into xdb_attr (name) values ('st_rdev');		-- 7\n"
"insert into xdb_attr (name) values ('st_size');		-- 8\n"
"insert into xdb_attr (name) values ('st_blksize');	-- 9\n"
"insert into xdb_attr (name) values ('st_blocks');	-- 10\n"
"insert into xdb_attr (name) values ('st_atime');	-- 11\n"
"insert into xdb_attr (name) values ('st_mtime');	-- 12\n"
"insert into xdb_attr (name) values ('st_ctime');	-- 13\n"
"\n"
"create index ix_xdb_attr_name on xdb_attr (name);\n"
"\n"
"-- file/attributes/values relations\n"
"create table xdb_xdata (\n"
"	xid	integer not null,\n"
"	fid	integer not null references xdb_file (fid),\n"
"	aid	integer not null references xdb_attr (aid),\n"
"	ival	integer,\n"
"	sval	text,\n"
"	val	blob not null,\n"
"\n"
"	primary key (xid)\n"
");\n"
"\n"
"create index ix_xdb_xdata_ival on xdb_xdata (aid, ival);\n"
"create index ix_xdb_xdata_sval on xdb_xdata (aid, sval);\n"
"\n"
;