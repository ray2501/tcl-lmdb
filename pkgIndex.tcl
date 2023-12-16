# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded lmdb 0.4.2 \
	    [list load [file join $dir libtcl9lmdb0.4.2.so] [string totitle lmdb]]
} else {
    package ifneeded lmdb 0.4.2 \
	    [list load [file join $dir liblmdb0.4.2.so] [string totitle lmdb]]
}
