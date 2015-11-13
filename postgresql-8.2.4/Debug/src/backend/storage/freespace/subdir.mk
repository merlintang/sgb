################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/storage/freespace/SUBSYS.o \
../src/backend/storage/freespace/freespace.o 

C_SRCS += \
../src/backend/storage/freespace/freespace.c 

OBJS += \
./src/backend/storage/freespace/freespace.o 

C_DEPS += \
./src/backend/storage/freespace/freespace.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/storage/freespace/%.o: ../src/backend/storage/freespace/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


