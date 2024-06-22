/*++

Copyright (c) 2024 Mario Bălănică <mariobalanica02@gmail.com>
Copyright (c) Microsoft Corporation. All rights reserved.

SPDX-License-Identifier: MIT

Module Name:

    dwcmshc.h

Abstract:

    Header for the host controller implementation.

Environment:

    Kernel mode only.

--*/

#pragma once

//
// Allocation tag
//
#define MSHC_ALLOC_TAG              ULONG('chSM')

//
// Memory registers
//
#define MSHC_CTRL               0x0000  // Control register
#define MSHC_PWREN              0x0004  // Power enable register
#define MSHC_CLKDIV             0x0008  // Clock divider register
#define MSHC_CLKSRC             0x000c  // SD clock source register
#define MSHC_CLKENA             0x0010  // Clock enable register
#define MSHC_TMOUT              0x0014  // Timeout register
#define MSHC_CTYPE              0x0018  // Card type register
#define MSHC_BLKSIZ             0x001c  // Block size register
#define MSHC_BYTCNT             0x0020  // Byte count register
#define MSHC_INTMASK            0x0024  // Interrupt mask register
#define MSHC_CMDARG             0x0028  // Command argument register
#define MSHC_CMD                0x002c  // Command register
#define MSHC_RESP0              0x0030  // Response register 0
#define MSHC_RESP1              0x0034  // Response register 1
#define MSHC_RESP2              0x0038  // Response register 2
#define MSHC_RESP3              0x003c  // Response register 3
#define MSHC_MINTSTS            0x0040  // Masked interrupt status register
#define MSHC_RINTSTS            0x0044  // Raw interrupt status register
#define MSHC_STATUS             0x0048  // Status register
#define MSHC_FIFOTH             0x004c  // FIFO threshold register
#define MSHC_CDETECT            0x0050  // Card detect register
#define MSHC_WRTPRT             0x0054  // Write protect register
#define MSHC_GPIO               0x0058  // GPIO register
#define MSHC_TCBCNT             0x005c  // Transferred card byte count register
#define MSHC_TBBCNT             0x0060  // Transferred host to FIFO byte count register
#define MSHC_DEBNCE             0x0064  // Debounce count register
#define MSHC_USRID              0x0068  // User ID register
#define MSHC_VERID              0x006c  // Version ID register
#define MSHC_HCON               0x0070  // Hardware configuration register
#define MSHC_RSTN               0x0078  // Hardware reset register
#define MSHC_UHS_REG            0x0074  // UHS-1 control register
#define MSHC_BMOD               0x0080  // Bus mode register
#define MSHC_PLDMND             0x0084  // Poll demand register
#define MSHC_DBADDR             0x0088  // Descriptor list base address register
#define MSHC_IDSTS              0x008c  // Internal DMAC status register
#define MSHC_IDINTEN            0x0090  // Internal DMAC interrupt enable register
#define MSHC_DSCADDR            0x0094  // Current host descriptor address register
#define MSHC_BUFADDR            0x0098  // Current buffer descriptor address register
#define MSHC_CARDTHRCTL         0x0100  // Card threshold control register
#define MSHC_BACKEND_POWER      0x0104  // Back-end power register
#define MSHC_UHS_REG_EXT        0x0108  // UHS-1 extended control register
#define MSHC_EMMCDDR_REG        0x010c  // eMMC4.5 DDR start bit detection control register
#define MSHC_RDYINT_GEN         0x0120  // Card ready interrupt generation control register
//
// 64-bit IDMAC support
//
#define MSHC_DBADDRL            0x0088  // Descriptor list lower base address register (64-bit IDMAC only)
#define MSHC_DBADDRU            0x008c  // Descriptor list upper base address register (64-bit IDMAC only)
#define MSHC_IDSTS64            0x0090  // Internal DMAC status register (64-bit IDMAC only)
#define MSHC_IDINTEN64          0x0094  // Internal DMAC interrupt enable register (64-bit IDMAC only)
#define MSHC_DSCADDRL           0x0098  // Current host descriptor lower address register (64-bit IDMAC only)
#define MSHC_DSCADDRU           0x009c  // Current host descriptor upper address register (64-bit IDMAC only)
#define MSHC_BUFADDRL           0x00A0  // Current buffer descriptor lower address register (64-bit IDMAC only)
#define MSHC_BUFADDRU           0x00A4  // Current buffer descriptor upper address register (64-bit IDMAC only)
//
// FIFO
//
#define MSHC_FIFO_LEGACY_BASE   0x0100  // FIFO base address register (< MSHC_VERID_240A)
#define MSHC_FIFO_BASE          0x0200  // FIFO base address register (>= MSHC_VERID_240A)

