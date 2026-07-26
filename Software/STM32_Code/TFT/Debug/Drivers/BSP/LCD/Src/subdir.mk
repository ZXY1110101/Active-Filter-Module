################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/BSP/LCD/Src/lcd.c 

OBJS += \
./Drivers/BSP/LCD/Src/lcd.o 

C_DEPS += \
./Drivers/BSP/LCD/Src/lcd.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/BSP/LCD/Src/%.o Drivers/BSP/LCD/Src/%.su Drivers/BSP/LCD/Src/%.cyclo: ../Drivers/BSP/LCD/Src/%.c Drivers/BSP/LCD/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DARM_MATH_CM4 -c -I../Core/Inc -I../App/Inc -I../Drivers/BSP/LCD/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Drivers/CMSIS/DSP/Include -I../Drivers/CMSIS/DSP/PrivateInclude -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-BSP-2f-LCD-2f-Src

clean-Drivers-2f-BSP-2f-LCD-2f-Src:
	-$(RM) ./Drivers/BSP/LCD/Src/lcd.cyclo ./Drivers/BSP/LCD/Src/lcd.d ./Drivers/BSP/LCD/Src/lcd.o ./Drivers/BSP/LCD/Src/lcd.su

.PHONY: clean-Drivers-2f-BSP-2f-LCD-2f-Src

