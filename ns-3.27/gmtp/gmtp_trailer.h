#ifndef GMTP_TRAILER_H
#define GMTP_TRAILER_H

class GmtpTrailer : public Trailer {
    public:
        static TypeId GetTypeId(void);
        virtual TypeId GetInstanceTypeId(void) const;
        virtual uint32_t GetSerializeSize(void) const;
        virtual void Serialize(Buffer::Iterator start) const;
        virtual uint32_t Deserialize(Buffer::Interator start);
        virtual void Print(std::ostream &os) const;

        void SetData(uint32_t data);
        uint32_t GetData(void) const;

    private:
        uint32_t m_data;
};

#endif


