#pragma once
#pragma warning(push)
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union

#pragma region Read32/Write32

template<class T>
struct TypeIdentity { using type = T; };

template<class T>
_IRQL_requires_max_(HIGH_LEVEL)
static T FORCEINLINE
Read32(_In_ T const* reg)
{
    // HIGH_LEVEL
    static_assert(sizeof(T) == sizeof(ULONG));
    auto const val = READ_REGISTER_ULONG((ULONG*)reg);
    return reinterpret_cast<T const&>(val);
}

template<class T>
_IRQL_requires_max_(HIGH_LEVEL)
static void FORCEINLINE
Write32(_Out_ T* reg, typename TypeIdentity<T>::type val)
{
    // HIGH_LEVEL
    static_assert(sizeof(T) == sizeof(ULONG));
#pragma warning(suppress: 6001) // WRITE_REGISTER_ULONG is not annotated correctly.
    WRITE_REGISTER_ULONG(reinterpret_cast<ULONG*>(reg), reinterpret_cast<ULONG const&>(val));
}

#pragma endregion

// Adapted from Rockchip RK3588 TRM V1.0.

#pragma region ChannelRegisters

union ChannelDmaControl_t
{
    UINT32 Value32;
    struct
    {
        UINT32 MaxSegmentSize : 14; // MSS - Maximum Segment Size
        UINT32 Reserved14 : 2;

        UINT32 PblX8 : 1; // PBLx8 - 8xPBL Mode
        UINT32 Reserved17 : 1;
        UINT32 DescriptorSkipLength : 3; // DSL - Descriptor Skip Length
        UINT32 Reserved21 : 11;
    };
};

union ChannelTxControl_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Start : 1; // ST
        UINT32 Reserved1 : 3;
        UINT32 OperateOnSecondPacket : 1; // OSF
        UINT32 Reserved5 : 7;
        UINT32 TcpSegmentation : 1; // TSE
        UINT32 TcpSegmentationMode : 2; // TSE_MODE
        UINT32 IgnorePbl : 1; // IPBL

        UINT32 TxPbl : 6; // TxPBL - Transmit Programmable Burst Length
        UINT32 Reserved22 : 10;
    };
};

union ChannelRxControl_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Start : 1; // SR
        UINT32 ReceiveBufferSize : 14; // RBSZ - Receive Buffer Size, low 3 bits must be 0.
        UINT32 Reserved15 : 1;

        UINT32 RxPbl : 6; // RxPBL - Receive Programmable Burst Length
        UINT32 Reserved22 : 9;
        UINT32 RxPacketFlush : 1; // RPF - Receive Packet Flush
    };
};

union ChannelInterruptEnable_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Tx : 1; // TIE
        UINT32 TxStopped : 1; // TXSE
        UINT32 TxBufferUnavailable : 1; // TBUE
        UINT32 Reserved3 : 3;
        UINT32 Rx : 1; // RIE
        UINT32 RxBufferUnavailable : 1; // RBUE

        UINT32 RxStopped : 1; // RSE
        UINT32 RxWatchdogTimeout : 1; // RWTE
        UINT32 EarlyTx : 1; // ETIE
        UINT32 EarlyRx : 1; // ERIE
        UINT32 FatalBusError : 1; // FBEE
        UINT32 ContextDescriptorError : 1; // CDEE
        UINT32 AbnormalInterruptSummary : 1; // AIE
        UINT32 NormalInterruptSummary : 1; // NIE

        UINT32 Reserved16 : 16;
    };
};

enum ChannelDmaError_t : UINT32
{
    ChannelDmaError_WriteBufferDmaOk = 0,    // 000
    ChannelDmaError_ReadBufferDmaOk,         // 001
    ChannelDmaError_WriteDescriptorDmaOk,    // 010
    ChannelDmaError_ReadDescriptorDmaOk,     // 011
    ChannelDmaError_WriteBufferDmaError,     // 100
    ChannelDmaError_ReadBufferDmaError,      // 101
    ChannelDmaError_WriteDescriptorDmaError, // 110
    ChannelDmaError_ReadDescriptorDmaError,  // 111
};

union ChannelStatus_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Tx : 1; // TI
        UINT32 TxStopped : 1; // TPS
        UINT32 TxBufferUnavailable : 1; // TBU
        UINT32 Reserved3 : 3;
        UINT32 Rx : 1; // RI
        UINT32 RxBufferUnavailable : 1; // RBU

        UINT32 RxStopped : 1; // RPS
        UINT32 RxWatchdogTimeout : 1; // RWT
        UINT32 EarlyTx : 1; // ETI
        UINT32 EarlyRx : 1; // ERI
        UINT32 FatalBusError : 1; // FBE
        UINT32 ContextDescriptorError : 1; // CDE
        UINT32 AbnormalInterruptSummary : 1; // AIS
        UINT32 NormalInterruptSummary : 1; // NIS

        ChannelDmaError_t TxDmaError : 3; // TEB - valid only when the FBE bit is set
        ChannelDmaError_t RxDmaError : 3; // REB - valid only when the FBE bit is set
        UINT32 Reserved22 : 10;
    };
};

struct ChannelRegisters
{
    // DMA_CHx_Control @ 0x00 = 0x0:
    // The register specifies the MSS value for segmentation, length to skip between
    // two descriptors, and 8xPBL mode.
    ChannelDmaControl_t DmaControl;

    // DMA_CHx_Tx_Control @ 0x04 = 0x0:
    // The register controls the Tx features such as PBL, TCP segmentation, and Tx
    // Channel weights.
    ChannelTxControl_t TxControl;

    // DMA_CHx_Rx_Control @ 0x08 = 0x0:
    // The DMA Channel0 Receive Control register controls the Rx features such as PBL,
    // buffer size, and extended status.
    ChannelRxControl_t RxControl;

    ULONG Padding0C[1];

    // DMA_CHx_TxDesc_List_Address_Hi @ 0x10 = 0x0:
    ULONG TxDescListAddressHigh;

    // DMA_CHx_TxDesc_List_Address @ 0x14 = 0x0:
    // The Channel0 Tx Descriptor List Address register points the DMA to the start of
    // Transmit.
    ULONG TxDescListAddress;

    // DMA_CHx_RxDesc_List_Address_Hi @ 0x18 = 0x0:
    ULONG RxDescListAddressHigh;

    // DMA_CHx_RxDesc_List_Address @ 0x1C = 0x0:
    // The Channel0 Rx Descriptor List Address register points the DMA to the start of
    // Receive descriptor list.
    ULONG RxDescListAddress;

    // DMA_CHx_TxDesc_Tail_Pointer @ 0x20 = 0x0:
    // The Channel0 Tx Descriptor Tail Pointer register points to an offset from the
    // base and indicates the location of the last valid descriptor.
    ULONG TxDescTailPointer;

    ULONG Padding24[1];

    // DMA_CHx_RxDesc_Tail_Pointer @ 0x28 = 0x0:
    // The Channel0 Rx Descriptor Tail Pointer Points to an offset from the base and
    // indicates the location of the last valid descriptor.
    ULONG RxDescTailPointer;

    // DMA_CHx_TxDesc_Ring_Length @ 0x2C = 0x0:
    // The Tx Descriptor Ring Length register contains the length of the Transmit
    // descriptor ring.
    ULONG TxDescRingLength;

    // DMA_CHx_RxDesc_Ring_Length @ 0x30 = 0x0:
    // The Channel0 Rx Descriptor Ring Length register contains the length of the
    // Receive descriptor circular ring.
    ULONG RxDescRingLength;

    // DMA_CHx_Interrupt_Enable @ 0x34 = 0x0:
    // The Channel0 Interrupt Enable register enables the interrupts reported by the
    // Status register.
    ChannelInterruptEnable_t InterruptEnable;

    // DMA_CHx_Rx_Interrupt_WD_Timer @ 0x38 = 0x0:
    // The Receive Interrupt Watchdog Timer register indicates the watchdog timeout
    // for Receive Interrupt (RI) from the DMA.
    ULONG RxInterruptWdTimer;

    // DMA_CHx_Slot_Func_Ctrl_Status @ 0x3C = 0x7C0:
    // The Slot Function Control and Status register contains the control bits for
    // slot function and the status for Transmit path.
    ULONG SlotFuncCtrlStatus;

    ULONG Padding40[1];

    // DMA_CHx_Current_App_TxDesc @ 0x44 = 0x0:
    // The Channel0 Current Application Transmit Descriptor register points to the
    // current Transmit descriptor read by the DMA.
    ULONG TxCurrentAppDesc;

    ULONG Padding48[1];

    // DMA_CHx_Current_App_RxDesc @ 0x4C = 0x0:
    // The Channel0 Current Application Receive Descriptor register points to the
    // current Receive descriptor read by the DMA.
    ULONG RxCurrentAppDesc;

    ULONG Padding50[1];

    // DMA_CHx_Current_App_TxBuffer @ 0x54 = 0x0:
    // The Channel0 Current Application Transmit Buffer Address register points to the
    // current Tx buffer address read by the DMA.
    ULONG TxCurrentAppBuffer;

    ULONG Padding58[1];

    // DMA_CHx_Current_App_RxBuffer @ 0x5C = 0x0:
    // The Channel0 Current Application Receive Buffer Address register points to the
    // current Rx buffer address read by the DMA.
    ULONG RxCurrentAppBuffer;

    // DMA_CHx_Status @ 0x60 = 0x0:
    // The software driver (application) reads the Status register during interrupt
    // service routine or polling to determine the status of the DMA.
    ChannelStatus_t Status;

    // DMA_CHx_Miss_Frame_Cnt @ 0x64 = 0x0:
    // This register has the number of packet counter that got dropped by the DMA
    // either due to Bus Error or due to programming RPF field in RxControl
    // register.
    ULONG MissFrameCnt;

    // DMA_CHx_RX_ERI_Cnt @ 0x68 = 0x0:
    // The RxEriCnt registers provides the count of the number of times ERI
    // was asserted.
    ULONG RxEriCnt;

