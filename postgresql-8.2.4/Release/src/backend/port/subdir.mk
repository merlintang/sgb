################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/port/SUBSYS.o \
../src/backend/port/dynloader.o \
../src/backend/port/pg_sema.o \
../src/backend/port/pg_shmem.o 

C_SRCS += \
../src/backend/port/dynloader.c \
../src/backend/port/ipc_test.c \
../src/backend/port/pg_sema.c \
../src/backend/port/pg_shmem.c \
../src/backend/port/posix_sema.c \
../src/backend/port/sysv_sema.c \
../src/backend/port/sysv_shmem.c \
../src/backend/port/win32_sema.c 

S_SRCS += \
../src/backend/port/tas.s 

OBJS += \
./src/backend/port/dynloader.o \
./src/backend/port/ipc_test.o \
./src/backend/port/pg_sema.o \
./src/backend/port/pg_shmem.o \
./src/backend/port/posix_sema.o \
./src/backend/port/sysv_sema.o \
./src/backend/port/sysv_shmem.o \
./src/backend/port/tas.o \
./src/backend/port/win32_sema.o 

C_DEPS += \
./src/backend/port/dynloader.d \
./src/backend/port/ipc_test.d \
./src/backend/port/pg_sema.d \
./src/backend/port/pg_shmem.d \
./src/backend/port/posix_sema.d \
./src/backend/port/sysv_sema.d \
./src/backend/port/sysv_shmem.d \
./src/backend/port/win32_sema.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/port/%.o: ../src/backend/port/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/backend/port/%.o: ../src/backend/port/%.s
	@echo 'Building file: $<'
	@echo 'Invoking: GCC Assembler'
	as  -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


