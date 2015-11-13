################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/storage/ipc/SUBSYS.o \
../src/backend/storage/ipc/ipc.o \
../src/backend/storage/ipc/ipci.o \
../src/backend/storage/ipc/pmsignal.o \
../src/backend/storage/ipc/procarray.o \
../src/backend/storage/ipc/shmem.o \
../src/backend/storage/ipc/shmqueue.o \
../src/backend/storage/ipc/sinval.o \
../src/backend/storage/ipc/sinvaladt.o 

C_SRCS += \
../src/backend/storage/ipc/ipc.c \
../src/backend/storage/ipc/ipci.c \
../src/backend/storage/ipc/pmsignal.c \
../src/backend/storage/ipc/procarray.c \
../src/backend/storage/ipc/shmem.c \
../src/backend/storage/ipc/shmqueue.c \
../src/backend/storage/ipc/sinval.c \
../src/backend/storage/ipc/sinvaladt.c 

OBJS += \
./src/backend/storage/ipc/ipc.o \
./src/backend/storage/ipc/ipci.o \
./src/backend/storage/ipc/pmsignal.o \
./src/backend/storage/ipc/procarray.o \
./src/backend/storage/ipc/shmem.o \
./src/backend/storage/ipc/shmqueue.o \
./src/backend/storage/ipc/sinval.o \
./src/backend/storage/ipc/sinvaladt.o 

C_DEPS += \
./src/backend/storage/ipc/ipc.d \
./src/backend/storage/ipc/ipci.d \
./src/backend/storage/ipc/pmsignal.d \
./src/backend/storage/ipc/procarray.d \
./src/backend/storage/ipc/shmem.d \
./src/backend/storage/ipc/shmqueue.d \
./src/backend/storage/ipc/sinval.d \
./src/backend/storage/ipc/sinvaladt.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/storage/ipc/%.o: ../src/backend/storage/ipc/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


