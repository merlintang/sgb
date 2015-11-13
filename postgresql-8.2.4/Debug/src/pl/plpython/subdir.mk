################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/pl/plpython/plpython.c 

OBJS += \
./src/pl/plpython/plpython.o 

C_DEPS += \
./src/pl/plpython/plpython.d 


# Each subdirectory must supply rules for building sources it contributes
src/pl/plpython/%.o: ../src/pl/plpython/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


