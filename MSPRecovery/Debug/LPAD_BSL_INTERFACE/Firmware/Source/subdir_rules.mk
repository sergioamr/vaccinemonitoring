################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
LPAD_BSL_INTERFACE/Firmware/Source/lpad_bsl_int.obj: ../LPAD_BSL_INTERFACE/Firmware/Source/lpad_bsl_int.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"/opt/ti/ccsv6/tools/compiler/ti-cgt-msp430_4.4.4/bin/cl430" -vmsp --abi=eabi --use_hw_mpy=none --include_path="/opt/ti/ccsv6/ccs_base/msp430/include" --include_path="/opt/ti/ccsv6/tools/compiler/ti-cgt-msp430_4.4.4/include" --advice:power=all -g --define=__MSP430G2533__ --diag_warning=225 --display_error_number --diag_wrap=off --printf_support=minimal --preproc_with_compile --preproc_dependency="LPAD_BSL_INTERFACE/Firmware/Source/lpad_bsl_int.pp" --obj_directory="LPAD_BSL_INTERFACE/Firmware/Source" $(GEN_OPTS__FLAG) "$(shell echo $<)"
	@echo 'Finished building: $<'
	@echo ' '


