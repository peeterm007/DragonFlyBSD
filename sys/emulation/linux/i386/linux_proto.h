/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $DragonFly: src/sys/emulation/linux/i386/linux_proto.h,v 1.12 2004/03/11 09:53:19 hmp Exp $
 * created from DragonFly: src/sys/emulation/linux/i386/syscalls.master,v 1.4 2003/10/21 01:05:09 daver Exp 
 */

#ifndef _LINUX_SYSPROTO_H_
#define	_LINUX_SYSPROTO_H_

#include <sys/signal.h>

#include <sys/acl.h>

#include <sys/msgport.h>

#include <sys/sysmsg.h>

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

struct	linux_fork_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_open_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	flags;	char flags_[PAD_(l_int)];
	l_int	mode;	char mode_[PAD_(l_int)];
};
struct	linux_waitpid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_pid_t	pid;	char pid_[PAD_(l_pid_t)];
	l_int *	status;	char status_[PAD_(l_int *)];
	l_int	options;	char options_[PAD_(l_int)];
};
struct	linux_creat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	mode;	char mode_[PAD_(l_int)];
};
struct	linux_link_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	char *	to;	char to_[PAD_(char *)];
};
struct	linux_unlink_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	linux_execve_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	char **	argp;	char argp_[PAD_(char **)];
	char **	envp;	char envp_[PAD_(char **)];
};
struct	linux_chdir_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	linux_time_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_time_t *	tm;	char tm_[PAD_(l_time_t *)];
};
struct	linux_mknod_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	mode;	char mode_[PAD_(l_int)];
	l_dev_t	dev;	char dev_[PAD_(l_dev_t)];
};
struct	linux_chmod_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_mode_t	mode;	char mode_[PAD_(l_mode_t)];
};
struct	linux_lchown16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_uid16_t	uid;	char uid_[PAD_(l_uid16_t)];
	l_gid16_t	gid;	char gid_[PAD_(l_gid16_t)];
};
struct	linux_stat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	struct ostat *	up;	char up_[PAD_(struct ostat *)];
};
struct	linux_lseek_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fdes;	char fdes_[PAD_(l_uint)];
	l_off_t	off;	char off_[PAD_(l_off_t)];
	l_int	whence;	char whence_[PAD_(l_int)];
};
struct	linux_getpid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_mount_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	specialfile;	char specialfile_[PAD_(char *)];
	char *	dir;	char dir_[PAD_(char *)];
	char *	filesystemtype;	char filesystemtype_[PAD_(char *)];
	l_ulong	rwflag;	char rwflag_[PAD_(l_ulong)];
	void *	data;	char data_[PAD_(void *)];
};
struct	linux_oldumount_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	linux_setuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid16_t	uid;	char uid_[PAD_(l_uid16_t)];
};
struct	linux_getuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_stime_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_ptrace_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_long	req;	char req_[PAD_(l_long)];
	l_long	pid;	char pid_[PAD_(l_long)];
	l_long	addr;	char addr_[PAD_(l_long)];
	l_long	data;	char data_[PAD_(l_long)];
};
struct	linux_alarm_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	secs;	char secs_[PAD_(l_uint)];
};
struct	linux_fstat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	struct ostat *	up;	char up_[PAD_(struct ostat *)];
};
struct	linux_pause_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_utime_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	fname;	char fname_[PAD_(char *)];
	struct l_utimbuf *	times;	char times_[PAD_(struct l_utimbuf *)];
};
struct	linux_access_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	flags;	char flags_[PAD_(l_int)];
};
struct	linux_nice_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	inc;	char inc_[PAD_(l_int)];
};
struct	linux_kill_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	pid;	char pid_[PAD_(l_int)];
	l_int	signum;	char signum_[PAD_(l_int)];
};
struct	linux_rename_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	from;	char from_[PAD_(char *)];
	char *	to;	char to_[PAD_(char *)];
};
struct	linux_mkdir_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	mode;	char mode_[PAD_(l_int)];
};
struct	linux_rmdir_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
};
struct	linux_pipe_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong *	pipefds;	char pipefds_[PAD_(l_ulong *)];
};
struct	linux_times_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_times_argv *	buf;	char buf_[PAD_(struct l_times_argv *)];
};
struct	linux_brk_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	dsend;	char dsend_[PAD_(l_ulong)];
};
struct	linux_setgid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid16_t	gid;	char gid_[PAD_(l_gid16_t)];
};
struct	linux_getgid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_signal_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	sig;	char sig_[PAD_(l_int)];
	l_handler_t	handler;	char handler_[PAD_(l_handler_t)];
};
struct	linux_geteuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_getegid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_umount_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_int	flags;	char flags_[PAD_(l_int)];
};
struct	linux_ioctl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	l_uint	cmd;	char cmd_[PAD_(l_uint)];
	l_ulong	arg;	char arg_[PAD_(l_ulong)];
};
struct	linux_fcntl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	l_uint	cmd;	char cmd_[PAD_(l_uint)];
	l_ulong	arg;	char arg_[PAD_(l_ulong)];
};
struct	linux_olduname_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_ustat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_dev_t	dev;	char dev_[PAD_(l_dev_t)];
	struct l_ustat *	ubuf;	char ubuf_[PAD_(struct l_ustat *)];
};
struct	linux_sigaction_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	sig;	char sig_[PAD_(l_int)];
	l_osigaction_t *	nsa;	char nsa_[PAD_(l_osigaction_t *)];
	l_osigaction_t *	osa;	char osa_[PAD_(l_osigaction_t *)];
};
struct	linux_sgetmask_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_ssetmask_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_osigset_t	mask;	char mask_[PAD_(l_osigset_t)];
};
struct	linux_setreuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid16_t	ruid;	char ruid_[PAD_(l_uid16_t)];
	l_uid16_t	euid;	char euid_[PAD_(l_uid16_t)];
};
struct	linux_setregid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid16_t	rgid;	char rgid_[PAD_(l_gid16_t)];
	l_gid16_t	egid;	char egid_[PAD_(l_gid16_t)];
};
struct	linux_sigsuspend_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	hist0;	char hist0_[PAD_(l_int)];
	l_int	hist1;	char hist1_[PAD_(l_int)];
	l_osigset_t	mask;	char mask_[PAD_(l_osigset_t)];
};
struct	linux_sigpending_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_osigset_t *	mask;	char mask_[PAD_(l_osigset_t *)];
};
struct	linux_setrlimit_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	resource;	char resource_[PAD_(l_uint)];
	struct l_rlimit *	rlim;	char rlim_[PAD_(struct l_rlimit *)];
};
struct	linux_old_getrlimit_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	resource;	char resource_[PAD_(l_uint)];
	struct l_rlimit *	rlim;	char rlim_[PAD_(struct l_rlimit *)];
};
struct	linux_getgroups16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	gidsetsize;	char gidsetsize_[PAD_(l_uint)];
	l_gid16_t *	gidset;	char gidset_[PAD_(l_gid16_t *)];
};
struct	linux_setgroups16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	gidsetsize;	char gidsetsize_[PAD_(l_uint)];
	l_gid16_t *	gidset;	char gidset_[PAD_(l_gid16_t *)];
};
struct	linux_old_select_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_old_select_argv *	ptr;	char ptr_[PAD_(struct l_old_select_argv *)];
};
struct	linux_symlink_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	char *	to;	char to_[PAD_(char *)];
};
struct	linux_readlink_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	name;	char name_[PAD_(char *)];
	char *	buf;	char buf_[PAD_(char *)];
	l_int	count;	char count_[PAD_(l_int)];
};
struct	linux_uselib_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	library;	char library_[PAD_(char *)];
};
struct	linux_reboot_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	magic1;	char magic1_[PAD_(l_int)];
	l_int	magic2;	char magic2_[PAD_(l_int)];
	l_uint	cmd;	char cmd_[PAD_(l_uint)];
	void *	arg;	char arg_[PAD_(void *)];
};
struct	linux_readdir_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	struct l_dirent *	dent;	char dent_[PAD_(struct l_dirent *)];
	l_uint	count;	char count_[PAD_(l_uint)];
};
struct	linux_mmap_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_mmap_argv *	ptr;	char ptr_[PAD_(struct l_mmap_argv *)];
};
struct	linux_truncate_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_ulong	length;	char length_[PAD_(l_ulong)];
};
struct	linux_ftruncate_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	fd;	char fd_[PAD_(int)];
	long	length;	char length_[PAD_(long)];
};
struct	linux_statfs_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	struct l_statfs_buf *	buf;	char buf_[PAD_(struct l_statfs_buf *)];
};
struct	linux_fstatfs_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	struct l_statfs_buf *	buf;	char buf_[PAD_(struct l_statfs_buf *)];
};
struct	linux_ioperm_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	start;	char start_[PAD_(l_ulong)];
	l_ulong	length;	char length_[PAD_(l_ulong)];
	l_int	enable;	char enable_[PAD_(l_int)];
};
struct	linux_socketcall_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	what;	char what_[PAD_(l_int)];
	l_ulong	args;	char args_[PAD_(l_ulong)];
};
struct	linux_syslog_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	type;	char type_[PAD_(l_int)];
	char *	buf;	char buf_[PAD_(char *)];
	l_int	len;	char len_[PAD_(l_int)];
};
struct	linux_setitimer_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	which;	char which_[PAD_(l_int)];
	struct l_itimerval *	itv;	char itv_[PAD_(struct l_itimerval *)];
	struct l_itimerval *	oitv;	char oitv_[PAD_(struct l_itimerval *)];
};
struct	linux_getitimer_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	which;	char which_[PAD_(l_int)];
	struct l_itimerval *	itv;	char itv_[PAD_(struct l_itimerval *)];
};
struct	linux_newstat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	struct l_newstat *	buf;	char buf_[PAD_(struct l_newstat *)];
};
struct	linux_newlstat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	struct l_newstat *	buf;	char buf_[PAD_(struct l_newstat *)];
};
struct	linux_newfstat_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	struct l_newstat *	buf;	char buf_[PAD_(struct l_newstat *)];
};
struct	linux_uname_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_iopl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	level;	char level_[PAD_(l_ulong)];
};
struct	linux_vhangup_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_vm86old_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_wait4_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_pid_t	pid;	char pid_[PAD_(l_pid_t)];
	l_uint *	status;	char status_[PAD_(l_uint *)];
	l_int	options;	char options_[PAD_(l_int)];
	struct l_rusage *	rusage;	char rusage_[PAD_(struct l_rusage *)];
};
struct	linux_swapoff_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_sysinfo_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_sysinfo *	info;	char info_[PAD_(struct l_sysinfo *)];
};
struct	linux_ipc_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	what;	char what_[PAD_(l_uint)];
	l_int	arg1;	char arg1_[PAD_(l_int)];
	l_int	arg2;	char arg2_[PAD_(l_int)];
	l_int	arg3;	char arg3_[PAD_(l_int)];
	void *	ptr;	char ptr_[PAD_(void *)];
	l_long	arg5;	char arg5_[PAD_(l_long)];
};
struct	linux_sigreturn_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_sigframe *	sfp;	char sfp_[PAD_(struct l_sigframe *)];
};
struct	linux_clone_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	flags;	char flags_[PAD_(l_int)];
	void *	stack;	char stack_[PAD_(void *)];
};
struct	linux_newuname_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_new_utsname *	buf;	char buf_[PAD_(struct l_new_utsname *)];
};
struct	linux_modify_ldt_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	func;	char func_[PAD_(l_int)];
	void *	ptr;	char ptr_[PAD_(void *)];
	l_ulong	bytecount;	char bytecount_[PAD_(l_ulong)];
};
struct	linux_adjtimex_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_sigprocmask_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	how;	char how_[PAD_(l_int)];
	l_osigset_t *	mask;	char mask_[PAD_(l_osigset_t *)];
	l_osigset_t *	omask;	char omask_[PAD_(l_osigset_t *)];
};
struct	linux_create_module_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_init_module_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_delete_module_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_get_kernel_syms_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_quotactl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_bdflush_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_sysfs_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	option;	char option_[PAD_(l_int)];
	l_ulong	arg1;	char arg1_[PAD_(l_ulong)];
	l_ulong	arg2;	char arg2_[PAD_(l_ulong)];
};
struct	linux_personality_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	per;	char per_[PAD_(l_ulong)];
};
struct	linux_setfsuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid16_t	uid;	char uid_[PAD_(l_uid16_t)];
};
struct	linux_setfsgid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid16_t	gid;	char gid_[PAD_(l_gid16_t)];
};
struct	linux_llseek_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	fd;	char fd_[PAD_(l_int)];
	l_ulong	ohigh;	char ohigh_[PAD_(l_ulong)];
	l_ulong	olow;	char olow_[PAD_(l_ulong)];
	l_loff_t *	res;	char res_[PAD_(l_loff_t *)];
	l_uint	whence;	char whence_[PAD_(l_uint)];
};
struct	linux_getdents_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	void *	dent;	char dent_[PAD_(void *)];
	l_uint	count;	char count_[PAD_(l_uint)];
};
struct	linux_select_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	nfds;	char nfds_[PAD_(l_int)];
	l_fd_set *	readfds;	char readfds_[PAD_(l_fd_set *)];
	l_fd_set *	writefds;	char writefds_[PAD_(l_fd_set *)];
	l_fd_set *	exceptfds;	char exceptfds_[PAD_(l_fd_set *)];
	struct l_timeval *	timeout;	char timeout_[PAD_(struct l_timeval *)];
};
struct	linux_msync_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	addr;	char addr_[PAD_(l_ulong)];
	l_size_t	len;	char len_[PAD_(l_size_t)];
	l_int	fl;	char fl_[PAD_(l_int)];
};
struct	linux_getsid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_pid_t	pid;	char pid_[PAD_(l_pid_t)];
};
struct	linux_fdatasync_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
};
struct	linux_sysctl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l___sysctl_args *	args;	char args_[PAD_(struct l___sysctl_args *)];
};
struct	linux_sched_setscheduler_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_pid_t	pid;	char pid_[PAD_(l_pid_t)];
	l_int	policy;	char policy_[PAD_(l_int)];
	struct l_sched_param *	param;	char param_[PAD_(struct l_sched_param *)];
};
struct	linux_sched_getscheduler_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_pid_t	pid;	char pid_[PAD_(l_pid_t)];
};
struct	linux_sched_get_priority_max_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	policy;	char policy_[PAD_(l_int)];
};
struct	linux_sched_get_priority_min_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	policy;	char policy_[PAD_(l_int)];
};
struct	linux_mremap_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	addr;	char addr_[PAD_(l_ulong)];
	l_ulong	old_len;	char old_len_[PAD_(l_ulong)];
	l_ulong	new_len;	char new_len_[PAD_(l_ulong)];
	l_ulong	flags;	char flags_[PAD_(l_ulong)];
	l_ulong	new_addr;	char new_addr_[PAD_(l_ulong)];
};
struct	linux_setresuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid16_t	ruid;	char ruid_[PAD_(l_uid16_t)];
	l_uid16_t	euid;	char euid_[PAD_(l_uid16_t)];
	l_uid16_t	suid;	char suid_[PAD_(l_uid16_t)];
};
struct	linux_getresuid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid16_t *	ruid;	char ruid_[PAD_(l_uid16_t *)];
	l_uid16_t *	euid;	char euid_[PAD_(l_uid16_t *)];
	l_uid16_t *	suid;	char suid_[PAD_(l_uid16_t *)];
};
struct	linux_vm86_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_query_module_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_nfsservctl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_setresgid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid16_t	rgid;	char rgid_[PAD_(l_gid16_t)];
	l_gid16_t	egid;	char egid_[PAD_(l_gid16_t)];
	l_gid16_t	sgid;	char sgid_[PAD_(l_gid16_t)];
};
struct	linux_getresgid16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid16_t *	rgid;	char rgid_[PAD_(l_gid16_t *)];
	l_gid16_t *	egid;	char egid_[PAD_(l_gid16_t *)];
	l_gid16_t *	sgid;	char sgid_[PAD_(l_gid16_t *)];
};
struct	linux_prctl_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_rt_sigreturn_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct l_ucontext *	ucp;	char ucp_[PAD_(struct l_ucontext *)];
};
struct	linux_rt_sigaction_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	sig;	char sig_[PAD_(l_int)];
	l_sigaction_t *	act;	char act_[PAD_(l_sigaction_t *)];
	l_sigaction_t *	oact;	char oact_[PAD_(l_sigaction_t *)];
	l_size_t	sigsetsize;	char sigsetsize_[PAD_(l_size_t)];
};
struct	linux_rt_sigprocmask_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	how;	char how_[PAD_(l_int)];
	l_sigset_t *	mask;	char mask_[PAD_(l_sigset_t *)];
	l_sigset_t *	omask;	char omask_[PAD_(l_sigset_t *)];
	l_size_t	sigsetsize;	char sigsetsize_[PAD_(l_size_t)];
};
struct	linux_rt_sigpending_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_rt_sigtimedwait_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_rt_sigqueueinfo_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_rt_sigsuspend_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_sigset_t *	newset;	char newset_[PAD_(l_sigset_t *)];
	l_size_t	sigsetsize;	char sigsetsize_[PAD_(l_size_t)];
};
struct	linux_pread_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	char *	buf;	char buf_[PAD_(char *)];
	l_size_t	nbyte;	char nbyte_[PAD_(l_size_t)];
	l_loff_t	offset;	char offset_[PAD_(l_loff_t)];
};
struct	linux_pwrite_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	char *	buf;	char buf_[PAD_(char *)];
	l_size_t	nbyte;	char nbyte_[PAD_(l_size_t)];
	l_loff_t	offset;	char offset_[PAD_(l_loff_t)];
};
struct	linux_chown16_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_uid16_t	uid;	char uid_[PAD_(l_uid16_t)];
	l_gid16_t	gid;	char gid_[PAD_(l_gid16_t)];
};
struct	linux_getcwd_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	buf;	char buf_[PAD_(char *)];
	l_ulong	bufsize;	char bufsize_[PAD_(l_ulong)];
};
struct	linux_capget_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_capset_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_sigaltstack_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_stack_t *	uss;	char uss_[PAD_(l_stack_t *)];
	l_stack_t *	uoss;	char uoss_[PAD_(l_stack_t *)];
};
struct	linux_sendfile_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_vfork_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_getrlimit_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	resource;	char resource_[PAD_(l_uint)];
	struct l_rlimit *	rlim;	char rlim_[PAD_(struct l_rlimit *)];
};
struct	linux_mmap2_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	addr;	char addr_[PAD_(l_ulong)];
	l_ulong	len;	char len_[PAD_(l_ulong)];
	l_ulong	prot;	char prot_[PAD_(l_ulong)];
	l_ulong	flags;	char flags_[PAD_(l_ulong)];
	l_ulong	fd;	char fd_[PAD_(l_ulong)];
	l_ulong	pgoff;	char pgoff_[PAD_(l_ulong)];
};
struct	linux_truncate64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_loff_t	length;	char length_[PAD_(l_loff_t)];
};
struct	linux_ftruncate64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	l_loff_t	length;	char length_[PAD_(l_loff_t)];
};
struct	linux_stat64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	filename;	char filename_[PAD_(char *)];
	struct l_stat64 *	statbuf;	char statbuf_[PAD_(struct l_stat64 *)];
	l_long	flags;	char flags_[PAD_(l_long)];
};
struct	linux_lstat64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	filename;	char filename_[PAD_(char *)];
	struct l_stat64 *	statbuf;	char statbuf_[PAD_(struct l_stat64 *)];
	l_long	flags;	char flags_[PAD_(l_long)];
};
struct	linux_fstat64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	fd;	char fd_[PAD_(l_ulong)];
	struct l_stat64 *	statbuf;	char statbuf_[PAD_(struct l_stat64 *)];
	l_long	flags;	char flags_[PAD_(l_long)];
};
struct	linux_lchown_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_uid_t	uid;	char uid_[PAD_(l_uid_t)];
	l_gid_t	gid;	char gid_[PAD_(l_gid_t)];
};
struct	linux_getuid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_getgid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_getgroups_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	gidsetsize;	char gidsetsize_[PAD_(l_int)];
	l_gid_t *	grouplist;	char grouplist_[PAD_(l_gid_t *)];
};
struct	linux_setgroups_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_int	gidsetsize;	char gidsetsize_[PAD_(l_int)];
	l_gid_t *	grouplist;	char grouplist_[PAD_(l_gid_t *)];
};
struct	linux_chown_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	l_uid_t	uid;	char uid_[PAD_(l_uid_t)];
	l_gid_t	gid;	char gid_[PAD_(l_gid_t)];
};
struct	linux_setfsuid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uid_t	uid;	char uid_[PAD_(l_uid_t)];
};
struct	linux_setfsgid_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_gid_t	gid;	char gid_[PAD_(l_gid_t)];
};
struct	linux_pivot_root_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	new_root;	char new_root_[PAD_(char *)];
	char *	put_old;	char put_old_[PAD_(char *)];
};
struct	linux_mincore_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_ulong	start;	char start_[PAD_(l_ulong)];
	l_size_t	len;	char len_[PAD_(l_size_t)];
	u_char *	vec;	char vec_[PAD_(u_char *)];
};
struct	linux_madvise_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	linux_getdents64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	void *	dirent;	char dirent_[PAD_(void *)];
	l_uint	count;	char count_[PAD_(l_uint)];
};
struct	linux_fcntl64_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	l_uint	fd;	char fd_[PAD_(l_uint)];
	l_uint	cmd;	char cmd_[PAD_(l_uint)];
	l_ulong	arg;	char arg_[PAD_(l_ulong)];
};