    ULONG Padding6C[5];
};
static_assert(sizeof(ChannelRegisters) == 128);

#pragma endregion

#pragma region MtlQueueRegisters

enum MtlTxQueueEnable_t : UINT32
{
    MtlTxQueueEnable_Disabled = 0,
    MtlTxQueueEnable_EnabledAV,
    MtlTxQueueEnable_Enabled,
};

enum MtlTxReadStatus_t : UINT32
{
    MtlTxReadStatus_Idle = 0,
    MtlTxReadStatus_Read,
    MtlTxReadStatus_Waiting,
    MtlTxReadStatus_Flushing,
};

union MtlTxOperationMode_t
{
    UINT32 Value32;
    struct
    {
        UINT32 FlushTxQueue : 1; // FTQ
        UINT32 StoreAndForward : 1; // TSF
        MtlTxQueueEnable_t QueueEnable : 2; // TXQEN
        UINT32 ThresholdControl : 3; // TTC
        UINT32 Reserved7 : 9;
        UINT32 QueueSize : 6; // TQS
        UINT32 Reserved22 : 10;
    };
};

union MtlTxDebug_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Paused : 1; // TXQPAUSED
        MtlTxReadStatus_t ReadStatus : 2; // TRCSTS
        UINT32 WriteStatus : 1; // TWCSTS
        UINT32 NotEmpty : 1; // TXQSTS
        UINT32 FifoFull : 1; // TXSTSFSTS
        UINT32 Reserved6 : 10;

        UINT32 QueuePackets : 3; // PTXQ
        UINT32 Reserved19 : 1;
        UINT32 QueueStatusWords : 3; // STXSTSF
        UINT32 Reserved23 : 9;
    };
};

union MtlRxOperationMode_t
{
    UINT32 Value32;
    struct
    {
        UINT32 ThresholdControl : 2; // RTC
        UINT32 Reserved2 : 1;
        UINT32 ForwardUndersizedGoodPackets : 1; // FUP
        UINT32 ForwardErrorPackets : 1; // FEP
        UINT32 StoreAndForward : 1; // RSF
        UINT32 DisableDropTcpChecksumError : 1; // DIS_TCP_EF
        UINT32 HardwareFlowControl : 1; // EHFC
        UINT32 FlowControlActivate : 6; // RFA
        UINT32 FlowControlDeactivate : 6; // RFD
        UINT32 QueueSize : 7; // RQS
        UINT32 Reserved27 : 5;
    };
};

struct MtlQueueRegisters
{
    // MTL_TxQx_Operation_Mode @ 0x00 = 0x60000:
    // The Queue X Transmit Operation Mode register establishes the Transmit queue
    // operating modes and commands.
    MtlTxOperationMode_t TxOperationMode;

    // MTL_TxQx_Underflow @ 0x04 = 0x0:
    // The Queue X Underflow Counter register contains the counter for packets aborted
    // because of Transmit queue underflow and packets missed because of Receive queue
    // packet flush.
    ULONG TxUnderflow;

    // MTL_TxQx_Debug @ 0x08 = 0x0:
    // The Queue X Transmit Debug register gives the debug status of various blocks
    // related to the Transmit queue.
    MtlTxDebug_t TxDebug;

    ULONG Padding0C[2];

    // MTL_TxQx_ETS_Status @ 0x14 = 0x0:
    // The Queue X ETS Status register provides the average traffic transmitted in
    // Queue X.
    ULONG TxEtsStatus;

    // MTL_TxQx_Quantum_Weight @ 0x18 = 0x0:
    // The Queue X Quantum or Weights register contains the quantum value for Deficit
    // Weighted Round Robin (DWRR), weights for the Weighted Round Robin (WRR), and
    // Weighted Fair Queuing (WFQ) for Queue X.
    ULONG TxQuantumWeight;

    ULONG Padding1C[4];

    // MTL_Qx_Interrupt_Ctrl_Status @ 0x2C = 0x0:
    // This register contains the interrupt enable and status bits for the queue X
    // interrupts.
    ULONG InterruptCtrlStatus;

    // MTL_RxQx_Operation_Mode @ 0x30 = 0x0:
    // The Queue X Receive Operation Mode register establishes the Receive queue
    // operating modes and command.
    MtlRxOperationMode_t RxOperationMode;

    // MTL_RxQx_Miss_Pkt_Ovf_Cnt @ 0x34 = 0x0:
    // The Queue X Missed Packet and Overflow Counter register contains the counter
    // for packets missed because of Receive queue packet flush and packets discarded
    // because of Receive queue overflow.
    ULONG RxMissPktOvfCnt;

    // MTL_RxQx_Debug @ 0x38 = 0x0:
    // The Queue X Receive Debug register gives the debug status of various blocks
    // related to the Receive queue.
    ULONG RxDebug;

    // MTL_RxQx_Control @ 0x3C = 0x0:
    // The Queue Receive Control register controls the receive arbitration and passing
    // of received packets to the application.
    ULONG RxControl;
};
static_assert(sizeof(MtlQueueRegisters) == 64);

#pragma endregion

#pragma region MacAddressRegisters

union MacAddressHigh_t
{
    UINT32 Value32;
    struct
    {
        UINT16 Value16Low;
        UINT16 Value16High;
    };
    struct
    {
        BYTE Addr4;
        BYTE Addr5;
        BYTE DmaChannelSelect;
        BYTE MaskByteControl : 6;
        BYTE SourceAddress : 1;
        BYTE AddressEnable : 1;
    };
};

union MacAddressLow_t
{
    UINT32 Value32;
    struct
    {
        BYTE Addr0;
        BYTE Addr1;
        BYTE Addr2;
        BYTE Addr3;
    };
};

struct MacAddressRegisters
{
    // MAC_AddressX_High @ 0x00 = 0xFFFF:
    // The MAC AddressX High register holds the upper 16 bits of the Xth 6-byte MAC
    // address of the station.
    MacAddressHigh_t High;

    // MAC_AddressX_Low @ 0x04 = 0xFFFFFFFF:
    // The MAC AddressX Low register holds the lower 32 bits of the Xth 6-byte MAC
    // address of the station.
    MacAddressLow_t Low;
};
static_assert(sizeof(MacAddressRegisters) == 8);

#pragma endregion

#pragma region MacL3L4Registers

struct MacL3L4Registers
{
    // MAC_L3_L4_ControlX @ 0x00 = 0x0:
    // The Layer 3 and Layer 4 Control register controls the operations of filter X of
    // Layer 3 and Layer 4.
    ULONG Control;

    // MAC_Layer4_AddressX @ 0x04 = 0x0:
    // The MAC_Layer4_Address, MAC_L3_L4_Control, MAC_Layer3_Addr0_Reg,
    // MAC_Layer3_Addr1_Reg, MAC_Layer3_Addr2_Reg and MAC_Layer3_Addr3_Reg
    // registers are reserved (RO with default value) if Enable Layer 3 and Layer 4
    // Packet Filter option is not selected while configuring the core.
    ULONG Layer4Address;

    ULONG Padding08[2];

    // MAC_Layer3_Addr0_RegX @ 0x10 = 0x0:
    // For IPv4 packets, the Layer 3 Address 0 Register contains the 32-bit
    // IP Source Address field. For IPv6 packets, it contains Bits[31:0] of the
    // 128-bit IP Source Address or Destination Address field.
    ULONG Layer3Addr0;

    // MAC_Layer3_Addr1_Reg @ 0x14 = 0x0:
    // For IPv4 packets, the Layer 3 Address 1 Register contains the 32-bit
    // IP Destination Address field. For IPv6 packets, it contains Bits[63:32] of the
    // 128-bit IP Source Address or Destination Address field.
    ULONG Layer3Addr1;

    // MAC_Layer3_Addr2_Reg @ 0x18 = 0x0:
    // The Layer 3 Address 2 Register is reserved for IPv4 packets. For
    // IPv6 packets, it contains Bits[95:64] of 128-bit IP Source Address or
    // Destination Address field.
    ULONG Layer3Addr2;

    // MAC_Layer3_Addr3_Reg @ 0x1C = 0x0:
    // The Layer 3 Address 3 Register is reserved for IPv4 packets. For
    // IPv6 packets, it contains Bits[127:96] of 128-bit IP Source Address or
    // Destination Address field.
    ULONG Layer3Addr3;

    ULONG Padding20[4];
};
static_assert(sizeof(MacL3L4Registers) == 48);

#pragma endregion

#pragma region MacRegisters

enum PortSelectSpeed_t : UINT32
{
    PortSelectSpeed_1000M=0,// PS = 0, FES = 0
    PortSelectSpeed_2500M,  // PS = 0, FES = 1
    PortSelectSpeed_10M,    // PS = 1, FES = 0
    PortSelectSpeed_100M,   // PS = 1, FES = 1
};

enum PhyIfSpeed_t : UINT32
{
    PhyIfSpeed_2_5M = 0, // 2.5 MHz
    PhyIfSpeed_25M,      // 25 MHz
    PhyIfSpeed_125M,     // 125 MHz
};

enum TimestampSource_t : UINT32
{
    TimestampSource_Internal = 0,
    TimestampSource_External,
    TimestampSource_Both,
};

enum ActivePhy_t : UINT32
{
    ActivePhy_GmiiOrMii = 0,
    ActivePhy_Rgmii,
    ActivePhy_Sgmii,
    ActivePhy_Tbi,
    ActivePhy_Rmii,
    ActivePhy_Rtbi,
    ActivePhy_Smii,
    ActivePhy_RevMii,
};

enum AddressWidth_t : UINT32
{
    AddressWidth_32 = 0,
    AddressWidth_40,
    AddressWidth_48,
};

enum HashTableSize_t : UINT32
{
    HashTableSize_0 = 0,
    HashTableSize_64,
    HashTableSize_128,
    HashTableSize_256,
};

