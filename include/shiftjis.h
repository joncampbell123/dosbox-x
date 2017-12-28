
struct ShiftJISDecoder {
                        ShiftJISDecoder();

    void                reset(void);
    bool                take(unsigned char c);
    bool                leadByteWaitingForSecondByte(void);
public:
    unsigned char       b1,b2;
    bool                fullwidth;
    bool                doublewide; /* character is displayed double-wide */
};

