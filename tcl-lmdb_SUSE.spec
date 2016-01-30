%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tcl-lmdb
Summary:       Tcl interface for LMDB
Version:       0.3.1
Release:       2
License:       BSD
Group:         Development/Libraries/Tcl
Source:        https://sites.google.com/site/ray2501/tcl-lmdb/tcl-lmdb_0.3.1.zip
URL:           https://sites.google.com/site/ray2501/tcl-lmdb 
Buildrequires: tcl >= 8.1
BuildRoot:     %{buildroot}

%description
LMDB is a Btree-based database management library with an API similar
to BerkeleyDB. The library is thread-aware and supports concurrent
read/write access from multiple processes and threads. The DB
structure is multi-versioned, and data pages use a copy-on-write
strategy, which also provides resistance to corruption and eliminates
the need for any recovery procedures. The database is exposed in a
memory map, requiring no page cache layer of its own.

This extension provides an easy to use interface for accessing LMDB 
database files from Tcl.

%prep
%setup -q -n %{name}

%build
CFLAGS="%optflags" ./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{directory}/%{_lib}/tcl/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%{directory}/%{_lib}/tcl
%{directory}/share/man/mann