//
// MSHC_CTRL fields
//
#define MSHC_CTRL_USE_INTERNAL_DMAC                 (0x1 << 25)
#define MSHC_CTRL_CEATA_DEVICE_INTERRUPT_STATUS     (0x1 << 11)
#define MSHC_CTRL_SEND_AUTO_STOP_CCSD               (0x1 << 10)
#define MSHC_CTRL_SEND_CCSD                         (0x1 << 9)
#define MSHC_CTRL_ABORT_READ_DATA                   (0x1 << 8)
#define MSHC_CTRL_SEND_IRQ_RESPONSE                 (0x1 << 7)
#define MSHC_CTRL_READ_WAIT                         (0x1 << 6)
#define MSHC_CTRL_DMA_ENABLE                        (0x1 << 5)
#define MSHC_CTRL_INT_ENABLE                        (0x1 << 4)
#define MSHC_CTRL_DMA_RESET                         (0x1 << 2)
#define MSHC_CTRL_FIFO_RESET                        (0x1 << 1)
#define MSHC_CTRL_CONTROLLER_RESET                  (0x1 << 0)
#define MSHC_CTRL_RESET_ALL                         (MSHC_CTRL_DMA_RESET            | \
                                                     MSHC_CTRL_FIFO_RESET           | \
                                                     MSHC_CTRL_CONTROLLER_RESET)
//
// MSHC_PWREN fields
//
#define MSHC_PWREN_POWER_ENABLE(n)                  (0x1 << (n))

//
// MSHC_CLKDIV fields
//
#define MSHC_CLKDIV_CLK_DIVIDER0_SHIFT              0
#define MSHC_CLKDIV_CLK_DIVIDER0_MASK               (0xff < 0)

//
// MSHC_CLKSRC fields
//
#define MSHC_CLKSRC_CLK_SOURCE0_SHIFT               0
#define MSHC_CLKSRC_CLK_SOURCE0_MASK                (0x3 << 0)

//
// MSHC_CLKENA fields
//
#define MSHC_CLKENA_CCLK_LOW_POWER(n)               (0x1 << (16 + (n)))
#define MSHC_CLKENA_CCLK_ENABLE(n)                  (0x1 << (n))

//
// MSHC_TMOUT fields
//
#define MSHC_TMOUT_DATA_TIMEOUT_SHIFT               8
#define MSHC_TMOUT_DATA_TIMEOUT_MASK                (0xffffff << 8)
#define MSHC_TMOUT_RESPONSE_TIMEOUT_SHIFT           0
#define MSHC_TMOUT_RESPONSE_TIMEOUT_MASK            (0xff << 0)

//
// MSHC_CTYPE fields
//
#define MSHC_CTYPE_CARD_WIDTH_MASK                  0x00010001
#define MSHC_CTYPE_CARD_WIDTH_8                     (0x1 << 16)
#define MSHC_CTYPE_CARD_WIDTH_4                     (0x1 << 0)
#define MSHC_CTYPE_CARD_WIDTH_1                     0

//
// MSHC_INTMASK/MINTSTS/RINTSTS fields
//
#define MSHC_INT_SDIO(n)                            (0x1 << (16 + (n)))   // SDIO slot interrupt
#define MSHC_INT_EBE                                (0x1 << 15)           // End-bit error (read)/Write no CRC
#define MSHC_INT_ACD                                (0x1 << 14)           // Auto command done
#define MSHC_INT_SBE                                (0x1 << 13)           // Start-bit error
#define MSHC_INT_HLE                                (0x1 << 12)           // Hardware locked write error
#define MSHC_INT_FRUN                               (0x1 << 11)           // FIFO underrun/overrun error
#define MSHC_INT_HTO                                (0x1 << 10)           // Data starvation-by-host timeout /Volt_switch_int
#define MSHC_INT_DRTO                               (0x1 << 9)            // Data read timeout
#define MSHC_INT_RTO                                (0x1 << 8)            // Response timeout
#define MSHC_INT_DCRC                               (0x1 << 7)            // Data CRC error
#define MSHC_INT_RCRC                               (0x1 << 6)            // Response CRC error
#define MSHC_INT_RXDR                               (0x1 << 5)            // Receive FIFO data request
#define MSHC_INT_TXDR                               (0x1 << 4)            // Transmit FIFO data request
#define MSHC_INT_DTO                                (0x1 << 3)            // Data transfer over
#define MSHC_INT_CMD                                (0x1 << 2)            // Command done
#define MSHC_INT_RE                                 (0x1 << 1)            // Response error
#define MSHC_INT_CD                                 (0x1 << 0)            // Card detect

