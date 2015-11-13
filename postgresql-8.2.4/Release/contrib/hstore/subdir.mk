################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/hstore/crc32.c \
../contrib/hstore/hstore_gist.c \
../contrib/hstore/hstore_io.c \
../contrib/hstore/hstore_op.c 

OBJS += \
./contrib/hstore/crc32.o \
./contrib/hstore/hstore_gist.o \
./contrib/hstore/hstore_io.o \
./contrib/hstore/hstore_op.o 

C_DEPS += \
./contrib/hstore/crc32.d \
./contrib/hstore/hstore_gist.d \
./contrib/hstore/hstore_io.d \
./contrib/hstore/hstore_op.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/hstore/%.o: ../contrib/hstore/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


