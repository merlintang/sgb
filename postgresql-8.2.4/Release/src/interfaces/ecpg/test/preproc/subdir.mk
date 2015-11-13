################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/interfaces/ecpg/test/preproc/comment.c \
../src/interfaces/ecpg/test/preproc/define.c \
../src/interfaces/ecpg/test/preproc/init.c \
../src/interfaces/ecpg/test/preproc/type.c \
../src/interfaces/ecpg/test/preproc/variable.c \
../src/interfaces/ecpg/test/preproc/whenever.c 

OBJS += \
./src/interfaces/ecpg/test/preproc/comment.o \
./src/interfaces/ecpg/test/preproc/define.o \
./src/interfaces/ecpg/test/preproc/init.o \
./src/interfaces/ecpg/test/preproc/type.o \
./src/interfaces/ecpg/test/preproc/variable.o \
./src/interfaces/ecpg/test/preproc/whenever.o 

C_DEPS += \
./src/interfaces/ecpg/test/preproc/comment.d \
./src/interfaces/ecpg/test/preproc/define.d \
./src/interfaces/ecpg/test/preproc/init.d \
./src/interfaces/ecpg/test/preproc/type.d \
./src/interfaces/ecpg/test/preproc/variable.d \
./src/interfaces/ecpg/test/preproc/whenever.d 


# Each subdirectory must supply rules for building sources it contributes
src/interfaces/ecpg/test/preproc/%.o: ../src/interfaces/ecpg/test/preproc/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


