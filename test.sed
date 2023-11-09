AM_PROG_LIBTOOL
fi

if test -n "${sys_dir}"; then
  case ${sys_dir} in
	a29khif) AC_CONFIG_SUBDIRS(a29khif) ;;
	arm) AC_CONFIG_SUBDIRS(arm) ;;
	d10v) AC_CONFIG_SUBDIRS(d10v) ;;
	decstation) AC_CONFIG_SUBDIRS(dec
