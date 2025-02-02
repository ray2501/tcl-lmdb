# -*- tcl -*-
# Tcl package index file, version 1.1
#
if {[package vsatisfies [package provide Tcl] 9.0-]} {
    package ifneeded lmdb 0.5.0 \
	    [list load [file join $dir libtcl9lmdb0.5.0.so] [string totitle lmdb]]
} else {
    package ifneeded lmdb 0.5.0 \
	    [list load [file join $dir liblmdb0.5.0.so] [string totitle lmdb]]
}
