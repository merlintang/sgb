################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/spi/preprocessor/step1.c 

OBJS += \
./contrib/spi/preprocessor/step1.o 

C_DEPS += \
./contrib/spi/preprocessor/step1.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/spi/preprocessor/%.o: ../contrib/spi/preprocessor/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


