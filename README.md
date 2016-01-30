tcl-lmdb
=====

This is the Lightning Memory-Mapped Database (LMDB) extension for Tcl 
using the Tcl Extension Architecture (TEA).

LMDB is a Btree-based database management library with an API similar 
to BerkeleyDB. The library is thread-aware and supports concurrent 
read/write access from multiple processes and threads. The DB structure 
is multi-versioned, and data pages use a copy-on-write strategy, which 
also provides resistance to corruption and eliminates the need for any 
recovery procedures. The database is exposed in a memory map, requiring 
no page cache layer of its own.

For additional information on LMDB see

[http://symas.com/mdb/] (http://symas.com/mdb/) 


License
=====

LMDB is Licensed under the [OpenLDAP, Public License] (http://www.openldap.org/software/release/license.html).  
tcl-lmdb is Licensed under the 2-Clause BSD license.


Documents
=====

* [LMDB Source code] (https://github.com/LMDB/lmdb)
* [LMDB C API reference] (http://symas.com/mdb/doc/index.html)


UNIX BUILD
=====

Building under most UNIX systems is easy, just run the configure script
and then run make. For more information about the build process, see
the tcl/unix/README file in the Tcl src dist. The following minimal
example will install the extension in the /opt/tcl directory.

    $ cd tcl-lmdb
    $ ./configure --prefix=/opt/tcl
    $ make
    $ make install
	
If you need setup directory containing tcl configuration (tclConfig.sh),
below is an example:

    $ cd tcl-lmdb
    $ ./configure --with-tcl=/opt/activetcl/lib
    $ make
    $ make install

If your Linux distribution (ex. Debian, Ubuntu, Fedora, and OpenSuSE) 
includes LMDB, tcl-lmdb support to use the system shared library for LMDB.

Below is an example:

    $ ./configure --with-system-lmdb=yes


WINDOWS BUILD
=====

The recommended method to build extensions under windows is to use the
Msys + Mingw build process. This provides a Unix-style build while
generating native Windows binaries. Using the Msys + Mingw build tools
means that you can use the same configure script as per the Unix build
to create a Makefile.


Implement commands
=====

The key and data is interpreted by Tcl as a byte array.

### Basic usage
lmdb version ?-string?

The command lmdb version return a list of the form {major minor patch} 
for the major, minor and patch levels of the LMDB release. -string 
return a string with LMDB version information.

### Database Environment

lmdb env  
env_handle open -path path ?-mode mode? ?-fixedmap BOOLEAN? ?-nosubdir BOOLEAN? ?-readonly BOOLEAN? ?-nosync BOOLEAN? ?-nordahead BOOLEAN?  
env_handle set_mapsize size  
env_handle set_maxreaders nReaders  
env_handle set_maxdbs nDbs  
env_handle sync force  
env_handle stat  
env_handle copy path ?-cp_compact boolean?  
env_handle get_path  
env_handle get_maxreaders  
env_handle get_maxkeysize  
env_handle close  

The lmdb env create an environment handle env_handle. The returned 
environment handle is bound to a Tcl command of the form envN, where N 
is an integer starting at 0 (for example, env0 and env1).

The env_handle open open an environment handle. The path is the directory 
in which the database files reside. This directory must already exist and 
be writable. The mode is the UNIX permissions to set on created files and 
semaphores. This parameter is ignored on Windows. -nosync flag setup don't 
flush system buffers to disk when committing a transaction.

By default, LMDB creates its environment in a directory whose pathname is 
given in path, and creates its data and lock files under that directory. 
With -nosubdir this option, path is used as-is for the database main data 
file. The database lock file is the path with "-lock" appended.

-nordahead this option turn off readahead. Most operating systems perform 
readahead on read requests by default. This option turns it off if the OS 
supports it. Turning it off may help random read performance when the DB 
is larger than RAM and system RAM is full. The option is not implemented 
on Windows.

The env_handle set_mapsize size set the size of the memory map to use for 
this environment. Default size of memory map is 10485760. Apps should always 
set the size explicitly using env_handle set_mapsize to setup size of the 
memory map.

The env_handle set_maxdbs set the maximum number of named databases for 
the environment. This command is only needed if multiple databases will 
be used in the environment. Simpler applications that use the environment 
as a single unnamed database can ignore this option. This function may 
only be called after lmdb env and before env_handle open command.

The env_handle sync flush the data buffers to disk. force is non-zero, 
force a synchronous flush. 0 do nothing. Data is always written to disk 
when is called, but the operating system may keep it buffered. LMDB 
always flushes the OS buffers upon commit as well, unless the environment 
was opened with -nosync. This command returns 0 on success, and in the 
case of error, a Tcl error is thrown.

The env_handle copy copy an LMDB environment to the specified path. 
-cp_compact perform compaction while copying. This command returns 0 on 
success, and in the case of error, a Tcl error is thrown.

The env_handle stat return statistics list about the LMDB environment.

The env_handle close command close the environment and release the memory
map. This command returns 0 on success, and in the case of error, a Tcl 
error is thrown.

### Database

lmdb open -env env_handle ?-name database? ?-reversekey BOOLEAN? ?-dupsort BOOLEAN? ?-dupfixed BOOLEAN? ?-reversedup BOOLEAN? ?-create BOOLEAN?  
dbi_handle put key data -txn txnid ?-nodupdata boolean? ?-nooverwrite boolean? ?-append boolean? ?-appenddup boolean?  
dbi_handle get key -txn txnid  
dbi_handle del key data -txn txnid  
dbi_handle drop del_flag -txn txnid  
dbi_handle stat -txn txnid  
dbi_handle close -env env_handle  

The command lmdb open create a database handle. -name database is the name 
of the database to open option. If only a single database is needed in the 
environment, skip to setup database name (or you need use env_handle 
set_maxdbs to set the maximum number of named databases for the environment).

-dupsort let duplicate keys may be used in the database. (Or, from another 
perspective, keys may have multiple data items, stored in sorted order.) 
By default keys must be unique and may have only a single data item. 
-dupfixed may only be used in combination with -dupsort, sorted dup items 
have fixed size.

-create to create the named database if it doesn't exist. This option is not 
allowed in a read-only transaction or a read-only environment. The returned 
database handle is bound to a Tcl command of the form dbiN, where N is an 
integer starting at 0 (for example, dbi0 and dbi1).

The command dbi_handle get get items from a database. If the database 
supports duplicate keys -dupsort then the first data item for the key will 
be returned. Retrieval of other items requires the use of cursor_handle get.

The command dbi_handle put store items into a database. -nodupdata may only 
be specified if the database was opened with -dupsort. -nooverwrite enter 
the new key/data pair only if the key does not already appear in the database.

The command dbi_handle del delete items from a database. If the database 
supports sorted duplicates and the data parameter is "" (empty string), 
all of the duplicate data items for the key will be deleted. Otherwise, if 
the data parameter is non-NULL only the matching data item will be deleted.

The command dbi_handle drop empty or delete+close a database. del_flag setup 0 
to empty the DB, 1 to delete it from the environment and close the DB handle.

The command dbi_handle stat return statistics list for a database.

### Transactions

env_handle txn ?-parent txnid? ?-readonly boolean?  
txn_handle abort  
txn_handle commit  
txn_handle reset  
txn_handle renew  
txn_handle close  

The command env_handle txn create a transaction for use with the environment. 
-parent txnid please notice: nested transactions max 1 child, write txns only. 
-readonly this transaction will not perform any write operations. The returned 
transaction handle is bound to a Tcl command of the form env.txnX, where X is 
an integer starting at 0 (for example, env0.txn0 and env0.txn1).

The command txn_handle reset reset a read-only transaction. Abort the 
transaction like txn_handle abort, but keep the transaction handle. txn_handle 
renew may reuse the handle. This command returns 0 on success, and in the case 
of error, a Tcl error is thrown.

### Cursor

dbi_handle cursor -txn txnid  
cursor_handle get ?-current? ?-first? ?-firstdup? ?-last? ?-lastdup? ?-next? ?-nextdup? ? ?-nextnodup? ?-prev? ?-prevdup? ?-prevnodup?  
cursor_handle get -set key  
cursor_handle get -set_range key  
cursor_handle get -get_both key data  
cursor_handle get -get_both_range key data  
cursor_handle put key data ?-current boolean? ?-nodupdata boolean? ?-nooverwrite boolean? ?-append boolean? ?-appenddup boolean?  
cursor_handle del ?-nodupdata boolean?  
cursor_handle renew -txn txnid  
cursor_handle count  
cursor_handle close  

The dbi_handle cursor command creates a database cursor. The returned cursor 
handle is bound to a Tcl command of the form dbiN.cX, where X is an integer 
starting at 0 (for example, dbi0.c0 and dbi0.c1).

The cursor_handle get command returns a list of {key value} pairs. -firstdup, 
-lastdup, -nextnodup, -prevdup and -get_both only for -dupsort. -set is 
position at specified key. -set_range is position at first key greater than 
or equal to specified key. -get_both is position at key/data pair. Only for 
-dupsort. -get_both_range is position at key, nearest data. Only for -dupsort.

The cursor_handle put command stores key/data pairs into the database. 
-nodupdata may only be specified if the database was opened with -dupsort. 
This command returns 0 on success, and in the case of error, a Tcl error is 
thrown.

The cursor_handle del command deletes the key/data pair to which the cursor 
refers. This command returns 0 on success, and in the case of error, a Tcl error 
is thrown.

The cursor_handle renew command renew a cursor handle. A cursor is associated 
with a specific transaction and database. Cursors that are only used in 
read-only transactions may be re-used, to avoid unnecessary malloc/free 
overhead. The cursor may be associated with a new read-only transaction, and 
referencing the same database handle as it was created with. This may be done 
whether the previous transaction is live or dead.

The cursor_handle count command return count of duplicates for current key. 
This command is only valid on databases that support sorted duplicate data 
items -dupsort. 


Examples
=====

### Get LMDB version

    package require lmdb

    set version [lmdb version -string]
    puts "LMDB version is $version"

### Create Env and get basic info

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    $myenv set_maxreaders 127
    file mkdir "testdb"
    $myenv open -path "testdb"
    set mydbi [lmdb open -env $myenv]

    puts [$myenv get_path]
    puts [$myenv get_maxreaders]
    puts [$myenv get_maxkeysize]

    file mkdir "test.db"
    # Copy an LMDB environment to the specified path
    $myenv copy "test.db"

    puts "Check current stat:"
    set stat [$myenv stat]
    puts "ms_psize: [lindex $stat 0]"
    puts "ms_depth: [lindex $stat 1]"
    puts "ms_branch_pages: [lindex $stat 2]"
    puts "ms_leaf_pages: [lindex $stat 3]"
    puts "ms_overflow_pages: [lindex $stat 4]"
    puts "ms_entries: [lindex $stat 5]"

    $mydbi close -env $myenv
    $myenv close
    exit

### Put and get data

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    file mkdir "testdb"

    if {[catch {$myenv open -path "testdb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv]

    set mytxn [$myenv txn]
    for {set i 1} {$i < 1000} {incr i} {
        $mydbi put $i $i -txn $mytxn
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    for {set i 1} {$i < 1000} {incr i} {
    puts [$mydbi get $i -txn $mytxn2]
    }
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit

### Put and get data (nosubdir case, for v0.2.4)

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824

    if {[catch {$myenv open -path "mytestdb" -nosubdir 1} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv]

    set mytxn [$myenv txn]
    for {set i 1000} {$i < 5000} {incr i} {
        $mydbi put $i $i -txn $mytxn
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    for {set i 1000} {$i < 5000} {incr i} {
    puts [$mydbi get $i -txn $mytxn2]
    }
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit

### Put and get a file

    package require lmdb

    set filename "lmdb-mdb.master.zip"
    set size [file size "/home/danilo/Downloads/lmdb-mdb.master.zip"] 
    set fd [open "/home/danilo/Downloads/lmdb-mdb.master.zip" {RDWR BINARY}]
    fconfigure $fd -blocking 1 -encoding binary -translation binary 
    set data [read $fd $size]
    close $fd  

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    file mkdir "testdb"

    if {[catch {$myenv open -path "testdb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv]

    set mytxn [$myenv txn]
    $mydbi put $filename $data -txn $mytxn
    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    set fetch_data [$mydbi get $filename -txn $mytxn2]
    set fd [open "/home/danilo/Downloads/lmdb-mdb.master_test.zip" {CREAT RDWR BINARY}]  
    fconfigure $fd -blocking 1 -encoding binary -translation binary 
    puts -nonewline $fd $fetch_data  
    close $fd
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close

### Cursor

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    file mkdir "testdb"

    if {[catch {$myenv open -path "testdb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv]

    set mytxn [$myenv txn]
    for {set i 1} {$i < 1000} {incr i} {
        $mydbi put $i $i -txn $mytxn
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    set mycursor [$mydbi cursor -txn $mytxn2]

    while { [catch {set data [$mycursor get -next]} result] == 0} {   
        puts $data
    }

    $mycursor close
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit

### Cursor (dupsort)

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    $myenv set_maxdbs 10
    file mkdir "testdb"

    if {[catch {$myenv open -path "testdb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv -name "testdb" -dupsort 1 -create 1]

    set mytxn [$myenv txn]
    for {set i 1} {$i < 1000} {incr i} {
    for {set j 1} {$j < 10} {incr j} {
        $mydbi put $i $j -txn $mytxn
    }
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    set mycursor [$mydbi cursor -txn $mytxn2]

    while { [catch {set data [$mycursor get -nextnodup]} result] == 0} {   
        puts $data
        set number [$mycursor count]       
        for {set num 0} {$num < $number} {incr num} {
        if { [catch {set data [$mycursor get -nextdup]} result] == 0 } {
            puts $data
        }
        }
    }

    $mycursor close
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit

### Cursor (dupfixed, for 0.3.1)

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    $myenv set_maxdbs 10
    file mkdir "fixeddb"

    if {[catch {$myenv open -path "fixeddb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv -name "fixeddb" \
                         -dupsort 1 -dupfixed 1 -create 1]

    set mytxn [$myenv txn]
    for {set i 1} {$i < 10} {incr i} {
    for {set j 1} {$j < 20} {incr j} {
        set value [format "%07x" $j]
        $mydbi put $i $value -txn $mytxn
    }
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    set mycursor [$mydbi cursor -txn $mytxn2]

    while { [catch {set data [$mycursor get -nextnodup]} result] == 0} {
        puts $data
        set number [$mycursor count]
        for {set num 0} {$num < $number} {incr num} {
        if { [catch {set data [$mycursor get -nextdup]} result] == 0 } {
            puts $data
        }
        }
    }

    $mycursor close
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit

### Cursor and Transaction

    package require lmdb

    set myenv [lmdb env]
    $myenv set_mapsize 1073741824
    file mkdir "testdb"

    if {[catch {$myenv open -path "testdb"} error] != 0} {
        puts "open database fail: $error"
        $myenv close
        exit
    }

    set mydbi [lmdb open -env $myenv]

    set mytxn [$myenv txn]
    for {set i 1} {$i <= 1000} {incr i} {
        $mydbi put $i $i -txn $mytxn
    }

    $mytxn commit
    $mytxn close

    set mytxn2 [$myenv txn -readonly 1]
    set mycursor [$mydbi cursor -txn $mytxn2]

    while { [catch {set data [$mycursor get -next]} result] == 0} {   
        puts $data
    }

    $mytxn2 reset
    $mytxn2 renew
    $mycursor renew -txn $mytxn2

    set data [$mycursor get -last]
    while { [catch {set data [$mycursor get -prev]} result] == 0} {   
        puts $data
    }

    $mycursor close
    $mytxn2 abort
    $mytxn2 close

    $mydbi close -env $myenv
    $myenv close
    exit 

### Thread (for v0.2.3)

    package require Thread

    set t1 [thread::create]
    thread::send $t1 {
        package require lmdb
        set myenv [lmdb env]
        $myenv set_mapsize 268435456
        $myenv set_maxdbs 10
        file mkdir "testdb"
            
        if {[catch {$myenv open -path "testdb"} error] != 0} {
            puts "open database fail: $error"
            $myenv close
            exit
        }    
        set mydbi [lmdb open -env $myenv -name "testdb" -create 1]
    }

    thread::send $t1 {
        set mytxn [$myenv txn]
        for {set i 1} {$i < 10} {incr i} {
            $mydbi put $i $i -txn $mytxn
        }

        $mytxn commit
        $mytxn close

        set mytxn2 [$myenv txn -readonly 1]
        for {set i 1} {$i < 10} {incr i} {
            puts [$mydbi get $i -txn $mytxn2]
        }
        $mytxn2 abort
        $mytxn2 close
    }

    thread::send $t1 {
        set mytxn [$myenv txn]
        $mydbi put foo bar -txn $mytxn
        $mytxn commit
        $mytxn close
    }

    set t2 [thread::create]
    thread::send $t2 {
        package require lmdb
        set myenv [lmdb env]
        $myenv set_mapsize 268435456
        $myenv set_maxdbs 10
        file mkdir "testdb"

        # The default flag setup -fixedmap to 1.
        # However, if you use other thread and the same env,
        # please setup -fixedmap to 0 (still need test)
        # or LMDB return ERROR: Device or resource busy   
        if {[catch {$myenv open -path "testdb" -fixedmap 0} error] != 0} {
            puts "open database fail: $error"
            $myenv close
            exit
        }           
        set dbi [lmdb open -env $myenv -name "testdb2" -create 1]
    }

    thread::send $t1 {
        set mytxn2 [$myenv txn -readonly 1]
        puts [$mydbi get foo -txn $mytxn2]
        $mytxn2 abort
        $mytxn2 close
    }

    thread::send $t2 {
        set mytxn [$myenv txn]
        for {set i 1} {$i < 10} {incr i} {
            $dbi put $i $i -txn $mytxn
        }

        $mytxn commit
        $mytxn close

        set mytxn2 [$myenv txn -readonly 1]
        for {set i 1} {$i < 10} {incr i} {
            puts [$dbi get $i -txn $mytxn2]
        }
        $mytxn2 abort
        $mytxn2 close
    }

    thread::send $t1 {
    $mydbi close -env $myenv
    $myenv close
    }

    thread::send $t2 {
    $dbi close -env $myenv
    $myenv close
    }

    exit
