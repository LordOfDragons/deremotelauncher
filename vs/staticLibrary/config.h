/*
Configuration File. Do not edit since
this is a generated file and all changes
will be lost the next time scons runs.
*/

/* disable OS_BEOS */
#ifdef OS_BEOS
#undef OS_BEOS
#endif

/* disable OS_MACOS */
#ifdef OS_MACOS
#undef OS_MACOS
#endif

/* enable OS_UNIX */
#ifdef OS_UNIX
#undefine OS_UNIX
#endif

/* disable OS_W32 */
#ifndef OS_W32
#define OS_W32 1
#endif

/* enable WITH_OPTIMIZATIONS */
#ifndef WITH_OPTIMIZATIONS
#define WITH_OPTIMIZATIONS 1
#endif

/* End of configuration file. */