#define MSHC_INT_DATA_ERROR                         (MSHC_INT_EBE   | \
                                                     MSHC_INT_SBE   | \
                                                     MSHC_INT_FRUN  | \
                                                     MSHC_INT_HTO   | \
                                                     MSHC_INT_DRTO  | \
                                                     MSHC_INT_DCRC)

#define MSHC_INT_CMD_ERROR                          (MSHC_INT_HLE   | \
                                                     MSHC_INT_RTO   | \
                                                     MSHC_INT_RCRC  | \
                                                     MSHC_INT_RE)

#define MSHC_INT_ERROR                              (MSHC_INT_DATA_ERROR | MSHC_INT_CMD_ERROR)

#define MSHC_INT_ALL                                0xffffffff

//
// MSHC_CMD fields
//
#define MSHC_CMD_START_CMD                          (0x1 << 31)
#define MSHC_CMD_USE_HOLD_REG                       (0x1 << 29)
#define MSHC_CMD_VOLT_SWITCH                        (0x1 << 28)
#define MSHC_CMD_BOOT_MODE                          (0x1 << 27)
#define MSHC_CMD_DISABLE_BOOT                       (0x1 << 26)
#define MSHC_CMD_EXPECT_BOOT_ACK                    (0x1 << 25)
#define MSHC_CMD_ENABLE_BOOT                        (0x1 << 24)
#define MSHC_CMD_CCS_EXPECTED                       (0x1 << 23)
#define MSHC_CMD_READ_CEATA_DEVICE                  (0x1 << 22)
#define MSHC_CMD_UPDATE_CLOCK_REGS_ONLY             (0x1 << 21)
#define MSHC_CMD_SEND_INITIALIZATION                (0x1 << 15)
#define MSHC_CMD_STOP_ABORT_CMD                     (0x1 << 14)
#define MSHC_CMD_WAIT_PRVDATA_COMPLETE              (0x1 << 13)
#define MSHC_CMD_SEND_AUTO_STOP                     (0x1 << 12)
#define MSHC_CMD_TRANSFER_MODE                      (0x1 << 11)
#define MSHC_CMD_WR                                 (0x1 << 10)
#define MSHC_CMD_DATA_EXPECTED                      (0x1 << 9)
#define MSHC_CMD_CHECK_RESPONSE_CRC                 (0x1 << 8)
#define MSHC_CMD_RESPONSE_LENGTH                    (0x1 << 7)
#define MSHC_CMD_RESPONSE_EXPECT                    (0x1 << 6)
#define MSHC_CMD_INDEX_SHIFT                        0
#define MSHC_CMD_INDEX_MASK                         (0x3f << 0)

//
// MSHC_STATUS fields
//
#define MSHC_STATUS_DMA_REQ                         (0x1 << 31)
#define MSHC_STATUS_DMA_ACK                         (0x1 << 30)
#define MSHC_STATUS_FIFO_COUNT_SHIFT                17
#define MSHC_STATUS_FIFO_COUNT_MASK                 (0x1fff << 17)
#define MSHC_STATUS_RESPONSE_INDEX_SHIFT            11
#define MSHC_STATUS_RESPONSE_INDEX_MASK             (0x3f << 11)
#define MSHC_STATUS_DATA_STATE_MC_BUSY              (0x1 << 10)
#define MSHC_STATUS_DATA_BUSY                       (0x1 << 9)
#define MSHC_STATUS_DATA_3_STATUS                   (0x1 << 8)
#define MSHC_STATUS_COMMAND_FSM_STATES_SHIFT        4
#define MSHC_STATUS_COMMAND_FSM_STATES_MASK         (0xf << 4)
#define MSHC_STATUS_FIFO_FULL                       (0x1 << 3)
#define MSHC_STATUS_FIFO_EMPTY                      (0x1 << 2)
#define MSHC_STATUS_FIFO_TX_WATERMARK               (0x1 << 1)
#define MSHC_STATUS_FIFO_RX_WATERMARK               (0x1 << 0)

//
// MSHC_FIFOTH fields
//
#define MSHC_FIFOTH_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT     28
#define MSHC_FIFOTH_DMA_MULTIPLE_TRANSACTION_SIZE_MASK      (0x7 << 28)
#define MSHC_FIFOTH_RX_WMARK_SHIFT                          16
#define MSHC_FIFOTH_RX_WMARK_MASK                           (0xfff << 16)
#define MSHC_FIFOTH_TX_WMARK_SHIFT                          0
#define MSHC_FIFOTH_TX_WMARK_MASK                           (0xfff << 0)

//
// MSHC_CDETECT fields
//
#define MSHC_CDETECT_CARD_DETECT_N(n)               (0x1 << (n))

