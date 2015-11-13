################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/port/copydir.o \
../src/port/copydir_srv.o \
../src/port/dirmod.o \
../src/port/dirmod_srv.o \
../src/port/exec.o \
../src/port/exec_srv.o \
../src/port/noblock.o \
../src/port/noblock_srv.o \
../src/port/path.o \
../src/port/path_srv.o \
../src/port/pgsleep.o \
../src/port/pgsleep_srv.o \
../src/port/pgstrcasecmp.o \
../src/port/pgstrcasecmp_srv.o \
../src/port/pipe.o \
../src/port/pipe_srv.o \
../src/port/qsort.o \
../src/port/qsort_arg.o \
../src/port/qsort_arg_srv.o \
../src/port/qsort_srv.o \
../src/port/sprompt.o \
../src/port/sprompt_srv.o \
../src/port/strlcpy.o \
../src/port/strlcpy_srv.o \
../src/port/thread.o \
../src/port/thread_srv.o 

C_SRCS += \
../src/port/copydir.c \
../src/port/crypt.c \
../src/port/dirent.c \
../src/port/dirmod.c \
../src/port/exec.c \
../src/port/fseeko.c \
../src/port/getaddrinfo.c \
../src/port/gethostname.c \
../src/port/getopt.c \
../src/port/getopt_long.c \
../src/port/getrusage.c \
../src/port/gettimeofday.c \
../src/port/inet_aton.c \
../src/port/isinf.c \
../src/port/kill.c \
../src/port/memcmp.c \
../src/port/noblock.c \
../src/port/open.c \
../src/port/path.c \
../src/port/pgsleep.c \
../src/port/pgstrcasecmp.c \
../src/port/pipe.c \
../src/port/qsort.c \
../src/port/qsort_arg.c \
../src/port/rand.c \
../src/port/random.c \
../src/port/rint.c \
../src/port/snprintf.c \
../src/port/sprompt.c \
../src/port/srandom.c \
../src/port/strdup.c \
../src/port/strerror.c \
../src/port/strlcpy.c \
../src/port/strtol.c \
../src/port/strtoul.c \
../src/port/thread.c \
../src/port/unsetenv.c \
../src/port/win32error.c 

OBJS += \
./src/port/copydir.o \
./src/port/crypt.o \
./src/port/dirent.o \
./src/port/dirmod.o \
./src/port/exec.o \
./src/port/fseeko.o \
./src/port/getaddrinfo.o \
./src/port/gethostname.o \
./src/port/getopt.o \
./src/port/getopt_long.o \
./src/port/getrusage.o \
./src/port/gettimeofday.o \
./src/port/inet_aton.o \
./src/port/isinf.o \
./src/port/kill.o \
./src/port/memcmp.o \
./src/port/noblock.o \
./src/port/open.o \
./src/port/path.o \
./src/port/pgsleep.o \
./src/port/pgstrcasecmp.o \
./src/port/pipe.o \
./src/port/qsort.o \
./src/port/qsort_arg.o \
./src/port/rand.o \
./src/port/random.o \
./src/port/rint.o \
./src/port/snprintf.o \
./src/port/sprompt.o \
./src/port/srandom.o \
./src/port/strdup.o \
./src/port/strerror.o \
./src/port/strlcpy.o \
./src/port/strtol.o \
./src/port/strtoul.o \
./src/port/thread.o \
./src/port/unsetenv.o \
./src/port/win32error.o 

C_DEPS += \
./src/port/copydir.d \
./src/port/crypt.d \
./src/port/dirent.d \
./src/port/dirmod.d \
./src/port/exec.d \
./src/port/fseeko.d \
./src/port/getaddrinfo.d \
./src/port/gethostname.d \
./src/port/getopt.d \
./src/port/getopt_long.d \
./src/port/getrusage.d \
./src/port/gettimeofday.d \
./src/port/inet_aton.d \
./src/port/isinf.d \
./src/port/kill.d \
./src/port/memcmp.d \
./src/port/noblock.d \
./src/port/open.d \
./src/port/path.d \
./src/port/pgsleep.d \
./src/port/pgstrcasecmp.d \
./src/port/pipe.d \
./src/port/qsort.d \
./src/port/qsort_arg.d \
./src/port/rand.d \
./src/port/random.d \
./src/port/rint.d \
./src/port/snprintf.d \
./src/port/sprompt.d \
./src/port/srandom.d \
./src/port/strdup.d \
./src/port/strerror.d \
./src/port/strlcpy.d \
./src/port/strtol.d \
./src/port/strtoul.d \
./src/port/thread.d \
./src/port/unsetenv.d \
./src/port/win32error.d 


# Each subdirectory must supply rules for building sources it contributes
src/port/%.o: ../src/port/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