#ifdef _KERNEL

int	linux_fork (struct linux_fork_args *);
int	linux_open (struct linux_open_args *);
int	linux_waitpid (struct linux_waitpid_args *);
int	linux_creat (struct linux_creat_args *);
int	linux_link (struct linux_link_args *);
int	linux_unlink (struct linux_unlink_args *);
int	linux_execve (struct linux_execve_args *);
int	linux_chdir (struct linux_chdir_args *);
int	linux_time (struct linux_time_args *);
int	linux_mknod (struct linux_mknod_args *);
int	linux_chmod (struct linux_chmod_args *);
int	linux_lchown16 (struct linux_lchown16_args *);
int	linux_stat (struct linux_stat_args *);
int	linux_lseek (struct linux_lseek_args *);
int	linux_getpid (struct linux_getpid_args *);
int	linux_mount (struct linux_mount_args *);
int	linux_oldumount (struct linux_oldumount_args *);
int	linux_setuid16 (struct linux_setuid16_args *);
int	linux_getuid16 (struct linux_getuid16_args *);
int	linux_stime (struct linux_stime_args *);
int	linux_ptrace (struct linux_ptrace_args *);
int	linux_alarm (struct linux_alarm_args *);
int	linux_fstat (struct linux_fstat_args *);
int	linux_pause (struct linux_pause_args *);
int	linux_utime (struct linux_utime_args *);
int	linux_access (struct linux_access_args *);
int	linux_nice (struct linux_nice_args *);
int	linux_kill (struct linux_kill_args *);
int	linux_rename (struct linux_rename_args *);
int	linux_mkdir (struct linux_mkdir_args *);
int	linux_rmdir (struct linux_rmdir_args *);
int	linux_pipe (struct linux_pipe_args *);
int	linux_times (struct linux_times_args *);
int	linux_brk (struct linux_brk_args *);
int	linux_setgid16 (struct linux_setgid16_args *);
int	linux_getgid16 (struct linux_getgid16_args *);
int	linux_signal (struct linux_signal_args *);
int	linux_geteuid16 (struct linux_geteuid16_args *);
int	linux_getegid16 (struct linux_getegid16_args *);
int	linux_umount (struct linux_umount_args *);
int	linux_ioctl (struct linux_ioctl_args *);
int	linux_fcntl (struct linux_fcntl_args *);
int	linux_olduname (struct linux_olduname_args *);
int	linux_ustat (struct linux_ustat_args *);
int	linux_sigaction (struct linux_sigaction_args *);
int	linux_sgetmask (struct linux_sgetmask_args *);
int	linux_ssetmask (struct linux_ssetmask_args *);
int	linux_setreuid16 (struct linux_setreuid16_args *);
int	linux_setregid16 (struct linux_setregid16_args *);
int	linux_sigsuspend (struct linux_sigsuspend_args *);
int	linux_sigpending (struct linux_sigpending_args *);
int	linux_setrlimit (struct linux_setrlimit_args *);
int	linux_old_getrlimit (struct linux_old_getrlimit_args *);
int	linux_getgroups16 (struct linux_getgroups16_args *);
int	linux_setgroups16 (struct linux_setgroups16_args *);
int	linux_old_select (struct linux_old_select_args *);
int	linux_symlink (struct linux_symlink_args *);
int	linux_readlink (struct linux_readlink_args *);
int	linux_uselib (struct linux_uselib_args *);
int	linux_reboot (struct linux_reboot_args *);
int	linux_readdir (struct linux_readdir_args *);
int	linux_mmap (struct linux_mmap_args *);
int	linux_truncate (struct linux_truncate_args *);
int	linux_ftruncate (struct linux_ftruncate_args *);
int	linux_statfs (struct linux_statfs_args *);
int	linux_fstatfs (struct linux_fstatfs_args *);
int	linux_ioperm (struct linux_ioperm_args *);
int	linux_socketcall (struct linux_socketcall_args *);
int	linux_syslog (struct linux_syslog_args *);
int	linux_setitimer (struct linux_setitimer_args *);
int	linux_getitimer (struct linux_getitimer_args *);
int	linux_newstat (struct linux_newstat_args *);
int	linux_newlstat (struct linux_newlstat_args *);
int	linux_newfstat (struct linux_newfstat_args *);
int	linux_uname (struct linux_uname_args *);
int	linux_iopl (struct linux_iopl_args *);
int	linux_vhangup (struct linux_vhangup_args *);
int	linux_vm86old (struct linux_vm86old_args *);
int	linux_wait4 (struct linux_wait4_args *);
int	linux_swapoff (struct linux_swapoff_args *);
int	linux_sysinfo (struct linux_sysinfo_args *);
int	linux_ipc (struct linux_ipc_args *);
int	linux_sigreturn (struct linux_sigreturn_args *);
int	linux_clone (struct linux_clone_args *);
int	linux_newuname (struct linux_newuname_args *);
int	linux_modify_ldt (struct linux_modify_ldt_args *);
int	linux_adjtimex (struct linux_adjtimex_args *);
int	linux_sigprocmask (struct linux_sigprocmask_args *);
int	linux_create_module (struct linux_create_module_args *);
int	linux_init_module (struct linux_init_module_args *);
int	linux_delete_module (struct linux_delete_module_args *);
int	linux_get_kernel_syms (struct linux_get_kernel_syms_args *);
int	linux_quotactl (struct linux_quotactl_args *);
int	linux_bdflush (struct linux_bdflush_args *);
int	linux_sysfs (struct linux_sysfs_args *);
int	linux_personality (struct linux_personality_args *);
int	linux_setfsuid16 (struct linux_setfsuid16_args *);
int	linux_setfsgid16 (struct linux_setfsgid16_args *);
int	linux_llseek (struct linux_llseek_args *);
int	linux_getdents (struct linux_getdents_args *);
int	linux_select (struct linux_select_args *);
int	linux_msync (struct linux_msync_args *);
int	linux_getsid (struct linux_getsid_args *);
int	linux_fdatasync (struct linux_fdatasync_args *);
int	linux_sysctl (struct linux_sysctl_args *);
int	linux_sched_setscheduler (struct linux_sched_setscheduler_args *);
int	linux_sched_getscheduler (struct linux_sched_getscheduler_args *);
int	linux_sched_get_priority_max (struct linux_sched_get_priority_max_args *);
int	linux_sched_get_priority_min (struct linux_sched_get_priority_min_args *);
int	linux_mremap (struct linux_mremap_args *);
int	linux_setresuid16 (struct linux_setresuid16_args *);
int	linux_getresuid16 (struct linux_getresuid16_args *);
int	linux_vm86 (struct linux_vm86_args *);
int	linux_query_module (struct linux_query_module_args *);
int	linux_nfsservctl (struct linux_nfsservctl_args *);
int	linux_setresgid16 (struct linux_setresgid16_args *);
int	linux_getresgid16 (struct linux_getresgid16_args *);
int	linux_prctl (struct linux_prctl_args *);
int	linux_rt_sigreturn (struct linux_rt_sigreturn_args *);
int	linux_rt_sigaction (struct linux_rt_sigaction_args *);
int	linux_rt_sigprocmask (struct linux_rt_sigprocmask_args *);
int	linux_rt_sigpending (struct linux_rt_sigpending_args *);
int	linux_rt_sigtimedwait (struct linux_rt_sigtimedwait_args *);
int	linux_rt_sigqueueinfo (struct linux_rt_sigqueueinfo_args *);
int	linux_rt_sigsuspend (struct linux_rt_sigsuspend_args *);
int	linux_pread (struct linux_pread_args *);
int	linux_pwrite (struct linux_pwrite_args *);
int	linux_chown16 (struct linux_chown16_args *);
int	linux_getcwd (struct linux_getcwd_args *);
int	linux_capget (struct linux_capget_args *);
int	linux_capset (struct linux_capset_args *);
int	linux_sigaltstack (struct linux_sigaltstack_args *);
int	linux_sendfile (struct linux_sendfile_args *);
int	linux_vfork (struct linux_vfork_args *);
int	linux_getrlimit (struct linux_getrlimit_args *);
int	linux_mmap2 (struct linux_mmap2_args *);
int	linux_truncate64 (struct linux_truncate64_args *);
int	linux_ftruncate64 (struct linux_ftruncate64_args *);
int	linux_stat64 (struct linux_stat64_args *);
int	linux_lstat64 (struct linux_lstat64_args *);
int	linux_fstat64 (struct linux_fstat64_args *);
int	linux_lchown (struct linux_lchown_args *);
int	linux_getuid (struct linux_getuid_args *);
int	linux_getgid (struct linux_getgid_args *);
int	linux_getgroups (struct linux_getgroups_args *);
int	linux_setgroups (struct linux_setgroups_args *);
int	linux_chown (struct linux_chown_args *);
int	linux_setfsuid (struct linux_setfsuid_args *);
int	linux_setfsgid (struct linux_setfsgid_args *);
int	linux_pivot_root (struct linux_pivot_root_args *);
int	linux_mincore (struct linux_mincore_args *);
int	linux_madvise (struct linux_madvise_args *);
int	linux_getdents64 (struct linux_getdents64_args *);
int	linux_fcntl64 (struct linux_fcntl64_args *);

#endif /* _KERNEL */

#ifdef COMPAT_43


#ifdef _KERNEL


#endif /* _KERNEL */

#endif /* COMPAT_43 */

#undef PAD_

#endif /* !_LINUX_SYSPROTO_H_ */
