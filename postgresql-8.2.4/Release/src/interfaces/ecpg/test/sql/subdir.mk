################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/sql/array.c \
../src/interfaces/ecpg/test/sql/binary.c \
../src/interfaces/ecpg/test/sql/code100.c \
../src/interfaces/ecpg/test/sql/copystdout.c \
../src/interfaces/ecpg/test/sql/define.c \
../src/interfaces/ecpg/test/sql/desc.c \
../src/interfaces/ecpg/test/sql/dynalloc.c \
../src/interfaces/ecpg/test/sql/dynalloc2.c \
../src/interfaces/ecpg/test/sql/dyntest.c \
../src/interfaces/ecpg/test/sql/execute.c \
../src/interfaces/ecpg/test/sql/fetch.c \
../src/interfaces/ecpg/test/sql/func.c \
../src/interfaces/ecpg/test/sql/indicators.c \
../src/interfaces/ecpg/test/sql/quote.c \
../src/interfaces/ecpg/test/sql/show.c \
../src/interfaces/ecpg/test/sql/update.c 

OBJS += \
./src/interfaces/ecpg/test/sql/array.o \
./src/interfaces/ecpg/test/sql/binary.o \
./src/interfaces/ecpg/test/sql/code100.o \
./src/interfaces/ecpg/test/sql/copystdout.o \
./src/interfaces/ecpg/test/sql/define.o \
./src/interfaces/ecpg/test/sql/desc.o \
./src/interfaces/ecpg/test/sql/dynalloc.o \
./src/interfaces/ecpg/test/sql/dynalloc2.o \
./src/interfaces/ecpg/test/sql/dyntest.o \
./src/interfaces/ecpg/test/sql/execute.o \
./src/interfaces/ecpg/test/sql/fetch.o \
./src/interfaces/ecpg/test/sql/func.o \
./src/interfaces/ecpg/test/sql/indicators.o \
./src/interfaces/ecpg/test/sql/quote.o \
./src/interfaces/ecpg/test/sql/show.o \
./src/interfaces/ecpg/test/sql/update.o 

C_DEPS += \
./src/interfaces/ecpg/test/sql/array.d \
./src/interfaces/ecpg/test/sql/binary.d \
./src/interfaces/ecpg/test/sql/code100.d \
./src/interfaces/ecpg/test/sql/copystdout.d \
./src/interfaces/ecpg/test/sql/define.d \
./src/interfaces/ecpg/test/sql/desc.d \
./src/interfaces/ecpg/test/sql/dynalloc.d \
./src/interfaces/ecpg/test/sql/dynalloc2.d \
./src/interfaces/ecpg/test/sql/dyntest.d \
./src/interfaces/ecpg/test/sql/execute.d \
./src/interfaces/ecpg/test/sql/fetch.d \
./src/interfaces/ecpg/test/sql/func.d \
./src/interfaces/ecpg/test/sql/indicators.d \
./src/interfaces/ecpg/test/sql/quote.d \
./src/interfaces/ecpg/test/sql/show.d \
./src/interfaces/ecpg/test/sql/update.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/sql/%.o: ../src/interfaces/ecpg/test/sql/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


