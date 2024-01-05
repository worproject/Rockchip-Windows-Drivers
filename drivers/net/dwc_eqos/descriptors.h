/*
Descriptor layout for the DesignWare EQOS Ethernet controller.
*/
#pragma once

#define DESCRIPTOR_ALIGN 64u

#if DBG
#define TX_DESCRIPTOR_DBG_SIZE 24u
#define RX_DESCRIPTOR_DBG_SIZE 20u
#else // DBG
#define TX_DESCRIPTOR_DBG_SIZE 16u
#define RX_DESCRIPTOR_DBG_SIZE 16u
#endif // DBG

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

#if DBG
    UINT32 FragmentIndex;
    UINT32 PacketIndex;
#endif
};
static_assert(sizeof(TxDescriptorRead) == TX_DESCRIPTOR_DBG_SIZE);

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

#if DBG
    UINT32 FragmentIndex;
    UINT32 PacketIndex;
#endif
};
static_assert(sizeof(TxDescriptorReadTso) == TX_DESCRIPTOR_DBG_SIZE);

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

#if DBG
    UINT32 FragmentIndex;
    UINT32 PacketIndex;
#endif
};
static_assert(sizeof(TxDescriptorWrite) == TX_DESCRIPTOR_DBG_SIZE);

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
    UINT8 Own : 1; // OWN

#if DBG
    UINT32 FragmentIndex;
    UINT32 PacketIndex;
#endif
};
static_assert(sizeof(TxDescriptorContext) == TX_DESCRIPTOR_DBG_SIZE);

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

#if DBG
    UINT32 FragmentIndex;
#endif
};
static_assert(sizeof(RxDescriptorRead) == RX_DESCRIPTOR_DBG_SIZE);

enum RxPayloadType : UINT8
{
    RxPayloadTypeUnknown = 0,
    RxPayloadTypeUdp,
    RxPayloadTypeTcp,
    RxPayloadTypeIcmp,
    RxPayloadTypeIgmp,
    RxPayloadTypeAvUntaggedControl,
    RxPayloadTypeAvTaggedData,
    RxPayloadTypeAvTaggedControl,
};

struct RxDescriptorWrite
{
    // RDES0

    UINT16 OuterVlanTag; // OVT
    UINT16 InnerVlanTag; // IVT

    // RDES1

    RxPayloadType PayloadType : 3; // PT
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

#if DBG
    UINT32 FragmentIndex;
#endif
};
static_assert(sizeof(RxDescriptorWrite) == RX_DESCRIPTOR_DBG_SIZE);

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

#if DBG
    UINT32 FragmentIndex;
#endif
};
static_assert(sizeof(RxDescriptorContext) == RX_DESCRIPTOR_DBG_SIZE);

union TxDescriptor
{
    TxDescriptorRead    Read;   // When read, TSE = 0, CTXT = 0.
    TxDescriptorReadTso ReadTso;// When read, TSE = 1, CTXT = 0.
    TxDescriptorWrite   Write;  // When write, CTXT = 0.
    TxDescriptorContext Context;// When CTXT = 1.
    UINT32 All[DESCRIPTOR_ALIGN / sizeof(UINT32)];
};
static_assert(sizeof(TxDescriptor) == DESCRIPTOR_ALIGN);

union RxDescriptor
{
    RxDescriptorRead    Read;   // When read.
    RxDescriptorWrite   Write;  // When write, CTXT = 0.
    RxDescriptorContext Context;// When write, CTXT = 1.
    UINT32 All[DESCRIPTOR_ALIGN / sizeof(UINT32)];
};
static_assert(sizeof(RxDescriptor) == DESCRIPTOR_ALIGN);

#undef DESCRIPTOR_ALIGN
#undef TX_DESCRIPTOR_DBG_SIZE
#undef RX_DESCRIPTOR_DBG_SIZE
