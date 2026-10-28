#include <errno.h>
#include <sys/stat.h>
#include <stddef.h>
/* No application or formatting path may obtain a dynamic heap at runtime. */
void *_sbrk(ptrdiff_t increment) { (void)increment;errno=ENOMEM;return (void *)-1; }
int _write(int fd,const void *data,size_t length){(void)fd;(void)data;return (int)length;}
int _read(int fd,void *data,size_t length){(void)fd;(void)data;(void)length;return 0;}
int _close(int fd){(void)fd;return -1;}
int _fstat(int fd,struct stat *st){(void)fd;st->st_mode=S_IFCHR;return 0;}
int _isatty(int fd){(void)fd;return 1;}
int _lseek(int fd,int position,int whence){(void)fd;(void)position;(void)whence;return 0;}
int _getpid(void){return 1;}
int _kill(int pid,int signal){(void)pid;(void)signal;errno=EINVAL;return -1;}
void _exit(int status){(void)status;for(;;){}}
