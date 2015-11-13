################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/tsearch2/ispell/regis.c \
../contrib/tsearch2/ispell/spell.c 

OBJS += \
./contrib/tsearch2/ispell/regis.o \
./contrib/tsearch2/ispell/spell.o 

C_DEPS += \
./contrib/tsearch2/ispell/regis.d \
./contrib/tsearch2/ispell/spell.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/tsearch2/ispell/%.o: ../contrib/tsearch2/ispell/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


