################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../src/backend/port/tas/dummy.s \
../src/backend/port/tas/hpux_hppa.s \
../src/backend/port/tas/solaris_sparc.s \
../src/backend/port/tas/solaris_x86.s 

OBJS += \
./src/backend/port/tas/dummy.o \
./src/backend/port/tas/hpux_hppa.o \
./src/backend/port/tas/solaris_sparc.o \
./src/backend/port/tas/solaris_x86.o 


# Each subdirectory must supply rules for building sources it contributes
src/backend/port/tas/%.o: ../src/backend/port/tas/%.s
	@echo 'Building file: $<'
	@echo 'Invoking: GCC Assembler'
	as  -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


