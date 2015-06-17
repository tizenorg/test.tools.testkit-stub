Name:		testkit-stub
Summary:	Test stub of testkit-lite
Version:	1.0
Release:	1
Group:		Development/Testing
License:	GPL-2.0
URL:		http://github.com/testkit/testkit-stub
Source:		%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest
BuildRoot:	%{_tmppath}/%{name}-%{version}-build


%description
Test stub for testkit-lite to run web applications


%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1001} .

%build
export CFLAGS+=" -Wno-error=unused-result "
make -j1


%install
rm -rf %{buildroot}
%make_install


%clean
rm -rf %{buildroot}


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_bindir}/testkit-stub