//
// MSHC_WRTPRT fields
//
#define MSHC_WRTPRT_WRITE_PROTECT(n)                (0x1 << (n))

//
// MSHC_DEBNCE fields
//
#define MSHC_DEBNCE_DEBOUNCE_COUNT_SHIFT            0
#define MSHC_DEBNCE_DEBOUNCE_COUNT_MASK             (0xffffff << 0)

//
// MSHC_VERID fields
//
#define MSHC_VERID_240A                             0x5342240a
#define MSHC_VERID_270A                             0x5342270a
#define MSHC_VERID_280A                             0x5342280a
#define MSHC_VERID_CORE_ID_SHIFT                    16
#define MSHC_VERID_CORE_ID_MASK                     (0xffff << 16)
#define MSHC_VERID_RELEASE_SHIFT                    0
#define MSHC_VERID_RELEASE_MASK                     (0xffff << 0)

//
// MSHC_HCON fields
//
#define MSHC_HCON_ADDR_CONFIG_IDMAC64               (0x1 << 27)
#define MSHC_HCON_AREA_OPTIMIZED                    (0x1 << 26)
#define MSHC_HCON_NUM_CLK_DIV_SHIFT                 24
#define MSHC_HCON_NUM_CLK_DIV_MASK                  (0x3 << 24)
#define MSHC_HCON_SET_CLK_FALSE_PATH                (0x1 << 23)
#define MSHC_HCON_IMPL_HOLD_REG                     (0x1 << 22)
#define MSHC_HCON_FIFO_RAM_INSIDE                   (0x1 << 21)
#define MSHC_HCON_GE_DMA_DATA_WIDTH_SHIFT           18
#define MSHC_HCON_GE_DMA_DATA_WIDTH_MASK            (0x7 << 18)
#define MSHC_HCON_GE_DMA_DATA_WIDTH_16_BITS         0x0
#define MSHC_HCON_GE_DMA_DATA_WIDTH_32_BITS         0x1
#define MSHC_HCON_GE_DMA_DATA_WIDTH_64_BITS         0x2
#define MSHC_HCON_DMA_INTERFACE_SHIFT               16
#define MSHC_HCON_DMA_INTERFACE_MASK                (0x3 << 16)
#define MSHC_HCON_DMA_INTERFACE_IDMAC               0x0
#define MSHC_HCON_DMA_INTERFACE_DW_DMA              0x1
#define MSHC_HCON_DMA_INTERFACE_GENERIC_DMA         0x2
#define MSHC_HCON_DMA_INTERFACE_NON_DW_DMA          0x3
#define MSHC_HCON_H_ADDR_WIDTH_SHIFT                10
#define MSHC_HCON_H_ADDR_WIDTH_MASK                 (0x3f << 10)
#define MSHC_HCON_H_DATA_WIDTH_SHIFT                7
#define MSHC_HCON_H_DATA_WIDTH_MASK                 (0x7 << 7)
#define MSHC_HCON_H_DATA_WIDTH_16_BITS              0x0
#define MSHC_HCON_H_DATA_WIDTH_32_BITS              0x1
#define MSHC_HCON_H_DATA_WIDTH_64_BITS              0x2
#define MSHC_HCON_H_BUS_TYPE                        (0x1 << 6)
#define MSHC_HCON_H_BUS_TYPE_APB                    0x0
#define MSHC_HCON_H_BUS_TYPE_AHB                    0x1
#define MSHC_HCON_CARD_NUM_SHIFT                    1
#define MSHC_HCON_CARD_NUM_MASK                     (0x1f << 1)
#define MSHC_HCON_CARD_TYPE                         (0x1 << 0)
#define MSHC_HCON_CARD_TYPE_MMC_ONLY                0x0
#define MSHC_HCON_CARD_TYPE_SD_MMC                  0x1

//
// MSHC_UHS_REG fields
//
#define MSHC_UHS_REG_DDR_REG(n)                     (0x1 << (16 + (n)))
#define MSHC_UHS_REG_VOLT_REG(n)                    (0x1 << (n))

//
// MSHC_RSTN fields
//
#define MSHC_RSTN_CARD_RESET(n)                     (0x1 << (n))

//
// MSHC_BMOD fields
//
#define MSHC_BMOD_PBL_SHIFT                         8
#define MSHC_BMOD_PBL_MASK                          (0x7 << 8)
#define MSHC_BMOD_DE                                (0x1 << 7)
#define MSHC_BMOD_DSL_SHIFT                         2
#define MSHC_BMOD_DSL_MASK                          (0x1f << 2)
#define MSHC_BMOD_FB                                (0x1 << 1)
#define MSHC_BMOD_SWR                               (0x1 << 0)

