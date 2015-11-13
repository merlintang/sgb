################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/tsearch2/wordparser/deflex.c \
../contrib/tsearch2/wordparser/parser.c 

OBJS += \
./contrib/tsearch2/wordparser/deflex.o \
./contrib/tsearch2/wordparser/parser.o 

C_DEPS += \
./contrib/tsearch2/wordparser/deflex.d \
./contrib/tsearch2/wordparser/parser.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/tsearch2/wordparser/%.o: ../contrib/tsearch2/wordparser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


