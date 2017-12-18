#include "gmtp_header.h"

TypeId GmtpHeader::GetTypeId(void) {
    static TypeId tid = TypeId("GmtpHeader")
        .SetParent<Header> ()
        .AddConstructor<GmtpHeader>();

    return tid;
}

TypeId GmtpHeader::GetInstanceTypeId(void) {
    return GetTypeId();
}

unit32_t GmtpHeader::GetSerializedSize (void) const {
    return 6;
}

void GmtpHeader::Serialize (Buffer::Iterator start) const {
    // The 2 byte-constant
    start.WriteU8 (0xfe);
    start.WriteU8 (0xef);
    // The data.
    start.WriteHtonU32 (m_data);
}


uint32_t GmtpHeader::Deserialize (Buffer::Iterator start) {

    uint8_t tmp;

    tmp = start.ReadU8();
    NS_ASSERT (tmp == 0xfe);
    tmp = start.ReadU8 ();
    NS_ASSERT (tmp == 0xef);
    m_data = start.ReadNtohU32 ();

    return 6; // the number of bytes consumed.
}