union MacConfiguration_t
{
    UINT32 Value32;
    struct
    {
        UINT32 ReceiverEnable : 1;
        UINT32 TransmitterEnable : 1;
        UINT32 PreambleLength : 2;
        UINT32 DeferralCheck : 1;
        UINT32 BackOffLimit : 2;
        UINT32 Reserved7 : 1;

        UINT32 DisableRetry : 1;
        UINT32 DisableCarrierSenseDuringTransmit : 1;
        UINT32 DisableReceiveOwn : 1;
        UINT32 EnableCarrierSenseBeforeTransmit : 1;
        UINT32 LoopbackMode : 1;
        UINT32 FullDuplex : 1;
        PortSelectSpeed_t PortSelectSpeed : 2;

        UINT32 JumboPacketEnable : 1;
        UINT32 JabberDisable : 1;
        UINT32 PacketBurstEnable : 1;
        UINT32 WatchdogDisable : 1;
        UINT32 PadOrCrcStripEnable : 1;
        UINT32 CrcStripEnableForType : 1;
        UINT32 Support2kPackets : 1;
        UINT32 GiantPacketSizeLimitControlEnable : 1;

        UINT32 InterPacketGap : 3;
        UINT32 ChecksumOffloadEnable : 1;
        UINT32 SourceAddrInsertionControl : 3;
        UINT32 ArpOffloadEnable : 1;
    };
};

union MacExtConfiguration_t
{
    UINT32 Value32;
    struct
    {
        UINT32 GiantPacketSizeLimit : 14;
        UINT32 Reserved14 : 2;

        UINT32 DisableCrcCheckRx : 1;
        UINT32 SlowProtocolDetect : 1;
        UINT32 UnicastSlowProtocolPacketDetect : 1;
        UINT32 Reserved19 : 1;
        UINT32 HeaderSplitMaxSize : 3;
        UINT32 Reserved23 : 1;
        UINT32 ExtendedIpgEnable : 1;
        UINT32 ExtendedIpg : 5;
        UINT32 Reserved30 : 2;
    };
};

union MacPacketFilter_t
{
    UINT32 Value32;
    struct
    {
        UINT32 PromiscuousMode : 1;
        UINT32 HashUnicast : 1;
        UINT32 HashMulticast : 1;
        UINT32 DestInverseFilter : 1;
        UINT32 PassAllMulticast : 1;
        UINT32 DisableBroadcast : 1;
        UINT32 PassControl : 2;

        UINT32 SourceInverseFilter : 1;
        UINT32 SourceFilter : 1;
        UINT32 HashPerfectFilter : 1;
        UINT32 Reserved11 : 5;

        UINT32 VlanTagFilter : 1;
        UINT32 Reserved17 : 3;
        UINT32 L3L4Filter : 1;
        UINT32 DropNonTcpUdp : 1;
        UINT32 Reserved22 : 9;
        UINT32 ReceiveAll : 1;
    };
};

union MacInterruptStatus_t
{
    UINT32 Value32;
    struct
    {
        UINT32 LinkStatus : 1; // RGSMIIIS - RGMII or SMII Interrupt Status
        UINT32 Reserved1 : 2;
        UINT32 Phy : 1; // PHYIS - PHY Interrupt Status
        UINT32 Pmt : 1; // PMTIS - PMT Interrupt Status (magic/wake-up frame)
        UINT32 Lpi : 1; // LPIIS - LPI Interrupt Status (energy efficient entry/exit)
        UINT32 Reserved6 : 2;

        UINT32 Mmc : 1; // MMCIS - MMC Interrupt Status
        UINT32 MmcRx : 1; // MMCRXIS - MMC Receive Interrupt Status
        UINT32 MmcTx : 1; // MMCTXIS - MMC Transmit Interrupt Status
        UINT32 MmcRxChecksum : 1; // MMCRXIPIS - MMC Receive Checksum Offload Interrupt Status
        UINT32 Timestamp : 1; // TSIS - Timestamp Interrupt Status
        UINT32 Tx : 1; // TXSTSIS - Transmit Interrupt Status
        UINT32 Rx : 1; // RXSTSIS - Receive Interrupt Status
        UINT32 Reserved15 : 2;

        UINT32 FramePreemption : 1; // FPEIS - Frame Preemption Interrupt Status
        UINT32 Mdio : 1; // MDIOIS - MDIO Interrupt Status
        UINT32 MmcFpeTx : 1; // MFTIS - MMC FPE Transmit Interrupt Status
        UINT32 MmcFpeRx : 1; // MFRIS - MMC FPE Receive Interrupt Status
        UINT32 Reserved21 : 11;
    };
};

union MacInterruptEnable_t
{
    UINT32 Value32;
    struct
    {
        UINT32 LinkStatus : 1; // RGSMIIIE - RGMII or SMII Interrupt Enable
        UINT32 Reserved1 : 2;
        UINT32 Phy : 1; // PHYIE - PHY Interrupt Enable
        UINT32 Pmt : 1; // PMTIE - PMT Interrupt Enable (magic/wake-up frame)
        UINT32 Lpi : 1; // LPIIE - LPI Interrupt Enable (energy efficient entry/exit)
        UINT32 Reserved6 : 6;
        UINT32 Timestamp : 1; // TSIE - Timestamp Interrupt Enable
        UINT32 Tx : 1; // TXSTSIE - Transmit Interrupt Enable
        UINT32 Rx : 1; // RXSTSIE - Receive Interrupt Enable
        UINT32 Reserved15 : 2;

        UINT32 FramePreemption : 1; // FPEIE - Frame Preemption Interrupt Enable
        UINT32 Mdio : 1; // MDIOIE - MDIO Interrupt Enable
        UINT32 Reserved19 : 13;
    };
};

union MacPhyIfControlStatus_t
{
    UINT32 Value32;
    struct
    {
        UINT32 TransmitConfig : 1; // TC - Transmit Configuration to PHY
        UINT32 LinkUpDuringConfig : 1; // LUD - Link Up
        UINT32 Reserved2 : 14;

        UINT32 FullDuplex : 1; // LNKMODE - Link Mode
        PhyIfSpeed_t Speed : 2; // SPD - Speed
        UINT32 LinkUp : 1; // LNKSTS - Link Status
        UINT32 Reserved20 : 12;
    };
};

union MacLpiControlStatus_t
{
    UINT32 Value32;
    struct
    {
        UINT32 TransmitLpiEntry : 1; // TLPIEN - Transmit LPI Entry
        UINT32 TransmitLpiExit : 1; // TLPIEX - Transmit LPI Exit
        UINT32 ReceiveLpiEntry : 1; // RLPIEN - Receive LPI Entry
        UINT32 ReceiveLpiExit : 1; // RLPIEX - Receive LPI Exit
        UINT32 Reserved4 : 4;

        UINT32 TransmitLpiState : 1; // TLPIST - Transmit LPI State
        UINT32 ReceiveLpiState : 1; // RLPIST - Receive LPI State
        UINT32 Reserved10 : 6;

        UINT32 LpiEnable : 1; // LPIEN - LPI Enable
        UINT32 PhyLinkStatus : 1; // PLS - PHY Link Status
        UINT32 PhyLinkStatusEnable : 1; // PLSEN - PHY Link Status Enable
        UINT32 LpTxAutomate : 1; // LPTXA - LPI Tx Automate
        UINT32 LpiTimerEnable : 1; // LPIATE - LPI Timer Enable
        UINT32 LpiTxClockStopEnable : 1; // LPITCSE - LPI Tx Clock Stop Enable

        UINT32 Reserved22 : 10;
    };
};

union MacVersion_t
{
    UINT32 Value32;
    struct
    {
        UINT32 RkVer : 8;
        UINT32 UserVer : 8;
        UINT32 Reserved : 16;
    };
};

union MacHwFeature0_t
{
    UINT32 Value32;
    struct
    {
        UINT32 Mii : 1;  // MIISEL - 10 or 100 Mbps Support
        UINT32 Gmii : 1; // GMIISEL - 1000 Mbps Support
        UINT32 HalfDuplex : 1;   // HDSEL - Half Duplex Support
        UINT32 PcsSel : 1;  // PCSSEL - TBI, SGMII, or RTBI PHY interface
        UINT32 VlanHash : 1; // VLHASH - VLAN Hash Filter Support
        UINT32 Mdio : 1;  // SMASEL - MDIO Interface Support
        UINT32 RemoteWake : 1;  // RWKSEL - PMT Remote Wake-up Packet Enable
        UINT32 MagicPacket : 1;  // MGKSEL - PMT Magic Packet Enable

        UINT32 ManagementCounters : 1;  // MMCSEL - MAC Management Counters
        UINT32 ArpOffload : 1; // ARPOFFSEL - ARP Offload Support
        UINT32 Reserved10 : 2;
        UINT32 Timestamp : 1;   // TSSEL - IEEE 1588-2008 Timestamp Support
        UINT32 EnergyEfficient : 1;  // EEESEL - Energy Efficient Ethernet Support
        UINT32 TxChecksumOffload : 1; // TXCOESEL - Transmit Checksum Offload Engine
        UINT32 Reserved15 : 1;

        UINT32 RxChecksumOffload : 1; // RXCOESEL - Receive Checksum Offload Engine
        UINT32 Reserved17 : 1;
        UINT32 MacAddrCount : 7; // ADDMACADRSEL - MAC Address Register Count
        TimestampSource_t TimestampSource : 2; // TSSTSSEL - IEEE 1588-2008 Timestamp Source
        UINT32 SaVlanIns : 1; // SAVLANINS - Source Address or VLAN Insertion
        ActivePhy_t ActivePhy : 4; // ACTPHYSEL - Active PHY Selected
    };
};

union MacHwFeature1_t
{
    UINT32 Value32;
    struct
    {
        UINT32 RxFifoSize : 5; // RXFIFOSIZE - MTL Receive FIFO Size = 128 << RxFifoSize
        UINT32 SinglePortRam : 1; // SPRAM - Single Port RAM
        UINT32 TxFifoSize : 5; // TXFIFOSIZE - MTL Transmit FIFO Size = 128 << RxFifoSize
        UINT32 OsTen : 1; // OSTEN - One-Step Timestamping Enable
        UINT32 PtpOff : 1; // PTOEN - PTP Offload Support
        UINT32 AdvTHword : 1; // ADVTHWORD - IEEE 1588 High Word Register
        AddressWidth_t AddressWidth : 2; // ADDR64 - Address width

