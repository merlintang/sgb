################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/pgtypeslib/dt_test.c \
../src/interfaces/ecpg/test/pgtypeslib/dt_test2.c \
../src/interfaces/ecpg/test/pgtypeslib/num_test.c \
../src/interfaces/ecpg/test/pgtypeslib/num_test2.c 

OBJS += \
./src/interfaces/ecpg/test/pgtypeslib/dt_test.o \
./src/interfaces/ecpg/test/pgtypeslib/dt_test2.o \
./src/interfaces/ecpg/test/pgtypeslib/num_test.o \
./src/interfaces/ecpg/test/pgtypeslib/num_test2.o 

C_DEPS += \
./src/interfaces/ecpg/test/pgtypeslib/dt_test.d \
./src/interfaces/ecpg/test/pgtypeslib/dt_test2.d \
./src/interfaces/ecpg/test/pgtypeslib/num_test.d \
./src/interfaces/ecpg/test/pgtypeslib/num_test2.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/pgtypeslib/%.o: ../src/interfaces/ecpg/test/pgtypeslib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


