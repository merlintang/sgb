################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/connect/test1.c \
../src/interfaces/ecpg/test/connect/test2.c \
../src/interfaces/ecpg/test/connect/test3.c \
../src/interfaces/ecpg/test/connect/test4.c \
../src/interfaces/ecpg/test/connect/test5.c 

OBJS += \
./src/interfaces/ecpg/test/connect/test1.o \
./src/interfaces/ecpg/test/connect/test2.o \
./src/interfaces/ecpg/test/connect/test3.o \
./src/interfaces/ecpg/test/connect/test4.o \
./src/interfaces/ecpg/test/connect/test5.o 

C_DEPS += \
./src/interfaces/ecpg/test/connect/test1.d \
./src/interfaces/ecpg/test/connect/test2.d \
./src/interfaces/ecpg/test/connect/test3.d \
./src/interfaces/ecpg/test/connect/test4.d \
./src/interfaces/ecpg/test/connect/test5.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/connect/%.o: ../src/interfaces/ecpg/test/connect/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


