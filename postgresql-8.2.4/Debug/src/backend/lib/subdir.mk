################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/lib/SUBSYS.o \
../src/backend/lib/dllist.o \
../src/backend/lib/stringinfo.o 

C_SRCS += \
../src/backend/lib/dllist.c \
../src/backend/lib/stringinfo.c 

OBJS += \
./src/backend/lib/dllist.o \
./src/backend/lib/stringinfo.o 

C_DEPS += \
./src/backend/lib/dllist.d \
./src/backend/lib/stringinfo.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/lib/%.o: ../src/backend/lib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