//
// MSHC_IDSTS fields
//
#define MSHC_IDSTS_FSM_SHIFT                        13
#define MSHC_IDSTS_FSM_MASK                         (0xf << 13)
#define MSHC_IDSTS_FSM_DMA_IDLE                     0x0
#define MSHC_IDSTS_FSM_DMA_SUSPEND                  0x1
#define MSHC_IDSTS_FSM_DESC_RD                      0x2
#define MSHC_IDSTS_FSM_DESC_CHK                     0x3
#define MSHC_IDSTS_DMA_RD_REQ_WAIT                  0x4
#define MSHC_IDSTS_DMA_WR_REQ_WAIT                  0x5
#define MSHC_IDSTS_DMA_RD                           0x6
#define MSHC_IDSTS_DMA_WR                           0x7
#define MSHC_IDSTS_DESC_CLOSE                       0x8
#define MSHC_IDSTS_EB_SHIFT                         10
#define MSHC_IDSTS_EB_MASK                          (0x3 << 10)

//
// MSHC_IDSTS/IDINTEN [9:0] fields
//
#define MSHC_IDINT_AIS                              (0x1 << 9)
#define MSHC_IDINT_NIS                              (0x1 << 8)
#define MSHC_IDINT_CES                              (0x1 << 5)
#define MSHC_IDINT_DUI                              (0x1 << 4)
#define MSHC_IDINT_FBE                              (0x1 << 2)
#define MSHC_IDINT_RI                               (0x1 << 1)
#define MSHC_IDINT_TI                               (0x1 << 0)

#define MSHC_IDINT_ALL                              (MSHC_IDINT_AIS | \
                                                     MSHC_IDINT_NIS | \
                                                     MSHC_IDINT_CES | \
                                                     MSHC_IDINT_DUI | \
                                                     MSHC_IDINT_FBE | \
                                                     MSHC_IDINT_RI  | \
                                                     MSHC_IDINT_TI)

//
// MSHC_CARDTHRCTL fields
//
#define MSHC_CARDTHRCTL_CARD_RD_THRES_SHIFT         16
#define MSHC_CARDTHRCTL_CARD_RD_THRES_MASK          (0xfff << 16)
#define MSHC_CARDTHRCTL_CARD_WR_THRES_EN            (0x1 << 2)
#define MSHC_CARDTHRCTL_BUSY_CLR_INT_EN             (0x1 << 1)
#define MSHC_CARDTHRCTL_CARD_RD_THRES_EN            (0x1 << 0)

//
// MSHC_BACKEND_POWER fields
//
#define MSHC_BACKEND_POWER_ENABLE(n)                (0x1 << (n))

//
// MSHC_EMMCDDR_REG fields
//
#define MSHC_EMMCDDR_REG_HALF_START_BIT             (0x1 << 0)

//
// MSHC_RDYINT_GEN fields
//
#define MSHC_RDYINT_GEN_CNT_FINISH                  (0x1 << 24)
#define MSHC_RDYINT_GEN_CNT_STATUS_SHIFT            16
#define MSHC_RDYINT_GEN_CNT_STATUS_MASK             (0xff << 16)
#define MSHC_RDYINT_GEN_WORKING                     (0x1 << 8)
#define MSHC_RDYINT_GEN_MAXVAL_SHIFT                0
#define MSHC_RDYINT_GEN_MAXVAL_MASK                 (0xff << 0)

//
// MSHC IDMAC fields
//
#define MSHC_IDMAC_DES0_OWN                         (0x1U << 31)
#define MSHC_IDMAC_DES0_CES                         (0x1 << 30)
#define MSHC_IDMAC_DES0_ER                          (0x1 << 5)
#define MSHC_IDMAC_DES0_CH                          (0x1 << 4)
#define MSHC_IDMAC_DES0_FS                          (0x1 << 3)
#define MSHC_IDMAC_DES0_LD                          (0x1 << 2)
#define MSHC_IDMAC_DES0_DIC                         (0x1 << 1)

#define MSHC_IDMAC_DES1_BS2_SHIFT                   13
#define MSHC_IDMAC_DES1_BS2_MASK                    (0x1fff << 13)
#define MSHC_IDMAC_DES1_BS1_SHIFT                   0
#define MSHC_IDMAC_DES1_BS1_MASK                    (0x1fff << 0)

#pragma pack(1)

typedef struct _MSHC_IDMAC_DESCRIPTOR {
    ULONG Des0; // Control & Status register
    ULONG Des1; // Buffer size
    ULONG Des2; // Buffer address
    ULONG Des3; // Next descriptor address
} MSHC_IDMAC_DESCRIPTOR, *PMSHC_IDMAC_DESCRIPTOR;