        UINT32 DcbEn : 1; // DCBEN - Data Center Bridging Support
        UINT32 SplitHeader : 1; // SPHEN - Split Header Support
        UINT32 TsoEn : 1; // TSOEN - TCP Segmentation Offload Support
        UINT32 DmaDebug : 1; // DBGMEMA - DMA Debug Support
        UINT32 AvSel : 1; // AVSEL - AV Bridging Support
        UINT32 RavSel : 1; // RAVSEL - Rx AV Bridging Support
        UINT32 Reserved22 : 1;
        UINT32 PtpOneStep : 1; // POUOST - One Step for PTP over UDP/IP Feature Enable

        HashTableSize_t HashTableSize : 2; // HASHTBLSZ - Hash Table Size
        UINT32 Reserved26 : 1;
        UINT32 L3L4Filters : 4; // L3L4FNUM - Number of L3/L4 Filters
        UINT32 Reserved31 : 1;
    };
};

union MacHwFeature2_t
{
    UINT32 Value32;
    struct
    {
        UINT32 RxQCnt : 4; // RXQCNT - Number of Rx Queues
        UINT32 Reserved4 : 2;
        UINT32 TxQCnt : 4; // TXQCNT - Number of Tx Queues
        UINT32 Reserved10 : 2;
        UINT32 RxChCnt : 4; // RXCHCNT - Number of DMA Receive Channels

        UINT32 Reserved16 : 2;
        UINT32 TxChCnt : 4; // TXCHCNT - Number of DMA Transmit Channels
        UINT32 Reserved22 : 2;

        UINT32 PpsOutputs : 3; // PPSOUTNUM - Number of PPS Outputs
        UINT32 Reserved27 : 1;
        UINT32 AuxSnapNum : 3; // AUXSNAPNUM - Number of Auxiliary Snapshot Inputs
        UINT32 Reserved31 : 1;
    };
};

union MacHwFeature3_t
{
    UINT32 Value32;
    struct
    {
        UINT32 ExtendedVlanTagFilters : 3; // NRVF - Number of Extended VLAN Tag Filters
        UINT32 Reserved3 : 1;
        UINT32 ChannelVlanTx : 1; // CBTISEL - Channel VLAN Tx Support
        UINT32 DoubleVlanTag : 1; // DVLAN - Double VLAN Tag Support
        UINT32 Reserved6 : 3;
        UINT32 PacketDuplication : 1; // PDUPSEL - Broadcast/Multicast Packet Duplication Support
        UINT32 FlexibleReceiveParser : 1; // FRPSEL - Flexible Receive Parser Support
        UINT32 FlexibleReceiveBufferSize : 2; // FRPBS - Flexible Receive Parser Buffer Size
        UINT32 FlexibleReceiveEntries : 2; // FRPES - Flexible Receive Parser Entries
        UINT32 Reserved15 : 1;

        UINT32 EnhancedScheduling : 1; // ESTSEL - Enhancements to Scheduling Traffic Enable
        UINT32 GateControlDepth : 3; // ESTDEP - Depth of the Gate Control List
        UINT32 GateControlTime : 2; // ESTWID - Width of the Time Interval field in the Gate Control List
        UINT32 Reserved22 : 4;
        UINT32 FramePreemption : 1; // FPESEL - Frame Preemption Enable
        UINT32 TimeBasedScheduling : 1; // TBSSEL - Time-Based Scheduling Enable
        UINT32 AutomotiveSafety : 2; // ASP - Automotive Safety Package Enable
        UINT32 Reserved31 : 2;
    };
};

union DmaSysBusMode_t
{
    UINT32 Value32;
    struct
    {
        UINT32 FixedBurst : 1; // FB - Fixed Burst Length
        UINT32 BurstLength4 : 1; // BLEN4 - AXI Burst Length 4
        UINT32 BurstLength8 : 1; // BLEN8 - AXI Burst Length 8
        UINT32 BurstLength16 : 1; // BLEN16 - AXI Burst Length 16
        UINT32 Reserved4 : 6;
        UINT32 AutoAxiLpi : 1; // AAL - Auto AXI LPI
        UINT32 Reserved11 : 1;
        UINT32 AddressAlignedBeats : 1; // AAL - Address Aligned Beats
        UINT32 Reserved13 : 1;
        UINT32 Reserved14 : 1; // mixed-burst?
        UINT32 Reserved15 : 1;

        UINT32 AxiMaxReadOutstanding : 4; // RD_OSR_LMT - AXI Maximum Read Outstanding Request Limit
        UINT32 Reserved20 : 4;
        UINT32 AxiMaxWriteOutstanding : 4; // WR_OSR_LMT - AXI Maximum Write Outstanding Request Limit
        UINT32 Reserved28 : 2;
        UINT32 UnlockOnPacket : 1; // LPI_XIT_PKT - Unlock on Magic/Remote Wake-up Packet
        UINT32 EnableLpi : 1; // EN_LPI - Enable Low Power Interface (LPI)
    };
};

union MacTxFlowCtrl_t
{
    UINT32 Value32;
    struct
    {
        UINT32 FlowControlBusyActivate : 1; // FCB_BPA - Flow Control Busy Activate
        UINT32 TransmitFlowControlEnable : 1; // TFE - Transmit Flow Control Enable
        UINT32 Reserved2 : 2;
        UINT32 PauseLowThreshold : 3; // PLT - Pause Low Threshold
        UINT32 DisableZeroQuantaPause : 1; // DZPQ - Disable Zero-Quanta Pause

        UINT32 Reserved8 : 8;

        UINT32 PauseTime : 16; // PT - Pause Time
    };
};

struct MacRegisters
{
    // MAC_Configuration @ 0x0000 = 0x0:
    // The MAC Configuration Register establishes the operating mode of the MAC.
    MacConfiguration_t MacConfiguration;

    // MAC_Ext_Configuration @ 0x0004 = 0x0:
    // The MAC Extended Configuration Register establishes the operating mode of the
    // MAC.
    MacExtConfiguration_t MacExtConfiguration;

    // MAC_Packet_Filter @ 0x0008 = 0x0:
    // The MAC Packet Filter register contains the filter controls for receiving
    // packets.
    MacPacketFilter_t MacPacketFilter;

    // MAC_Watchdog_Timeout @ 0x000C = 0x0:
    // The Watchdog Timeout register controls the watchdog timeout for received
    // packets.
    ULONG MacWatchdogTimeout;

    // MAC_Hash_Table_RegX @ 0x0010 = 0x0:
    // The Hash Table Register X contains the Xth 32 bits of the hash table.
    ULONG MacHashTable[3];

    ULONG Padding001C[13];

    // MAC_VLAN_Tag_Ctrl @ 0x0050 = 0x0:
    // This register is the redefined format of the MAC VLAN Tag Register. It is used
    // for indirect addressing. It contains the address offset, command type and Busy
    // Bit for CSR access of the Per VLAN Tag registers.
    ULONG MacVlanTagCtrl;

    // MAC_VLAN_Tag_Data @ 0x0054 = 0x0:
    // This register holds the read/write data for Indirect Access of the Per VLAN Tag
    // registers.During the read access, this field contains valid read data only
    // after the OB bit is reset. During the write access, this field should be valid
    // prior to setting the OB bit in the MacVlanTag_Ctrl Register.
    ULONG MacVlanTagData;

    // MAC_VLAN_Hash_Table @ 0x0058 = 0x0:
    // When VTHM bit of the MacVlanTag register is set, the 16-bit VLAN Hash Table
    // register is used for group address filtering based on the VLAN tag.
    ULONG MacVlanHashTable;

    ULONG Padding005C[1];

    // MAC_VLAN_Incl @ 0x0060 = 0x0:
    // The VLAN Tag Inclusion or Replacement register contains the VLAN tag for
    // insertion or replacement in the Transmit packets. It also contains the VLAN tag
    // insertion controls.
    ULONG MacVlanIncl;

    // MAC_Inner_VLAN_Incl @ 0x0064 = 0x0:
    // The Inner VLAN Tag Inclusion or Replacement register contains the inner VLAN
    // tag to be inserted or replaced in the Transmit packet. It also contains the
    // inner VLAN tag insertion controls.
    ULONG MacInner_VlanIncl;

    ULONG Padding0068[2];

    // MAC_Q0_Tx_Flow_Ctrl @ 0x0070 = 0x0:
    // The Flow Control register controls the generation and reception of the Control
    // (Pause Command) packets by the Flow control module of the MAC.
    MacTxFlowCtrl_t MacTxFlowCtrl;

    ULONG Padding0074[7];

    // MAC_Rx_Flow_Ctrl @ 0x0090 = 0x0:
    // The Receive Flow Control register controls the pausing of MAC Transmit based on
    // the received Pause packet.
    ULONG MacRxFlowCtrl;

    // MAC_RxQ_Ctrl4 @ 0x0094 = 0x0:
    // The Receive Queue Control 4 register controls the routing of unicast and
    // multicast packets that fail the Destination or Source address filter to the Rx
    // queues.
    ULONG MacRxCtrl4;

    ULONG Padding0098[2];

    // MAC_RxQ_Ctrl0 @ 0x00A0 = 0x0:
    // The Receive Queue Control 0 register controls the queue management in the MAC
    // Receiver.
    ULONG MacRxCtrl0;

    // MAC_RxQ_Ctrl1 @ 0x00A4 = 0x0:
    // The Receive Queue Control 1 register controls the routing of multicast,
    // broadcast, AV, DCB, and untagged packets to the Rx queues.
    ULONG MacRxCtrl1;

    // MAC_RxQ_Ctrl2 @ 0x00A8 = 0x0:
    // This register controls the routing of tagged packets based on the USP (user
    // Priority) field of the received packets to the RxQueues 0 to 3.
    ULONG MacRxCtrl2;

