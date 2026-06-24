# Unlock/lock helpers for CH32H417 (dual-core, flash controlled by cpu.0)

proc get_reg_value { data } {
    set reg_str [string trimright $data "\n " ]
    lassign [split $reg_str ":"] reg out
    set hex 0x[string trim $out]
    return $hex
}

proc _unlock_flash_by_reg { TARGET REG } {
    $TARGET mww $REG 0x45670123
    $TARGET mww $REG 0xCDEF89AB
}

proc _unlock_flash { TARGET } {
    set _FLASH_KEYR_ADDR    0x40022004
    _unlock_flash_by_reg $TARGET $_FLASH_KEYR_ADDR
}

proc _unlock_user_ob { TARGET } {
    set _FLASH_OBKEYR_ADDR  0x40022008
    _unlock_flash_by_reg $TARGET $_FLASH_OBKEYR_ADDR
}

set _TARGET             wch_riscv.cpu.0
set FLASH_CTLR_ADDR     0x40022010
set FLASH_STATR_ADDR    0x4002200C
set FLASH_OBR_ADDR      0x4002201C
set RDPR_ADDR           0x1FFFF800

set BSY_BIT_MASK    [expr {1 << 0}]
set EOP_BIT_MASK    [expr {1 << 5}]
set LOCK_BIT_MASK   [expr {1 << 7}]
set OBWRE_BIT_MASK  [expr {1 << 9}]
set RDPRT_BIT_MASK  [expr {1 << 1}]
set OBPG_BIT_MASK   [expr {1 << 4}]
set OBER_BIT_MASK   [expr {1 << 5}]
set STRT_BIT_MASK   [expr {1 << 6}]

set RDPR_MASK_LOCK      0x5aa5
set RDPR_MASK_UNLOCK    0xe339

set TIMEOUT_STEP    10
set TIMEOUT         500

proc unlock { } {
    global _TARGET
    global FLASH_CTLR_ADDR FLASH_STATR_ADDR FLASH_OBR_ADDR RDPR_ADDR
    global BSY_BIT_MASK EOP_BIT_MASK LOCK_BIT_MASK OBWRE_BIT_MASK RDPRT_BIT_MASK
    global OBPG_BIT_MASK
    global RDPR_MASK_LOCK
    global TIMEOUT_STEP TIMEOUT

    set op "UNLOCK"

    set RDPRT_byte [ get_reg_value [ $_TARGET mdb $FLASH_OBR_ADDR ] ]
    if { [ expr {$RDPRT_byte & $RDPRT_BIT_MASK} ] == 0 } {
        echo "** [$op] Memory is unlocked already **"
        return
    }

    set flash_ctrl_data [ get_reg_value [ $_TARGET mdb $FLASH_CTLR_ADDR ] ]
    if { [ expr {$flash_ctrl_data & $LOCK_BIT_MASK} ] == $LOCK_BIT_MASK } {
        echo "** [$op] FLASH_CTLR locked. Releasing ... **"
        _unlock_flash $_TARGET
    }

    set timeout $TIMEOUT
    while { 1 } {
        set flash_statr_data [ get_reg_value [ $_TARGET mdb $FLASH_STATR_ADDR ] ]
        if { [ expr {$flash_statr_data & $BSY_BIT_MASK} ] != $BSY_BIT_MASK } { break }
        if {$timeout == 0} { echo "[$op] !Err 1: Flash busy timeout **"; return }
        incr timeout -$TIMEOUT_STEP
        after $TIMEOUT_STEP
    }

    set flash_ctrl_data [ get_reg_value [ $_TARGET mdw $FLASH_CTLR_ADDR ] ]
    if { [ expr {$flash_ctrl_data & $OBWRE_BIT_MASK} ] == 0 } {
        echo "** [$op] User option bytes locked. Unlocking ... **"
        _unlock_user_ob $_TARGET
    }

    set flash_ctrl_data [ get_reg_value [ $_TARGET mdh $FLASH_CTLR_ADDR ] ]
    $_TARGET mwh $FLASH_CTLR_ADDR [expr {$flash_ctrl_data | $OBPG_BIT_MASK}]

    $_TARGET mwh $RDPR_ADDR $RDPR_MASK_LOCK

    set timeout $TIMEOUT
    while { 1 } {
        set flash_statr_data [ get_reg_value [ $_TARGET mdb $FLASH_STATR_ADDR ] ]
        if {[ expr {$flash_statr_data & $BSY_BIT_MASK} ] != $BSY_BIT_MASK || [expr {$flash_statr_data & $EOP_BIT_MASK}] == $EOP_BIT_MASK} {
            $_TARGET mww $FLASH_STATR_ADDR [expr {$flash_statr_data & ~$EOP_BIT_MASK}]
            break
        }
        if {$timeout == 0} { echo "[$op] !Err 2: Flash operation timeout **"; return }
        incr timeout -$TIMEOUT_STEP
        after $TIMEOUT_STEP
    }

    set result [ get_reg_value [ $_TARGET mdh $RDPR_ADDR ] ]
    if { $result == $RDPR_MASK_LOCK } {
        echo "** [$op] Memory unlocked **"
    } else {
        echo "[$op] !Err 3: Fail memory unlocking **"
    }
}

