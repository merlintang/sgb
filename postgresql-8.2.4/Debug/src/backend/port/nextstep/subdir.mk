################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/backend/port/nextstep/port.c 

OBJS += \
./src/backend/port/nextstep/port.o 

C_DEPS += \
./src/backend/port/nextstep/port.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/port/nextstep/%.o: ../src/backend/port/nextstep/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