    ULONG Padding00AC[1];

    // MAC_Interrupt_Status @ 0x00B0 = 0x0:
    // The Interrupt Status register contains the status of interrupts.
    MacInterruptStatus_t MacInterruptStatus;

    // MAC_Interrupt_Enable @ 0x00B4 = 0x0:
    // The Interrupt Enable register contains the masks for generating the interrupts.
    MacInterruptEnable_t MacInterruptEnable;

    // MAC_Rx_Tx_Status @ 0x00B8 = 0x0:
    // The Receive Transmit Status register contains the Receive and Transmit Error
    // status.
    ULONG MacRxTxStatus;

    ULONG Padding00BC[1];

    // MAC_PMT_Control_Status @ 0x00C0 = 0x0:
    // The PMT Control and Status Register.
    ULONG MacPmtControlStatus;

    // MAC_RWK_Packet_Filter @ 0x00C4 = 0x0:
    // The Remote Wakeup Filter registers are implemented as 8, 16, or 32 indirect
    // access registers (wkuppktfilter_reg#i) based on whether 4, 8, or 16 Remote
    // Wakeup Filters are selected in the configuration and accessed by application
    // through MacRWK_Packet_Filter register.
    ULONG MacRwkPacketFilter;

    // RWK_Filter01_CRC @ 0x00C4 = 0x0:
    // RWK Filter 0/1 CRC-16.
    //ULONG RWK_Filter01_CRC; // UNION

    // RWK_Filter0_Byte_Mask @ 0x00C4 = 0x0:
    // RWK Filter0 Byte Mask.
    //ULONG RWK_Filter0_Byte_Mask; // UNION

    // RWK_Filter1_Byte_Mask @ 0x00C4 = 0x0:
    // RWK Filter1 Byte Mask.
    //ULONG RWK_Filter1_Byte_Mask; // UNION

    // RWK_Filter23_CRC @ 0x00C4 = 0x0:
    // RWK Filter 2/3 CRC-16.
    //ULONG RWK_Filter23_CRC; // UNION

    // RWK_Filter2_Byte_Mask @ 0x00C4 = 0x0:
    // RWK Filter2 Byte Mask.
    //ULONG RWK_Filter2_Byte_Mask; // UNION

    // RWK_Filter3_Byte_Mask @ 0x00C4 = 0x0:
    // RWK Filter3 Byte Mask.
    //ULONG RWK_Filter3_Byte_Mask; // UNION

    // RWK_Filter_Command @ 0x00C4 = 0x0:
    // RWK Filter Command.
    //ULONG RWK_Filter_Command; // UNION

    // RWK_Filter_Offset @ 0x00C4 = 0x0:
    // RWK Filter Offset.
    //ULONG RWK_Filter_Offset; // UNION

    ULONG Padding00C8[2];

    // MAC_LPI_Control_Status @ 0x00D0 = 0x0:
    // The LPI Control and Status Register controls the LPI functions and provides the
    // LPI interrupt status. The status bits are cleared when this register is read.
    MacLpiControlStatus_t MacLpiControlStatus;

    // MAC_LPI_Timers_Control @ 0x00D4 = 0x3E80000:
    // The LPI Timers Control register controls the timeout values in the LPI states.
    ULONG MacLpiTimersControl;

    // MAC_LPI_Entry_Timer @ 0x00D8 = 0x0:
    // This register controls the Tx LPI entry timer.
    ULONG MacLpiEntryTimer;

    // MAC_1US_Tic_Counter @ 0x00DC = 0x3F:
    // This register controls the generation of the Reference time (1 microsecond
    // tic).
    ULONG Mac1usTicCounter;

    ULONG Padding00E0[6];

    // MAC_PHYIF_Control_Status @ 0x00F8 = 0x0:
    // PHY Interface Control and Status Register.
    MacPhyIfControlStatus_t MacPhyIfControlStatus;

    ULONG Padding00FC[5];

    // MAC_Version @ 0x0110 = 0x3051:
    // The version register identifies the version of the GMAC.
    MacVersion_t MacVersion;

    // MAC_Debug @ 0x0114 = 0x0:
    // The Debug register provides the debug status of various MAC blocks.
    ULONG MacDebug;

    ULONG Padding0118[1];

    // MAC_HW_Feature0 @ 0x011C = 0x181173F3:
    // This register indicates the presence of first set of the optional features or
    // functions.
    MacHwFeature0_t MacHwFeature0;

    // MAC_HW_Feature1 @ 0x0120 = 0x111E01E8:
    // This register indicates the presence of second set of the optional features or
    // functions.
    MacHwFeature1_t MacHwFeature1;

    // MAC_HW_Feature2 @ 0x0124 = 0x11041041:
    // This register indicates the presence of third set of the optional features or
    // functions.
    MacHwFeature2_t MacHwFeature2;

    // MAC_HW_Feature3 @ 0x0128 = 0xC370031:
    // This register indicates the presence of fourth set the optional features or
    // functions.
    MacHwFeature3_t MacHwFeature3;

    ULONG Padding012C[53];

    // MAC_MDIO_Address @ 0x0200 = 0x0:
    // The MDIO Address register controls the management cycles to external PHY
    // through a management interface.
    ULONG MacMdioAddress;

    // MAC_MDIO_Data @ 0x0204 = 0x0:
    // The MDIO Data register stores the Write data to be written to the PHY register
    // located at the address specified in MacMDIO_Address.
    ULONG MacMdioData;

    ULONG Padding0208[2];

    // MAC_ARP_Address @ 0x0210 = 0x0:
    // The ARP Address register contains the IPv4 Destination Address of the MAC.
    ULONG MacArpAddress;

    ULONG Padding0214[7];

    // MAC_CSR_SW_Ctrl @ 0x0230 = 0x0:
    // This register contains SW programmable controls for changing the CSR access
    // response and status bits clearing.
    ULONG MacCsrSwCtrl;

    // MAC_FPE_CTRL_STS @ 0x0234 = 0x0:
    // This register controls the operation of Frame Preemption.
    ULONG MacFpeCtrlSts;

    // MAC_Ext_Cfg1 @ 0x0238 = 0x0:
    // This register contains Split mode control field and offset field for Split
    // Header feature.
    ULONG MacExtCfg1;

    ULONG Padding023C[1];

    // MAC_Presn_Time_ns @ 0x0240 = 0x0:
    // This register contains the 32- bit binary rollover equivalent time of the PTP
    // System Time in ns.
    ULONG MacPresnTimeNS;

    // MAC_Presn_Time_Updt @ 0x0244 = 0x0:
    // This field holds the 32-bit value of MAC 1722 Presentation Time in ns, that
    // should be added to the Current Presentation Time Counter value. Init happens
    // when TSINIT is set, and update happens when the TSUPDT bit is set (TSINIT and
    // TSINIT defined in MacTimestamp_Control register.
    ULONG MacPresnTimeUpdt;

    ULONG Padding0248[46];

    // MAC_AddressX_High, MacAddressX_Low @ 0x0300, 0x0308, 0x0310, 0x0318.
    MacAddressRegisters MacAddress[4];

    ULONG Padding0320[248];

    // MMC_Control @ 0x0700 = 0x0:
    // This register establishes the operating mode of MMC.
    ULONG MmcControl;

    // MMC_Rx_Interrupt @ 0x0704 = 0x0:
    // Maintains the interrupts generated from all Receive statistics counters.
    ULONG MmcRxInterrupt;

    // MMC_Tx_Interrupt @ 0x0708 = 0x0:
    // Maintains the interrupts generated from all Transmit statistics counters.
    ULONG MmcTxInterrupt;

    // MMC_Rx_Interrupt_Mask @ 0x070C = 0x0:
    // This register maintains the masks for interrupts generated from all Receive
    // statistics counters.
    ULONG MmcRxInterruptMask;

    // MMC_Tx_Interrupt_Mask @ 0x0710 = 0x0:
    // This register maintains the masks for interrupts generated from all Transmit
    // statistics counters.
    ULONG MmcTxInterruptMask;

    // Tx_Octet_Count_Good_Bad @ 0x0714 = 0x0:
    // This register provides the number of bytes transmitted by the GMAC, exclusive
    // of preamble and retried bytes, in good and bad packets.
    ULONG TxOctetCountGoodBad;

    // Tx_Packet_Count_Good_Bad @ 0x0718 = 0x0:
    // This register provides the number of good and bad packets, exclusive of retried
    // packets.
    ULONG TxPacketCountGoodBad;

    ULONG Padding071C[11];

    // Tx_Underflow_Error_Packets @ 0x0748 = 0x0:
    // This register provides the number of packets aborted because of packets
    // underflow error.
    ULONG TxUnderflowErrorPackets;

    ULONG Padding074C[5];

    // Tx_Carrier_Error_Packets @ 0x0760 = 0x0:
    // This register provides the number of packets aborted because of carrier sense
    // error (no carrier or loss of carrier).
    ULONG TxCarrierErrorPackets;

    // Tx_Octet_Count_Good @ 0x0764 = 0x0:
    // This register provides the number of bytes exclusive of preamble, only in good
    // packets.
    ULONG TxOctetCountGood;

    // Tx_Packet_Count_Good @ 0x0768 = 0x0:
    // This register provides the number of good packets transmitted by GMAC.
    ULONG TxPacketCountGood;

    ULONG Padding076C[1];

    // Tx_Pause_Packets @ 0x0770 = 0x0:
    // This register provides the number of good Pause packets transmitted by GMAC.
    ULONG TxPausePackets;

    ULONG Padding0774[3];

    // Rx_Packets_Count_Good_Bad @ 0x0780 = 0x0:
    // This register provides the number of good and bad packets received by GMAC.
    ULONG RxPacketCountGoodBad;

    // Rx_Octet_Count_Good_Bad @ 0x0784 = 0x0:
    // This register provides the number of bytes received by GMAC, exclusive of
    // preamble, in good and bad packets.
    ULONG RxOctetCountGoodBad;

