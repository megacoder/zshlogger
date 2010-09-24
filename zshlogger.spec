%define	ver	1.0.0
%define	rel	1
%define	prefix	/usr

Prefix:	%{prefix}

Summary: Run zsh(1) and capture output as annotated text
Name: zshlogger
Version: %{ver}
Release: %{rel}
License: GPLv2
Group: User Interface/Desktops
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: readline-devel,ncurses-devel
Requires: readline,ncurses

%description
Build repeatable demonstrations using zsh(1), based on a scripted directives
file.
%prep
%setup -q

%build
	./autogen.sh
	./configure --prefix=%{prefix}
	make

%install
	rm -rf %{buildroot}
	make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
	rm -rf %{buildroot}

%files
%defattr(-, root, root)
%doc AUTHORS
%doc COPYING
%doc ChangeLog
%doc INSTALL
%doc NEWS
%doc README
%{prefix}/bin/zshlogger

%changelog
