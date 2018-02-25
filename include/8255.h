
#include <stdint.h>

class Intel8255 {
public:
                        Intel8255();
    virtual            ~Intel8255();
public:
    void                reset(void);

    uint8_t             readPortA(void) const;
    uint8_t             readPortB(void) const;
    uint8_t             readPortC(void) const;
    uint8_t             readControl(void) const;

    void                writePortA(uint8_t data,uint8_t mask);
    void                writePortB(uint8_t data,uint8_t mask);
    void                writePortC(uint8_t data,uint8_t mask);
    void                writeControl(uint8_t data);
public:
    virtual uint8_t     inPortA(void) const;
    virtual uint8_t     inPortB(void) const;
    virtual uint8_t     inPortC(void) const;
public:
    inline uint8_t      portAWriteMask(void) const {
        return   (mode & 0x10) ? 0x00 : 0xFF;
    }
    inline uint8_t      portBWriteMask(void) const {
        return   (mode & 0x02) ? 0x00 : 0xFF;
    }
    inline uint8_t      portCWriteMask(void) const {
        return  ((mode & 0x08) ? 0x00 : 0xF0) +
                ((mode & 0x01) ? 0x00 : 0x0F);
    }
public:
    uint8_t             outPortA,outPortB,outPortC;
    uint8_t             mode;
};