#pragma pack()

//
// Maximum buffer size per chained descriptor entry
//
#define MSHC_IDMAC_MAX_LENGTH_PER_ENTRY     0x1000

#define MSHC_OPTIMAL_FIFO_THRESHOLD(_Depth)  ((_Depth) / 2)

#define MSHC_DCRC_ERROR_RETUNING_THRESHOLD  10

//
// Standard SD/MMC commands
//
#define SDCMD_GO_IDLE_STATE             0
#define SDCMD_SEND_RELATIVE_ADDR        3
#define SDCMD_VOLTAGE_SWITCH            11
#define SDCMD_STOP_TRANSMISSION         12
#define SDCMD_SEND_STATUS               13
#define SDCMD_GO_INACTIVE_STATE         15
#define SDCMD_SEND_TUNING_BLOCK         19
#define SDCMD_EMMC_SEND_TUNING_BLOCK    21

typedef struct _MSHC_EXTENSION MSHC_EXTENSION, *PMSHC_EXTENSION;

//
// Platform-specific abstractions (callbacks and info)
//

typedef
NTSTATUS
MSHC_PLATFORM_SET_CLOCK(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG FrequencyKhz
    );
typedef MSHC_PLATFORM_SET_CLOCK *PMSHC_PLATFORM_SET_CLOCK;

typedef
NTSTATUS
MSHC_PLATFORM_SET_VOLTAGE(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_VOLTAGE Voltage
    );
typedef MSHC_PLATFORM_SET_VOLTAGE *PMSHC_PLATFORM_SET_VOLTAGE;

typedef
NTSTATUS
MSHC_PLATFORM_SET_SIGNALING_VOLTAGE(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_SIGNALING_VOLTAGE Voltage
    );
typedef MSHC_PLATFORM_SET_SIGNALING_VOLTAGE *PMSHC_PLATFORM_SET_SIGNALING_VOLTAGE;

typedef
NTSTATUS
MSHC_PLATFORM_EXECUTE_TUNING(
    _In_ PMSHC_EXTENSION MshcExtension
    );
typedef MSHC_PLATFORM_EXECUTE_TUNING *PMSHC_PLATFORM_EXECUTE_TUNING;

typedef struct _MSHC_PLATFORM_OPERATIONS {
    PMSHC_PLATFORM_SET_CLOCK SetClock;
    PMSHC_PLATFORM_SET_VOLTAGE SetVoltage;
    PMSHC_PLATFORM_SET_SIGNALING_VOLTAGE SetSignalingVoltage;
    PMSHC_PLATFORM_EXECUTE_TUNING ExecuteTuning;
} MSHC_PLATFORM_OPERATIONS, *PMSHC_PLATFORM_OPERATIONS;

typedef struct _MSHC_PLATFORM_CAPABILITIES {
    ULONG ClockFrequency;
    ULONG MaxFrequency;
    ULONG FifoDepth;
    ULONG BusWidth;
    ULONG SupportHighSpeed;
    ULONG SupportDdr50;
    ULONG SupportSdr50;
    ULONG SupportSdr104;
    ULONG SupportHs200;
    ULONG No18vRegulator;
} MSHC_PLATFORM_CAPABILITIES, *PMSHC_PLATFORM_CAPABILITIES;

typedef enum _MSHC_PLATFORM_TYPE {
    MshcPlatformUnknown = 0,
    MshcPlatformRockchip = 1
} MSHC_PLATFORM_TYPE, *PMSHC_PLATFORM_TYPE;

typedef struct _MSHC_PLATFORM_INFO {
    MSHC_PLATFORM_TYPE Type;
    MSHC_PLATFORM_CAPABILITIES Capabilities;
} MSHC_PLATFORM_INFO, *PMSHC_PLATFORM_INFO;

typedef struct _MSHC_PLATFORM_INFO_LIST_ENTRY {
    LIST_ENTRY ListEntry;
    PDEVICE_OBJECT MiniportFdo;
    MSHC_PLATFORM_INFO PlatformInfo;
} MSHC_PLATFORM_INFO_LIST_ENTRY, *PMSHC_PLATFORM_INFO_LIST_ENTRY;

typedef struct _MSHC_PLATFORM_DRIVER_DATA {
    MSHC_PLATFORM_INFO Info;
    MSHC_PLATFORM_OPERATIONS Operations;
} MSHC_PLATFORM_DRIVER_DATA, *PMSHC_PLATFORM_DRIVER_DATA;

//
// Slot extension
//
struct _MSHC_EXTENSION {
    BOOLEAN Initialized;

