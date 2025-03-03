# KallistiOS environment variable settings. These are the shared pieces
# for the Dreamcast(tm) platform.

# Add the default external DC tools path if it isn't already set.
if [ -z "${DC_TOOLS_BASE}" ] ; then
    export DC_TOOLS_BASE="${KOS_CC_BASE}/../bin"
fi

# Add the external DC tools dir to the path if it is not already.
if ! expr ":$PATH:" : ".*:${DC_TOOLS_BASE}:.*" > /dev/null ; then
  export PATH="${PATH}:${DC_TOOLS_BASE}"
fi

# Default the SH4 floating-point precision if it isn't already set.
# m4-single is used if supported by the current toolchain, otherwise
# m4-single-only is used as a fallback option.
if [ -z "${KOS_SH4_PRECISION}" ] ; then
    if echo 'int main(){}' | ${KOS_CC} -x c -c -o /dev/null - -m4-single 2>/dev/null; then
        export KOS_SH4_PRECISION="-m4-single"
    else
        export KOS_SH4_PRECISION="-m4-single-only"
    fi
fi

export KOS_CFLAGS="${KOS_CFLAGS} ${KOS_SH4_PRECISION} -ml -mfsrra -mfsca -ffunction-sections -fdata-sections -matomic-model=soft-imask -ftls-model=local-exec"
export KOS_AFLAGS="${KOS_AFLAGS} -little"

if [ x${KOS_SUBARCH} = xnaomi ]; then
	export KOS_CFLAGS="${KOS_CFLAGS} -D__NAOMI__"
	export KOS_LDFLAGS="${KOS_LDFLAGS} ${KOS_SH4_PRECISION} -ml -Wl,-Ttext=0x8c020000 -Wl,--gc-sections"
	export KOS_LD_SCRIPT="-T${KOS_BASE}/utils/ldscripts/shlelf-naomi.xc"
else
	export KOS_CFLAGS="${KOS_CFLAGS} -D__DREAMCAST__"
	export KOS_LDFLAGS="${KOS_LDFLAGS} ${KOS_SH4_PRECISION} -ml -Wl,-Ttext=0x8c010000 -Wl,--gc-sections"
	export KOS_LD_SCRIPT="-T${KOS_BASE}/utils/ldscripts/shlelf.xc"
fi

# If we're building for DC, we need the ARM compiler paths as well.
if [ x${KOS_ARCH} = xdreamcast ]; then
	export DC_ARM_CC="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-gcc"
	export DC_ARM_AS="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-as"
	export DC_ARM_AR="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ar"
	export DC_ARM_OBJCOPY="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-objcopy"
	export DC_ARM_LD="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ld"
	export DC_ARM_CFLAGS="-mcpu=arm7di -Wall -O2 -fno-strict-aliasing -Wl,--fix-v4bx -Wa,--fix-v4bx"
	export DC_ARM_AFLAGS="-mcpu=arm7di --fix-v4bx"
	export DC_ARM_MAKE="make"
	export DC_ARM_START="${KOS_ARCH_DIR}/sound/arm/crt0.s"
	export DC_ARM_LDFLAGS="-Wl,-Ttext=0x00000000,-N -nostartfiles -e reset"
	export DC_ARM_LIB_PATHS="-L ${KOS_BASE}/kernel/arch/dreamcast/sound/arm/aicaos"
	export DC_ARM_LIBS="-Wl,--start-group -laicaos -lgcc -Wl,--end-group"
fi
