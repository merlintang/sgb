################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/bin/initdb/initdb.o 

C_SRCS += \
../src/bin/initdb/initdb.c 

OBJS += \
./src/bin/initdb/initdb.o 

C_DEPS += \
./src/bin/initdb/initdb.d 


# Each subdirectory must supply rules for building sources it contributes
src/bin/initdb/%.o: ../src/bin/initdb/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


