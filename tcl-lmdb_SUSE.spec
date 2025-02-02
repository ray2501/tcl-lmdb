%{!?directory:%define directory /usr}

%define buildroot %{_tmppath}/%{name}

Name:          tcl-lmdb
Summary:       Tcl interface for LMDB
Version:       0.5.0
Release:       2
License:       BSD-2-Clause
Group:         Development/Libraries/Tcl
Source:        %{name}-%{version}.tar.gz
URL:           https://github.com/ray2501/tcl-lmdb
BuildRequires: autoconf
BuildRequires: make
BuildRequires: tcl-devel >= 8.1
Requires:      tcl >= 8.1
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
%setup -q -n %{name}-%{version}

%build
CFLAGS="%optflags" ./configure \
	--prefix=%{directory} \
	--exec-prefix=%{directory} \
	--libdir=%{directory}/%{_lib}
make 

%install
make DESTDIR=%{buildroot} pkglibdir=%{tcl_archdir}/%{name}%{version} install

%clean
rm -rf %buildroot

%files
%defattr(-,root,root)
%doc README.md LICENSE.LMDB COPYRIGHT.LMDB
%{tcl_archdir}
%{directory}/share/man/mann
