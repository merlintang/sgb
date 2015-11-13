################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/tsearch2/snowball/api.c \
../contrib/tsearch2/snowball/english_stem.c \
../contrib/tsearch2/snowball/russian_stem.c \
../contrib/tsearch2/snowball/russian_stem_UTF8.c \
../contrib/tsearch2/snowball/utilities.c 

OBJS += \
./contrib/tsearch2/snowball/api.o \
./contrib/tsearch2/snowball/english_stem.o \
./contrib/tsearch2/snowball/russian_stem.o \
./contrib/tsearch2/snowball/russian_stem_UTF8.o \
./contrib/tsearch2/snowball/utilities.o 

C_DEPS += \
./contrib/tsearch2/snowball/api.d \
./contrib/tsearch2/snowball/english_stem.d \
./contrib/tsearch2/snowball/russian_stem.d \
./contrib/tsearch2/snowball/russian_stem_UTF8.d \
./contrib/tsearch2/snowball/utilities.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/tsearch2/snowball/%.o: ../contrib/tsearch2/snowball/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


