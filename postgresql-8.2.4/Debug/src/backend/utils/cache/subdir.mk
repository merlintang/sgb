################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../src/backend/utils/cache/SUBSYS.o \
../src/backend/utils/cache/catcache.o \
../src/backend/utils/cache/inval.o \
../src/backend/utils/cache/lsyscache.o \
../src/backend/utils/cache/relcache.o \
../src/backend/utils/cache/syscache.o \
../src/backend/utils/cache/typcache.o 

C_SRCS += \
../src/backend/utils/cache/catcache.c \
../src/backend/utils/cache/inval.c \
../src/backend/utils/cache/lsyscache.c \
../src/backend/utils/cache/relcache.c \
../src/backend/utils/cache/syscache.c \
../src/backend/utils/cache/typcache.c 

OBJS += \
./src/backend/utils/cache/catcache.o \
./src/backend/utils/cache/inval.o \
./src/backend/utils/cache/lsyscache.o \
./src/backend/utils/cache/relcache.o \
./src/backend/utils/cache/syscache.o \
./src/backend/utils/cache/typcache.o 

C_DEPS += \
./src/backend/utils/cache/catcache.d \
./src/backend/utils/cache/inval.d \
./src/backend/utils/cache/lsyscache.d \
./src/backend/utils/cache/relcache.d \
./src/backend/utils/cache/syscache.d \
./src/backend/utils/cache/typcache.d 


# Each subdirectory must supply rules for building sources it contributes
src/backend/utils/cache/%.o: ../src/backend/utils/cache/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


