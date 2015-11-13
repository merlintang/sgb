################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/thread/thread.c \
../src/interfaces/ecpg/test/thread/thread_implicit.c 

OBJS += \
./src/interfaces/ecpg/test/thread/thread.o \
./src/interfaces/ecpg/test/thread/thread_implicit.o 

C_DEPS += \
./src/interfaces/ecpg/test/thread/thread.d \
./src/interfaces/ecpg/test/thread/thread_implicit.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/thread/%.o: ../src/interfaces/ecpg/test/thread/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


