################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/cube/cube.c \
../contrib/cube/cubeparse.c \
../contrib/cube/cubescan.c 

OBJS += \
./contrib/cube/cube.o \
./contrib/cube/cubeparse.o \
./contrib/cube/cubescan.o 

C_DEPS += \
./contrib/cube/cube.d \
./contrib/cube/cubeparse.d \
./contrib/cube/cubescan.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/cube/%.o: ../contrib/cube/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


