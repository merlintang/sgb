################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/storage/large_object/SUBSYS.o \
../src/backend/storage/large_object/inv_api.o 

C_SRCS += \
../src/backend/storage/large_object/inv_api.c 

OBJS += \
./src/backend/storage/large_object/inv_api.o 

C_DEPS += \
./src/backend/storage/large_object/inv_api.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/storage/large_object/%.o: ../src/backend/storage/large_object/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


