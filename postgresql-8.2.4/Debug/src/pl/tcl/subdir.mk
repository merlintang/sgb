################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/pl/tcl/pltcl.c 

OBJS += \
./src/pl/tcl/pltcl.o 

C_DEPS += \
./src/pl/tcl/pltcl.d 


# Each subdirectory must supply rules for building sources it contributes
src/pl/tcl/%.o: ../src/pl/tcl/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