    // Rx_Octet_Count_Good @ 0x0788 = 0x0:
    // This register provides the number of bytes received by GMAC, exclusive of
    // preamble, only in good packets.
    ULONG RxOctetCountGood;

    ULONG Padding078C[1];

    // Rx_Multicast_Packets_Good @ 0x0790 = 0x0:
    // This register provides the number of good multicast packets received by GMAC.
    ULONG RxMulticastPacketsGood;

    // Rx_CRC_Error_Packets @ 0x0794 = 0x0:
    // This register provides the number of packets received by GMAC with CRC error.
    ULONG RxCrcErrorPackets;

    ULONG Padding0798[4];

    // Rx_Oversize_Packets_Good @ 0x07A8 = 0x0:
    // This register provides the number of packets received by GMAC without errors,
    // with length greater than the maxsize (1,518 bytes or 1,522 bytes for VLAN
    // tagged packets; 2000 bytes if enabled in the S2KP bit of the MAC_Configuration
    // register).
    ULONG RxOversizePacketsGood;

    ULONG Padding07AC[7];

    // Rx_Length_Error_Packets @ 0x07C8 = 0x0:
    // This register provides the number of packets received by GMAC with length error
    // (Length Type field not equal to packet size), for all packets with valid length
    // field.
    ULONG RxLengthErrorPackets;

    ULONG Padding07CC[1];

    // Rx_Pause_Packets @ 0x07D0 = 0x0:
    // This register provides the number of good and valid Pause packets received by
    // GMAC.
    ULONG RxPausePackets;

    // Rx_FIFO_Overflow_Packets @ 0x07D4 = 0x0:
    // This register provides the number of missed received packets because of FIFO
    // overflow.
    ULONG RxFifoOverflowPackets;

    ULONG Padding07D8[1];

    // Rx_Watchdog_Error_Packets @ 0x07DC = 0x0:
    // This register provides the number of packets received by GMAC with error
    // because of watchdog timeout error (packets with a data load larger than 2,048
    // bytes (when JE and WD bits are reset in MAC_Configuration register), 10,240
    // bytes (when JE bit is set and WD bit is reset in MAC_Configuration register),
    // 16,384 bytes (when WD bit is set in MAC_Configuration register) or the value
    // programmed in the MAC_Watchdog_Timeout register).
    ULONG RxWatchdogErrorPackets;

    ULONG Padding07E0[8];

    // MMC_IPC_Rx_Interrupt_Mask @ 0x0800 = 0x0:
    // This register maintains the mask for the interrupt generated from the receive
    // IPC statistic counters.
    ULONG MmcIpcRxInterruptMask;

    ULONG Padding0804[1];

    // MMC_IPC_Rx_Interrupt @ 0x0808 = 0x0:
    // This register maintains the interrupt that the receive IPC statistic counters
    // generate.
    ULONG MmcIpcRxInterrupt;

    ULONG Padding080C[1];

    // RxIPv4_Good_Packets @ 0x0810 = 0x0:
    // This register provides the number of good IPv4 datagrams received by GMAC with
    // the TCP, UDP, or ICMP payload.
    ULONG RxIPv4GoodPackets;

    // RxIPv4_Header_Error_Packets @ 0x0814 = 0x0:
    // This register provides the number of IPv4 datagrams received by GMAC with
    // header (checksum, length, or version mismatch) errors.
    ULONG RxIPv4HeaderErrorPackets;

    ULONG Padding0818[3];

    // RxIPv6_Good_Packets @ 0x0824 = 0x0:
    // This register provides the number of good IPv6 datagrams received by GMAC.
    ULONG RxIPv6GoodPackets;

    // RxIPv6_Header_Error_Packets @ 0x0828 = 0x0:
    // This register provides the number of IPv6 datagrams received by GMAC with
    // header (length or version mismatch) errors.
    ULONG RxIPv6HeaderErrorPackets;

    ULONG Padding082C[2];

    // RxUDP_Error_Packets @ 0x0834 = 0x0:
    // This register provides the number of good IP datagrams received by GMAC whose
    // UDP payload has a checksum error.
    ULONG RxUdpErrorPackets;

    ULONG Padding0838[1];

    // RxTCP_Error_Packets @ 0x083C = 0x0:
    // This register provides the number of good IP datagrams received by GMAC whose
    // TCP payload has a checksum error.
    ULONG RxTcpErrorPackets;

    ULONG Padding0840[1];

    // RxICMP_Error_Packets @ 0x0844 = 0x0:
    // This register provides the number of good IP datagrams received by GMAC whose
    // ICMP payload has a checksum error.
    ULONG RxIcmpErrorPackets;

    ULONG Padding0848[3];

    // RxIPv4_Header_Error_Octets @ 0x0854 = 0x0:
    // This register provides the number of bytes received by GMAC in IPv4 datagrams
    // with header errors (checksum, length, version mismatch).
    ULONG RxIPv4HeaderErrorOctets;

    ULONG Padding0858[4];

    // RxIPv6_Header_Error_Octets @ 0x0868 = 0x0:
    // This register provides the number of bytes received by GMAC in IPv6 datagrams
    // with header errors (length, version mismatch).
    ULONG RxIPv6HeaderErrorOctets;

    ULONG Padding086C[2];

    // RxUDP_Error_Octets @ 0x0874 = 0x0:
    // This register provides the number of bytes received by GMAC in a UDP segment
    // that had checksum errors.
    ULONG RxUdpErrorOctets;

    ULONG Padding0878[1];

    // RxTCP_Error_Octets @ 0x087C = 0x0:
    // This register provides the number of bytes received by GMAC in a TCP segment
    // that had checksum errors.
    ULONG RxTcpErrorOctets;

    ULONG Padding0880[1];

    // RxICMP_Error_Octets @ 0x0884 = 0x0:
    // This register provides the number of bytes received by GMAC in a good ICMP
    // segment.
    ULONG RxIcmpErrorOctets;

    ULONG Padding0888[6];

    // MMC_FPE_Tx_Interrupt @ 0x08A0 = 0x0:
    // This register maintains the interrupts generated from all FPE related Transmit
    // statistics counters.
    ULONG MmcFpeTxInterrupt;

    // MMC_FPE_Tx_Interrupt_Mask @ 0x08A4 = 0x0:
    // This register maintains the masks for interrupts generated from all FPE related
    // Transmit statistics counters.
    ULONG MmcFpeTxInterruptMask;

    // MmcTxFpeFragment_Cntr @ 0x08A8 = 0x0:
    // This register provides the number of additional mPackets transmitted due to
    // preemption.
    ULONG MmcTxFpeFragmentCntr;

    // MMC_Tx_Hold_Req_Cntr @ 0x08AC = 0x0:
    // This register provides the count of number of times a hold request is given to
    // MAC.
    ULONG MmcTxHoldReqCntr;

    ULONG Padding08B0[4];

    // MMC_FPE_Rx_Interrupt @ 0x08C0 = 0x0:
    // This register maintains the interrupts generated from all FPE related Receive
    // statistics counters.
    ULONG MmcFpeRxInterrupt;

    // MMC_FPE_Rx_Interrupt_Mask @ 0x08C4 = 0x0:
    // This register maintains the masks for interrupts generated from all FPE related
    // Receive statistics counters.
    ULONG MmcFpeRxInterruptMask;

    // MMC_Rx_Packet_Asm_Err_Cntr @ 0x08C8 = 0x0:
    // This register provides the number of MAC frames with reassembly errors on the
    // Receiver, due to mismatch in the Fragment Count value.
    ULONG MmcRxPacketAsmErrCntr;

    // MMC_Rx_Packet_SMD_Err_Cntr @ 0x08CC = 0x0:
    // This register provides the number of received MAC frames rejected due to
    // unknown SMD value and MAC frame fragments rejected due to arriving with an
    // SMD-C when there was no.
    ULONG MmcRxPacketSmdErrCntr;

    // MMC_Rx_Packet_Assembly_OK_Cntr @ 0x08D0 = 0x0:
    // This register provides the number of MAC frames that were successfully
    // reassembled and delivered to MAC.
    ULONG MmcRxPacketAssemblyOkCntr;

    // MMC_Rx_FPE_Fragment_Cntr @ 0x08D4 = 0x0:
    // This register provides the number of additional mPackets transmitted due to
    // preemption.
    ULONG MmcRxFpeFragmentCntr;

    ULONG Padding08D8[10];

    // MAC_L3_L4 filters @ 0x0900, 0x0930.
    MacL3L4Registers MacL3L4[2];

    ULONG Padding0950[104];

    // MAC_Timestamp_Control @ 0x0B00 = 0x0:
    // This register controls the operation of the System Time generator and
    // processing of PTP packets for timestamping in the Receiver.
    ULONG MacTimestampControl;

    // MAC_Sub_Second_Increment @ 0x0B04 = 0x0:
    // Specifies the value to be added to the internal system time register every
    // cycle of clk_ptp_ref_i clock.
    ULONG MacSubSecondIncrement;

    // MAC_System_Time_Secs @ 0x0B08 = 0x0:
    // The System Time Nanoseconds register, along with System Time Seconds register,
    // indicates the current value of the system time maintained by the MAC.
    ULONG MacSystemTimeSecs;

    // MAC_System_Time_NS @ 0x0B0C = 0x0:
    // The System Time Nanoseconds register, along with System Time Seconds register,
    // indicates the current value of the system time maintained by the MAC.
    ULONG MacSystemTimeNS;

    // MAC_Sys_Time_Secs_Update @ 0x0B10 = 0x0:
    // The System Time Seconds Update register, along with the System Time Nanoseconds
    // Update register, initializes or updates the system time maintained by the MAC.
    ULONG MacSysTimeSecsUpdate;

    // MAC_Sys_Time_NS_Update @ 0x0B14 = 0x0:
    // MAC System Time Nanoseconds Update register.
    ULONG MacSysTimeNsUpdate;

