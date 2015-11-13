################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/tools/entab/entab.c \
../src/tools/entab/halt.c 

OBJS += \
./src/tools/entab/entab.o \
./src/tools/entab/halt.o 

C_DEPS += \
./src/tools/entab/entab.d \
./src/tools/entab/halt.d 


# Each subdirectory must supply rules for building sources it contributes
src/tools/entab/%.o: ../src/tools/entab/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


