dnl
dnl CHECK_FD_CLOEXEC([action-if-found], [action-if-not-found])
dnl Check if FD_CLOEXEC is supported
dnl
AC_DEFUN([CHECK_FD_CLOEXEC], [{
    AC_MSG_CHECKING(whether FD_CLOEXEC is supported)
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <unistd.h>
#include <fcntl.h>
int main (int argc, char *argv [])
{
    int fds[[2]];
    int res = pipe (fds);
    fcntl(fds[[0]], F_SETFD, fcntl(fds[[0]], F_GETFD) | FD_CLOEXEC);
    return 0;
}
])],
    [AC_MSG_RESULT(yes) ; $1],
    [AC_MSG_RESULT(no)  ; $2])
}])

