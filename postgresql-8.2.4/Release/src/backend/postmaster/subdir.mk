################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/postmaster/SUBSYS.o \
../src/backend/postmaster/autovacuum.o \
../src/backend/postmaster/bgwriter.o \
../src/backend/postmaster/fork_process.o \
../src/backend/postmaster/pgarch.o \
../src/backend/postmaster/pgstat.o \
../src/backend/postmaster/postmaster.o \
../src/backend/postmaster/syslogger.o 

C_SRCS += \
../src/backend/postmaster/autovacuum.c \
../src/backend/postmaster/bgwriter.c \
../src/backend/postmaster/fork_process.c \
../src/backend/postmaster/pgarch.c \
../src/backend/postmaster/pgstat.c \
../src/backend/postmaster/postmaster.c \
../src/backend/postmaster/syslogger.c 

OBJS += \
./src/backend/postmaster/autovacuum.o \
./src/backend/postmaster/bgwriter.o \
./src/backend/postmaster/fork_process.o \
./src/backend/postmaster/pgarch.o \
./src/backend/postmaster/pgstat.o \
./src/backend/postmaster/postmaster.o \
./src/backend/postmaster/syslogger.o 

C_DEPS += \
./src/backend/postmaster/autovacuum.d \
./src/backend/postmaster/bgwriter.d \
./src/backend/postmaster/fork_process.d \
./src/backend/postmaster/pgarch.d \
./src/backend/postmaster/pgstat.d \
./src/backend/postmaster/postmaster.d \
./src/backend/postmaster/syslogger.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/postmaster/%.o: ../src/backend/postmaster/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