    // MAC_Timestamp_Addend @ 0x0B18 = 0x0:
    // Timestamp Addend register.  This register value is used only when the system
    // time is configured for Fine Update mode (TSCFUPDT bit in the
    // MacTimestamp_Control register).
    ULONG MacTimestampAddend;

    ULONG Padding0B1C[1];

    // MAC_Timestamp_Status @ 0x0B20 = 0x0:
    // Timestamp Status register. All bits except Bits[27:25] gets cleared when the
    // application reads this register.
    ULONG MacTimestampStatus;

    ULONG Padding0B24[3];

    // MAC_Tx_TS_Status_NS @ 0x0B30 = 0x0:
    // This register contains the nanosecond part of timestamp captured for Transmit
    // packets when Tx status is disabled.
    ULONG MacTxTsStatusNS;

    // MAC_Tx_TS_Status_Secs @ 0x0B34 = 0x0:
    // The register contains the higher 32 bits of the timestamp (in seconds) captured
    // when a PTP packet is transmitted.
    ULONG MacTxTsStatusSecs;

    ULONG Padding0B38[2];

    // MAC_Auxiliary_Control @ 0x0B40 = 0x0:
    // The Auxiliary Timestamp Control register controls the Auxiliary Timestamp
    // snapshot.
    ULONG MacAuxiliaryControl;

    ULONG Padding0B44[1];

    // MAC_Auxiliary_TS_NS @ 0x0B48 = 0x0:
    // The Auxiliary Timestamp Nanoseconds register, along with
    // MacAuxiliary_Timestamp_Seconds, gives the 64-bit timestamp stored as auxiliary
    // snapshot.
    ULONG MacAuxiliaryTsNS;

    // MAC_Auxiliary_TS_Secs @ 0x0B4C = 0x0:
    // The Auxiliary Timestamp - Seconds register contains the lower 32 bits of the
    // Seconds field of the auxiliary timestamp register.
    ULONG MacAuxiliaryTsSecs;

    ULONG Padding0B50[2];

    // MAC_TS_Ingress_Corr_NS @ 0x0B58 = 0x0:
    // This register contains the correction value in nanoseconds to be used with the
    // captured timestamp value in the ingress path.
    ULONG MacTsIngressCorrNS;

    // MAC_TS_Egress_Corr_NS @ 0x0B5C = 0x0:
    // This register contains the correction value in nanoseconds to be used with the
    // captured timestamp value in the egress path.
    ULONG MacTsEgressCorrNS;

    ULONG Padding0B60[2];

    // MAC_TS_Ingress_Latency @ 0x0B68 = 0x0:
    // This register holds the Ingress MAC latency.
    ULONG MacTsIngressLatency;

    // MAC_TS_Egress_Latency @ 0x0B6C = 0x0:
    // This register holds the Egress MAC latency.
    ULONG MacTsEgressLatency;

    // MAC_PPS_Control @ 0x0B70 = 0x0:
    // PPS Control register.
    ULONG MacPpsControl;

    ULONG Padding0B74[3];

    // MAC_PPS0_Target_Time_Seconds @ 0x0B80 = 0x0:
    // The PPS Target Time Seconds register, along with PPS Target Time Nanoseconds
    // register, is used to schedule an interrupt event [Bit 1 of
    // MacTimestamp_Status] when the system time exceeds the value programmed in
    // these registers.
    ULONG MacPps0TargetTimeSeconds;

    // MAC_PPS0_Target_Time_Ns @ 0x0B84 = 0x0:
    // PPS0 Target Time Nanoseconds register.
    ULONG MacPps0TargetTimeNs;

    // MAC_PPS0_Interval @ 0x0B88 = 0x0:
    // The PPS0 Interval register contains the number of units of sub-second increment
    // value between the rising edges of PPS0 signal output (ptp_pps_o[0]).
    ULONG MacPps0Interval;

    // MAC_PPS0_Width @ 0x0B8C = 0x0:
    // The PPS0 Width register contains the number of units of sub-second increment
    // value.
    ULONG MacPps0Width;

    ULONG Padding0B90[28];

    // MTL_Operation_Mode @ 0x0C00 = 0x0:
    // The Operation Mode register establishes the Transmit and Receive operating
    // modes and commands.
    ULONG MtlOperationMode;

    ULONG Padding0C04[1];

    // MTL_DBG_CTL @ 0x0C08 = 0x0:
    // The FIFO Debug Access Control and Status register controls the operation mode
    // of FIFO debug access.
    ULONG MtlDbgCtl;

    // MTL_DBG_STS @ 0x0C0C = 0x1900000:
    // The FIFO Debug Status register contains the status of FIFO debug access.
    ULONG MtlDebugStatus;

    // MTL_FIFO_Debug_Data @ 0x0C10 = 0x0:
    // The FIFO Debug Data register contains the data to be written to or read from
    // the FIFOs.
    ULONG MtlFifoDebugData;

    ULONG Padding0C14[3];

    // MTL_Interrupt_Status @ 0x0C20 = 0x0:
    // The software driver (application) reads this register during interrupt service
    // routine or polling to determine the interrupt status of MTL queues and the MAC.
    ULONG MtlInterruptStatus;

    ULONG Padding0C24[3];

    // MTL_RxQ_DMA_Map0 @ 0x0C30 = 0x0:
    // The Receive Queue and DMA Channel Mapping 0 register is reserved in EQOS-CORE
    // and EQOS-MTL configurations.
    ULONG MtlRxDmaMap0;

    ULONG Padding0C34[3];

    // MTL_TBS_CTRL @ 0x0C40 = 0x0:
    // This register controls the operation of Time Based Scheduling.
    ULONG MtlTbsCtrl;

    ULONG Padding0C44[3];

    // MTL_EST_Control @ 0x0C50 = 0x0:
    // This register controls the operation of Enhancements to Scheduled Transmission
    // (IEEE802.1Qbv).
    ULONG MtlEstControl;

    ULONG Padding0C54[1];

    // MTL_EST_Status @ 0x0C58 = 0x0:
    // This register provides Status related to Enhancements to Scheduled
    // Transmission.
    ULONG MtlEstStatus;

    ULONG Padding0C5C[1];

    // MTL_EST_Sch_Error @ 0x0C60 = 0x0:
    // This register provides the One Hot encoded Queue Numbers that are having the
    // Scheduling related error (timeout).
    ULONG MtlEstSchError;

    // MTL_EST_Frm_Size_Error @ 0x0C64 = 0x0:
    // This register provides the One Hot encoded Queue Numbers that are having the
    // Frame.
    ULONG MtlEstFrmSizeError;

    // MTL_EST_Frm_Size_Capture @ 0x0C68 = 0x0:
    // This register captures the Frame Size and Queue Number of the first occurrence
    // of the Frame Size related error. Up on clearing it captures the data of
    // immediate next occurrence of a similar error.
    ULONG MtlEstFrmSizeCapture;

    ULONG Padding0C6C[1];

    // MTL_EST_Intr_Enable @ 0x0C70 = 0x0:
    // This register implements the Interrupt Enable bits for the various events that
    // generate an interrupt. Bit positions have a 1 to 1 correlation with the status
    // bit positions in MtlETS_Status.
    ULONG MtlEstIntrEnable;

    ULONG Padding0C74[3];

    // MTL_EST_GCL_Control @ 0x0C80 = 0x0:
    // This register provides the control information for reading/writing to the Gate
    // Control lists.
    ULONG MtlEstGclControl;

    // MTL_EST_GCL_Data @ 0x0C84 = 0x0:
    // This register holds the read data or write data in case of reads and writes
    // respectively.
    ULONG MtlEstGclData;

    ULONG Padding0C88[2];

    // MTL_FPE_CTRL_STS @ 0x0C90 = 0x0:
    // This register controls the operation of, and provides status for Frame
    // Preemption (IEEE802.1Qbu/802.3br).
    ULONG MtlFpeCtrlSts;

    // MTL_FPE_Advance @ 0x0C94 = 0x0:
    // This register holds the Hold and Release Advance time.
    ULONG MtlFpeAdvance;

    ULONG Padding0C98[26];

    // MTL_Qx @ 0x0D00, 0x0D40.
    MtlQueueRegisters MtlQ[2];

    ULONG Padding0D80[160];

    // DMA_Mode @ 0x1000 = 0x0:
    // The Bus Mode register establishes the bus operating modes for the DMA.
    ULONG DmaMode;

    // DMA_SysBus_Mode @ 0x1004 = 0x10000:
    // The System Bus mode register controls the behavior of the AHB or AXI master.
    DmaSysBusMode_t DmaSysBusMode;

    // DMA_Interrupt_Status @ 0x1008 = 0x0:
    // The application reads this Interrupt Status register during interrupt service
    // routine or polling to determine the interrupt status of DMA channels, MTL
    // queues, and the MAC.
    ULONG DmaInterruptStatus;

    // DMA_Debug_Status0 @ 0x100C = 0x0:
    // The Debug Status 0 register gives the Receive and Transmit process status for
    // DMA Channel 0-Channel 2 for debugging purpose.
    ULONG DmaDebugStatus0;

    ULONG Padding1010[12];

    // AXI_LPI_Entry_Interval @ 0x1040 = 0x0:
    // This register is used to control the AXI LPI entry interval.
    ULONG AxiLpiEntryInterval;

    ULONG Padding1044[3];

    // DMA_TBS_CTRL @ 0x1050 = 0x0:
    // This register is used to control the TBS attributes.
    ULONG DmaTbsCtrl;

    ULONG Padding1054[43];
    
    // DMA_CH0 @ 0x1100.
    // DMA_CH1 @ 0x1180.
    ChannelRegisters DmaCh[2];
};
static_assert(sizeof(MacRegisters) == 0x1200);

#pragma endregion

#pragma region DmaDescriptor

#define DESCRIPTOR_ALIGN 64

enum TxChecksumInsertion : UINT16
{
    TxChecksumInsertionDisabled = 0,
    TxChecksumInsertionEnabledHeaderOnly = 1,
    TxChecksumInsertionEnabledExceptPseudo = 2,
    TxChecksumInsertionEnabledIncludingPseudo = 3,
};

