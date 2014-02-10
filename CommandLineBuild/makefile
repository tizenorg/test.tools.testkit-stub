################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init
INCLUDES = -I ../include

RM := rm -rf

# All of the sources participating in the build are defined here
# -include sources.mk
O_SRCS := 
PO_SRCS := 
CPP_SRCS := 
C_UPPER_SRCS := 
C_SRCS := 
S_UPPER_SRCS := 
OBJ_SRCS := 
EDC_SRCS := 
ASM_SRCS := 
CXX_SRCS := 
C++_SRCS := 
CC_SRCS := 
MO_FILES := 
OBJS := 
C++_DEPS := 
C_DEPS := 
CC_DEPS := 
EDJ_FILES := 
CPP_DEPS := 
EXECUTABLES := 
C_UPPER_DEPS := 
CXX_DEPS := 

# Every subdirectory with source files must be described here
SUBDIRS := \
source \

# -include src/subdir.mk
# Add inputs and outputs from these tool invocations to the build variables 
#CPP_SRCS += \
../source/main/main.cpp \
../source/comfun.cpp \
../source/testcase.cpp \
../source/httpserver.cpp \
../source/json/json_reader.cpp \
../source/json/json_value.cpp \
../source/json/json_writer.cpp


OBJS += \
./source/main/main.o \
./source/comfun.o \
./source/testcase.o \
./source/httpserver.o \
./source/json/json_reader.o \
./source/json/json_value.o \
./source/json/json_writer.o


#CPP_DEPS += \
./source/main/main.d \
./source/comfun.d \
./source/testcase.d \
./source/httpserver.d \
./source/json/json_reader.d \
./source/json/json_value.d \
./source/json/json_writer.d


# Each subdirectory must supply rules for building sources it contributes
source/%.o: ../source/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: C++ Compiler'
	$(CC) -I"pch" -D_DEBUG -O0 -g3 -Wall -c -fmessage-length=0 $(INCLUDE_PATH) $(INCLUDES) $(CFLAG) -D_APP_LOG -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# -include objects.mk
USER_OBJS :=

LIBS :=

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: testkit-stub secondary-outputs

init: 
	mkdir -p source/json
	mkdir -p source/main

# Tool invocations
testkit-stub: init $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: C++ Linker'
	$(CC) -o"testkit-stub" $(OBJS) $(USER_OBJS) $(LIBS) $(LFLAG)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) source testkit-stub

secondary-outputs: $(MO_FILES) $(EDJ_FILES)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

