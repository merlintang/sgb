################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/fuzzystrmatch/dmetaphone.c \
../contrib/fuzzystrmatch/fuzzystrmatch.c 

OBJS += \
./contrib/fuzzystrmatch/dmetaphone.o \
./contrib/fuzzystrmatch/fuzzystrmatch.o 

C_DEPS += \
./contrib/fuzzystrmatch/dmetaphone.d \
./contrib/fuzzystrmatch/fuzzystrmatch.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/fuzzystrmatch/%.o: ../contrib/fuzzystrmatch/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