struct TxDescriptorRead
{
    // TDES0, TDES1

    UINT32 Buf1Ap; // BUF1AP
    UINT32 Buf2Ap; // BUF2AP

    // TDES2

    UINT16 Buf1Length : 14; // B1L
    UINT16 VlanTagControl : 2; // VTIR

    UINT16 Buf2Length : 14; // B2L
    UINT16 TransmitTimestampEnable : 1; // TTSE
    UINT16 InterruptOnCompletion : 1; // IOC

    // TDES3

    UINT16 FrameLength; // FL (15 bits)

    TxChecksumInsertion ChecksumInsertion : 2; // CIC
    UINT16 TcpSegmentationEnable : 1; // TSE = 0
    UINT16 SlotNumber : 4; // Slot Number Control Bits in AV Mode
    UINT16 SourceAddressInsertionControl : 3; // SAIC
    UINT16 CrcPadControl : 2; // CPC
    UINT16 LastDescriptor : 1; // LD
    UINT16 FirstDescriptor : 1; // FD
    UINT16 ContextType : 1; // CTXT = 0
    UINT16 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
    UINT32 PacketIndex;
};
static_assert(sizeof(TxDescriptorRead) == 24);

struct TxDescriptorReadTso
{
    // TDES0, TDES1

    UINT32 Buf1Ap; // BUF1AP, TSO Header Address Pointer if FD = 1
    UINT32 Buf2Ap; // BUF2AP

    // TDES2

    UINT16 Buf1Length : 14; // B1L (10-bit header length if FD = 1)
    UINT16 VlanTagControl : 2; // VTIR

    UINT16 Buf2Length : 14; // B2L
    UINT16 TsoMemoryWriteDisable : 1; // TMWD
    UINT16 InterruptOnCompletion : 1; // IOC

    // TDES3

    UINT32 TcpPayloadLength : 18; // TPL
    UINT32 TcpSegmentationEnable : 1; // TSE = 0
    UINT32 TcpHeaderLength : 4; // TCP/UDP header length (must be 2 for UDP)
    UINT32 SourceAddressInsertionControl : 3; // SAIC
    UINT32 Reserved26 : 2; // CPC, ignored when TSE = 1
    UINT32 LastDescriptor : 1; // LD
    UINT32 FirstDescriptor : 1; // FD
    UINT32 ContextType : 1; // CTXT = 0
    UINT32 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
    UINT32 PacketIndex;
};
static_assert(sizeof(TxDescriptorReadTso) == 24);

struct TxDescriptorWrite
{
    // TDES0, TDES1

    UINT32 TimestampLow;    // TTSL
    UINT32 TimestampHigh;   // TTSH

    // TDES2

    UINT32 Reserved;

    // TDES3

    UINT8 IpHeaderError : 1; // IHE
    UINT8 DeferredBit : 1; // DB
    UINT8 UnderflowError : 1; // UF
    UINT8 ExcessiveDeferral : 1; // ED
    UINT8 CollisionCount : 4; // CC

    UINT8 ExcessiveCollision : 1; // EC
    UINT8 LateCollision : 1; // LC
    UINT8 NoCarrier : 1; // NC
    UINT8 LossOfCarrier : 1; // LC
    UINT8 PayloadChecksumError : 1; // PCE
    UINT8 PacketFlushed : 1; // PF
    UINT8 JabberTimeout : 1; // JT
    UINT8 ErrorSummary : 1; // ES

    UINT8 EccUncorrectableError : 1; // EUE
    UINT8 TimestampStatus : 1; // TTSS
    UINT8 Reserved18 : 5;
    UINT8 DescriptorError : 1; // DE

    UINT8 Reserved24 : 4;
    UINT8 LastDescriptor : 1; // LD
    UINT8 FirstDescriptor : 1; // FD
    UINT8 ContextType : 1; // CTXT = 0
    UINT8 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
    UINT32 PacketIndex;
};
static_assert(sizeof(TxDescriptorWrite) == 24);

struct TxDescriptorContext
{
    // TDES0, TDES1

    UINT32 TimestampLow;    // TTSL
    UINT32 TimestampHigh;   // TTSH

    // TDES2

    UINT16 MaximumSegmentSize : 14; // MSS
    UINT16 Reserved14 : 2;

    UINT16 InnerVlanTag; // IVT

    // TDES3

    UINT16 VlanTag; // VT

    UINT8 VlanTagValid : 1; // VLTV
    UINT8 InnerVlanTagValid : 1; // IVLTV
    UINT8 InnverVlanTagControl : 2; // IVTIR
    UINT8 Reserved20 : 3;
    UINT8 DescriptorError : 1; // DE

    UINT8 Reserved24 : 2;
    UINT8 OneStepInputOrMssValid : 1; // TCMSSV
    UINT8 OneStepEnable : 1; // OSTC
    UINT8 Reserved28 : 2;
    UINT8 ContextType : 1; // CTXT = 1

    // DEBUG

    UINT32 FragmentIndex;
    UINT32 PacketIndex;
};
static_assert(sizeof(TxDescriptorContext) == 24);

union TxDescriptor
{
    TxDescriptorRead    Read;   // When read, TSE = 0, CTXT = 0.
    TxDescriptorReadTso ReadTso;// When read, TSE = 1, CTXT = 0.
    TxDescriptorWrite   Write;  // When write, CTXT = 0.
    TxDescriptorContext Context;// When CTXT = 1.
    UINT32 All[DESCRIPTOR_ALIGN / sizeof(UINT32)];
};
static_assert(sizeof(TxDescriptor) == DESCRIPTOR_ALIGN);

struct RxDescriptorRead
{
    // RDES0, RDES1

    UINT32 Buf1ApLow; // BUF1AP
    UINT32 Buf1ApHigh; // BUF1AP

    // RDES2

    UINT32 Buf2Ap; // BUF2AP

    // RDES3

    UINT16 Reserved0;
    UINT8 Reserved16;

    UINT8 Buf1Valid : 1; // BUF1V
    UINT8 Buf2Valid : 1; // BUF2V
    UINT8 Reserved26 : 4;
    UINT8 InterruptOnCompletion : 1; // IOC
    UINT8 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
};
static_assert(sizeof(RxDescriptorRead) == 20);

struct RxDescriptorWrite
{
    // RDES0

    UINT16 OuterVlanTag; // OVT
    UINT16 InnerVlanTag; // IVT

    // RDES1

    UINT8 PayloadType : 3; // PT
    UINT8 IPHeaderError : 1; // IPHE
    UINT8 IPv4HeaderPresent : 1; // IPV4
    UINT8 IPv6HeaderPresent : 1; // IPV6
    UINT8 IPChecksumBypassed : 1; // IPCB
    UINT8 IPPayloadError : 1; // IPCE

    UINT8 PtpMessageType : 4; // PMT
    UINT8 PtpPacketType : 1; // PFT
    UINT8 PtpVersion : 1; // PV
    UINT8 TimestampAvailable : 1; // TSA
    UINT8 TimestampDropped : 1; // TD

    UINT16 OamSubtype; // OPC - OAM sub-type and code, or MAC control packet opcode (based on LengthType).

    // RDES2

    UINT16 L3L4HeaderLength : 10; // HL
    UINT16 ArpReplyNotGenerated : 1; // ARPNR
    UINT16 Reserved11 : 3;
    UINT16 InnerVlanTagStatus : 1; // ITS
    UINT16 VlanFilterStatus : 1; // OTS

    UINT16 SourceFilterFail : 1; // SAF/RXPD, based on Flexible RX Parser enable.
    UINT16 DestinationFilterFail : 1; // DAF/RXPI, based on Flexible RX Parser enable.
    UINT16 HashFilterStatus : 1; // HF
    UINT16 MacAddressMatchHashValue : 8; // MADRM
    UINT16 L3FilterMatch : 1; // L3FM
    UINT16 L4FilterMatch : 1; // L4FM
    UINT16 L3L4FilterNumber : 3; // L3L4FM

    // RDES3

    UINT16 PacketLength : 15; // PL
    UINT16 ErrorSummary : 1; // ES

    UINT8 LengthType : 3; // LT
    UINT8 DribbleBitError : 1; // DE
    UINT8 ReceiveError : 1; // RE
    UINT8 OverflowError : 1; // OE
    UINT8 ReceiveWatchdogTimeout : 1; // RWT
    UINT8 GiantPacket : 1; // GP

    UINT8 CrcError : 1; // CE
    UINT8 Rdes0Valid : 1; // RS0V, valid only if LD = 1.
    UINT8 Rdes1Valid : 1; // RS1V, valid only if LD = 1.
    UINT8 Rdes2Valid : 1; // RS2V, valid only if LD = 1.
    UINT8 LastDescriptor : 1; // LD
    UINT8 FirstDescriptor : 1; // FD
    UINT8 ContextType : 1; // CTXT = 0
    UINT8 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
};
static_assert(sizeof(RxDescriptorWrite) == 20);

struct RxDescriptorContext
{
    UINT32 TimestampLow;    // RSTL
    UINT32 TimestampHigh;   // RSTH
    UINT32 Reserved2;

    // RDES3

    UINT16 Reserved3a;
    UINT8 Reserved3b;
    UINT8 Reserved3c : 5;
    UINT8 DescriptorError : 1; // DE
    UINT8 ContextType : 1; // CTXT = 0
    UINT8 Own : 1; // OWN

    // DEBUG

    UINT32 FragmentIndex;
};
static_assert(sizeof(RxDescriptorContext) == 20);

union RxDescriptor
{
    RxDescriptorRead    Read;   // When read.
    RxDescriptorWrite   Write;  // When write, CTXT = 0.
    RxDescriptorContext Context;// When write, CTXT = 1.
    UINT32 All[DESCRIPTOR_ALIGN / sizeof(UINT32)];
};
static_assert(sizeof(TxDescriptor) == DESCRIPTOR_ALIGN);

#pragma endregion

#pragma warning(pop)
