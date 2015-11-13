################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/intagg/int_aggregate.c 

OBJS += \
./contrib/intagg/int_aggregate.o 

C_DEPS += \
./contrib/intagg/int_aggregate.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/intagg/%.o: ../contrib/intagg/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