    //
    // WPP tracing
    //
    RECORDER_LOG LogHandle;
    BOOLEAN BreakOnError;

    //
    // Controller info
    //
    PVOID PhysicalAddress;
    PVOID BaseAddress;
    ULONG CoreVersion;
    SDPORT_CAPABILITIES Capabilities;
    PMSHC_PLATFORM_INFO PlatformInfo;
    PMSHC_PLATFORM_OPERATIONS PlatformOperations;

    //
    // Slot location
    //
    ULONG SlotId;

    //
    // Current bus configuration
    //
    BOOLEAN CardInitialized;
    BOOLEAN CardPowerControlSupported;
    USHORT CardRca;
    SDPORT_SIGNALING_VOLTAGE SignalingVoltage;
    SDPORT_BUS_SPEED BusSpeed;
    SDPORT_BUS_WIDTH BusWidth;
    ULONG BusFrequencyKhz;

    //
    // Current request state
    //
    PSDPORT_REQUEST OutstandingRequest;
    ULONG CurrentEvents;
    ULONG CurrentErrors;
    ULONG CurrentTransferRemainingLength;

    //
    // Local request state (issued by the miniport)
    //
    KDPC LocalRequestDpc;
    LONG LocalRequestPending;

    //
    // FIFO configuration for the current transfer
    //
    ULONG FifoThreshold;
    ULONG FifoDmaBurstSize;

    //
    // Work item for completing lengthy commands with busy signaling
    //
    PIO_WORKITEM CompleteRequestBusyWorkItem;

    //
    // Tuning state
    //
    BOOLEAN TuningPerformed;
    ULONG DataCrcErrorsSinceLastTuning;
};

#define MINIPORT_FROM_SLOTEXT(SlotExtension) \
    (CONTAINING_RECORD(SlotExtension, SDPORT_SLOT_EXTENSION, PrivateExtension)->Miniport)


extern "C" DRIVER_INITIALIZE DriverEntry;

//
// sdport DDI callbacks
//

SDPORT_GET_SLOT_COUNT MshcGetSlotCount;
SDPORT_GET_SLOT_CAPABILITIES MshcGetSlotCapabilities;
SDPORT_INITIALIZE MshcSlotInitialize;
SDPORT_ISSUE_BUS_OPERATION MshcSlotIssueBusOperation;
SDPORT_GET_CARD_DETECT_STATE MshcSlotGetCardDetectState;
SDPORT_GET_WRITE_PROTECT_STATE MshcSlotGetWriteProtectState;
SDPORT_INTERRUPT MshcSlotInterrupt;
SDPORT_ISSUE_REQUEST MshcSlotIssueRequest;
SDPORT_GET_RESPONSE MshcSlotGetResponse;
SDPORT_TOGGLE_EVENTS MshcSlotToggleEvents;
SDPORT_CLEAR_EVENTS MshcSlotClearEvents;
SDPORT_REQUEST_DPC MshcRequestDpc;
SDPORT_SAVE_CONTEXT MshcSaveContext;
SDPORT_RESTORE_CONTEXT MshcRestoreContext;
SDPORT_PO_FX_POWER_CONTROL_CALLBACK MshcPoFxPowerControlCallback;
SDPORT_CLEANUP MshcCleanup;

