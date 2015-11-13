################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/compat_informix/charfuncs.c \
../src/interfaces/ecpg/test/compat_informix/dec_test.c \
../src/interfaces/ecpg/test/compat_informix/rfmtdate.c \
../src/interfaces/ecpg/test/compat_informix/rfmtlong.c \
../src/interfaces/ecpg/test/compat_informix/rnull.c \
../src/interfaces/ecpg/test/compat_informix/test_informix.c \
../src/interfaces/ecpg/test/compat_informix/test_informix2.c 

OBJS += \
./src/interfaces/ecpg/test/compat_informix/charfuncs.o \
./src/interfaces/ecpg/test/compat_informix/dec_test.o \
./src/interfaces/ecpg/test/compat_informix/rfmtdate.o \
./src/interfaces/ecpg/test/compat_informix/rfmtlong.o \
./src/interfaces/ecpg/test/compat_informix/rnull.o \
./src/interfaces/ecpg/test/compat_informix/test_informix.o \
./src/interfaces/ecpg/test/compat_informix/test_informix2.o 

C_DEPS += \
./src/interfaces/ecpg/test/compat_informix/charfuncs.d \
./src/interfaces/ecpg/test/compat_informix/dec_test.d \
./src/interfaces/ecpg/test/compat_informix/rfmtdate.d \
./src/interfaces/ecpg/test/compat_informix/rfmtlong.d \
./src/interfaces/ecpg/test/compat_informix/rnull.d \
./src/interfaces/ecpg/test/compat_informix/test_informix.d \
./src/interfaces/ecpg/test/compat_informix/test_informix2.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/compat_informix/%.o: ../src/interfaces/ecpg/test/compat_informix/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