proc lock { } {
    global _TARGET
    global FLASH_CTLR_ADDR FLASH_STATR_ADDR FLASH_OBR_ADDR RDPR_ADDR
    global BSY_BIT_MASK EOP_BIT_MASK LOCK_BIT_MASK OBWRE_BIT_MASK RDPRT_BIT_MASK
    global OBER_BIT_MASK STRT_BIT_MASK
    global RDPR_MASK_LOCK RDPR_MASK_UNLOCK
    global TIMEOUT_STEP TIMEOUT

    set op "LOCK"

    set RDPRT_byte [ get_reg_value [ $_TARGET mdb $FLASH_OBR_ADDR ] ]
    if { [ expr {$RDPRT_byte & $RDPRT_BIT_MASK} ] == $RDPRT_BIT_MASK } {
        echo "** [$op] Memory is locked already **"
        return
    }

    set flash_ctrl_data [ get_reg_value [ $_TARGET mdb $FLASH_CTLR_ADDR ] ]
    if { [ expr {$flash_ctrl_data & $LOCK_BIT_MASK} ] == $LOCK_BIT_MASK } {
        echo "** [$op] FLASH_CTLR locked. Releasing ... **"
        _unlock_flash $_TARGET
    }

    set timeout $TIMEOUT
    while { 1 } {
        set flash_statr_data [ get_reg_value [ $_TARGET mdh $FLASH_STATR_ADDR ] ]
        if { [ expr {$flash_statr_data & $BSY_BIT_MASK} ] != $BSY_BIT_MASK } { break }
        if {$timeout == 0} { echo "[$op] !Err 1: Flash busy timeout **"; return }
        incr timeout -$TIMEOUT_STEP
        after $TIMEOUT_STEP
    }

    set flash_ctrl_data [ get_reg_value [ $_TARGET mdh $FLASH_CTLR_ADDR ] ]
    if { [ expr {$flash_ctrl_data & $OBWRE_BIT_MASK} ] == 0 } {
        echo "** [$op] User option bytes locked. Unlocking ... **"
        _unlock_user_ob $_TARGET
    }

    $_TARGET mwb $FLASH_CTLR_ADDR $OBER_BIT_MASK
    $_TARGET mwb $FLASH_CTLR_ADDR $STRT_BIT_MASK

    set timeout $TIMEOUT
    while { 1 } {
        set flash_statr_data [ get_reg_value [ $_TARGET mdb $FLASH_STATR_ADDR ] ]
        if {[ expr {$flash_statr_data & $BSY_BIT_MASK} ] != $BSY_BIT_MASK || [expr {$flash_statr_data & $EOP_BIT_MASK}] == $EOP_BIT_MASK} {
            $_TARGET mww $FLASH_STATR_ADDR [expr {$flash_statr_data & ~$EOP_BIT_MASK}]
            break
        }
        if {$timeout == 0} { echo "[$op] !Err 2: Flash operation timeout **"; return }
        incr timeout -$TIMEOUT_STEP
        after $TIMEOUT_STEP
    }

    set result [ get_reg_value [ $_TARGET mdh $RDPR_ADDR ] ]
    if {$result == $RDPR_MASK_UNLOCK} {
        echo "** [$op] Flash successfully locked **"
    } else {
        echo "[$op] !Err 3: Fail flash locking **"
    }

    set flash_statr_data [ get_reg_value [ $_TARGET mdw $FLASH_CTLR_ADDR ] ]
    $_TARGET mww $FLASH_CTLR_ADDR [expr {$flash_statr_data & ~$OBER_BIT_MASK}]
}