//
// Bus operation routines
//

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcResetHost(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_RESET_TYPE ResetType
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSetVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_VOLTAGE Voltage
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSetClock(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG FrequencyKhz
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSetBusWidth(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_WIDTH BusWidth
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSetBusSpeed(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_BUS_SPEED Speed
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSetSignalingVoltage(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ SDPORT_SIGNALING_VOLTAGE Voltage
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcExecuteTuning(
    _In_ PMSHC_EXTENSION MshcExtension
    );

//
// Interrupt helper routines
//

VOID
MshcEnableGlobalInterrupt(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ BOOLEAN Enable
    );

VOID
MshcEnableInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    );

VOID
MshcDisableInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    );

VOID
MshcAcknowledgeInterrupts(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Intsts,
    _In_ ULONG Idsts
    );

VOID
MshcConvertIntStatusToStandardEvents(
    _In_ ULONG Intsts,
    _In_ ULONG Idsts,
    _Out_ PULONG Events,
    _Out_ PULONG Errors
    );

VOID
MshcConvertStandardEventsToIntStatus(
    _In_ ULONG Events,
    _In_ ULONG Errors,
    _Out_ PULONG Intsts,
    _Out_ PULONG Idsts
    );

NTSTATUS
MshcConvertStandardErrorToStatus(
    _In_ ULONG Error
    );

//
// Request helper routines
//

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcSendCommand(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcSetTransferMode(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcBuildTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcStartTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcBuildPioTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcBuildDmaTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcStartPioTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
MshcStartDmaTransfer(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
MshcCompleteRequest(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request,
    _In_ NTSTATUS Status
    );

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
MshcCompleteRequestBusyWorker(
    _In_opt_ PDEVICE_OBJECT DeviceObject,
    _In_ PVOID Context
    );

_IRQL_requires_(DISPATCH_LEVEL)
NTSTATUS
MshcCreateIdmacDescriptorTable(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcIssueRequestSynchronously(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ PSDPORT_REQUEST Request,
    _In_ ULONG TimeoutUs,
    _In_ BOOLEAN ErrorRecovery
    );

_IRQL_requires_(DISPATCH_LEVEL)
VOID
MshcLocalRequestDpc(
    _In_ PKDPC Dpc,
    _In_ PVOID DeferredContext,
    _In_ PVOID SystemArgument1,
    _In_ PVOID SystemArgument2
    );

//
// Local command routines
//

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSendStopCommand(
    _In_ PMSHC_EXTENSION MshcExtension
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSendStatusCommand(
    _In_ PMSHC_EXTENSION MshcExtension,
    _Out_ PULONG StatusRegister
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcSendTuningCommand(
    _In_ PMSHC_EXTENSION MshcExtension
    );

//
// General utility routines
//

VOID
MshcReadDataPort(
    _In_ PMSHC_EXTENSION MshcExtension,
    _Out_writes_all_(Length) PULONG Buffer,
    _In_ ULONG Length
    );

VOID
MshcWriteDataPort(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_reads_(Length) PULONG Buffer,
    _In_ ULONG Length
    );

NTSTATUS
MshcResetWait(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Mask
    );

BOOLEAN
MshcIsDataBusy(
    _In_ PMSHC_EXTENSION MshcExtension
    );

NTSTATUS
MshcWaitDataIdle(
    _In_ PMSHC_EXTENSION MshcExtension
    );

NTSTATUS
MshcCiuUpdateClock(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Flags
    );

BOOLEAN
MshcIsCardInserted(
    _In_ PMSHC_EXTENSION MshcExtension
    );

BOOLEAN
MshcIsCardWriteProtected(
    _In_ PMSHC_EXTENSION MshcExtension
    );

_IRQL_requires_max_(APC_LEVEL)
NTSTATUS
MshcQueryAcpiPlatformCapabilities(
    _In_ PDEVICE_OBJECT Pdo,
    _Out_ PMSHC_PLATFORM_CAPABILITIES Capabilities
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
MSHC_PLATFORM_TYPE
MshcGetPlatformType(
    _In_ PDEVICE_OBJECT Pdo
    );

PMSHC_PLATFORM_DRIVER_DATA
MshcGetPlatformDriverData(
    _In_ MSHC_PLATFORM_TYPE PlatformType
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
MshcMiniportInitialize(
    _In_ PSD_MINIPORT Miniport
    );

_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MshcLogInit(
    _In_ PMSHC_EXTENSION MshcExtension
    );

_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
MshcLogCleanup(
    _In_ PMSHC_EXTENSION MshcExtension
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
RegKeyQueryUlongValue(
    _In_ HANDLE Key,
    _In_ PUNICODE_STRING ValueName,
    _Out_ PULONG Value
    );

//
// Register access helpers
//

__forceinline
ULONG
MshcReadRegister(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Register
    )
{
    return SdPortReadRegisterUlong(MshcExtension->BaseAddress, Register);
}

__forceinline
VOID
MshcWriteRegister(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Register,
    _In_ ULONG Data
    )
{
    SdPortWriteRegisterUlong(MshcExtension->BaseAddress, Register, Data);
}

__forceinline
VOID
MshcReadRegisterBufferUlong(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Register,
    _Out_writes_all_(Length) PULONG Buffer,
    _In_ ULONG Length
    )
{
    SdPortReadRegisterBufferUlong(
        MshcExtension->BaseAddress,
        Register,
        Buffer,
        Length);
}

__forceinline
VOID
MshcWriteRegisterBufferUlong(
    _In_ PMSHC_EXTENSION MshcExtension,
    _In_ ULONG Register,
    _In_reads_(Length) PULONG Buffer,
    _In_ ULONG Length
    )
{
    SdPortWriteRegisterBufferUlong(
        MshcExtension->BaseAddress,
        Register,
        Buffer,
        Length);
}

//
// General utility macros
//

#define MIN(x,y) ((x) > (y) ? (y) : (x))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define DIV_CEIL(x, y) (((x) + (y) - 1) / (y))

#define IS_ALIGNED(val, align)  (((val) & ((align) - 1)) == 0)
