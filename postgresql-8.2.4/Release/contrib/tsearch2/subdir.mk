################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../contrib/tsearch2/common.c \
../contrib/tsearch2/crc32.c \
../contrib/tsearch2/dict.c \
../contrib/tsearch2/dict_ex.c \
../contrib/tsearch2/dict_ispell.c \
../contrib/tsearch2/dict_snowball.c \
../contrib/tsearch2/dict_syn.c \
../contrib/tsearch2/dict_thesaurus.c \
../contrib/tsearch2/ginidx.c \
../contrib/tsearch2/gistidx.c \
../contrib/tsearch2/prs_dcfg.c \
../contrib/tsearch2/query.c \
../contrib/tsearch2/query_cleanup.c \
../contrib/tsearch2/query_gist.c \
../contrib/tsearch2/query_rewrite.c \
../contrib/tsearch2/query_support.c \
../contrib/tsearch2/query_util.c \
../contrib/tsearch2/rank.c \
../contrib/tsearch2/snmap.c \
../contrib/tsearch2/stopword.c \
../contrib/tsearch2/ts_cfg.c \
../contrib/tsearch2/ts_lexize.c \
../contrib/tsearch2/ts_locale.c \
../contrib/tsearch2/ts_stat.c \
../contrib/tsearch2/tsvector.c \
../contrib/tsearch2/tsvector_op.c \
../contrib/tsearch2/wparser.c \
../contrib/tsearch2/wparser_def.c 

OBJS += \
./contrib/tsearch2/common.o \
./contrib/tsearch2/crc32.o \
./contrib/tsearch2/dict.o \
./contrib/tsearch2/dict_ex.o \
./contrib/tsearch2/dict_ispell.o \
./contrib/tsearch2/dict_snowball.o \
./contrib/tsearch2/dict_syn.o \
./contrib/tsearch2/dict_thesaurus.o \
./contrib/tsearch2/ginidx.o \
./contrib/tsearch2/gistidx.o \
./contrib/tsearch2/prs_dcfg.o \
./contrib/tsearch2/query.o \
./contrib/tsearch2/query_cleanup.o \
./contrib/tsearch2/query_gist.o \
./contrib/tsearch2/query_rewrite.o \
./contrib/tsearch2/query_support.o \
./contrib/tsearch2/query_util.o \
./contrib/tsearch2/rank.o \
./contrib/tsearch2/snmap.o \
./contrib/tsearch2/stopword.o \
./contrib/tsearch2/ts_cfg.o \
./contrib/tsearch2/ts_lexize.o \
./contrib/tsearch2/ts_locale.o \
./contrib/tsearch2/ts_stat.o \
./contrib/tsearch2/tsvector.o \
./contrib/tsearch2/tsvector_op.o \
./contrib/tsearch2/wparser.o \
./contrib/tsearch2/wparser_def.o 

C_DEPS += \
./contrib/tsearch2/common.d \
./contrib/tsearch2/crc32.d \
./contrib/tsearch2/dict.d \
./contrib/tsearch2/dict_ex.d \
./contrib/tsearch2/dict_ispell.d \
./contrib/tsearch2/dict_snowball.d \
./contrib/tsearch2/dict_syn.d \
./contrib/tsearch2/dict_thesaurus.d \
./contrib/tsearch2/ginidx.d \
./contrib/tsearch2/gistidx.d \
./contrib/tsearch2/prs_dcfg.d \
./contrib/tsearch2/query.d \
./contrib/tsearch2/query_cleanup.d \
./contrib/tsearch2/query_gist.d \
./contrib/tsearch2/query_rewrite.d \
./contrib/tsearch2/query_support.d \
./contrib/tsearch2/query_util.d \
./contrib/tsearch2/rank.d \
./contrib/tsearch2/snmap.d \
./contrib/tsearch2/stopword.d \
./contrib/tsearch2/ts_cfg.d \
./contrib/tsearch2/ts_lexize.d \
./contrib/tsearch2/ts_locale.d \
./contrib/tsearch2/ts_stat.d \
./contrib/tsearch2/tsvector.d \
./contrib/tsearch2/tsvector_op.d \
./contrib/tsearch2/wparser.d \
./contrib/tsearch2/wparser_def.d 


# Each subdirectory must supply rules for building sources it contributes
contrib/tsearch2/%.o: ../contrib/tsearch2/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


