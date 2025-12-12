# KallistiOS environment variable settings. These are the shared pieces
# for the Dreamcast(tm) platform.

# Add the default subarch (DC) if one hasn't already been set.
if [ -z "${KOS_SUBARCH}" ] ; then
    export KOS_SUBARCH="pristine"
fi

# Add the default external DC tools path if it isn't already set.
if [ -z "${DC_TOOLS_BASE}" ] ; then
    export DC_TOOLS_BASE="${KOS_CC_BASE}/../bin"
fi

# Add the external DC tools dir to the path if it is not already.
if ! expr ":$PATH:" : ".*:${DC_TOOLS_BASE}:.*" > /dev/null ; then
  export PATH="${PATH}:${DC_TOOLS_BASE}"
fi

export KOS_CFLAGS="${KOS_CFLAGS} -mcpu=arm7di -ffunction-sections -fdata-sections -ftls-model=local-exec"
#export KOS_AFLAGS="${KOS_AFLAGS} -little"
export KOS_LDFLAGS="${KOS_LDFLAGS} -Wl,--gc-sections -Wl,--wrap=puts"
export KOS_LD_SCRIPT="-T${KOS_BASE}/utils/ldscripts/armeabi.xc"

export KOS_GDB_CPU=arm
