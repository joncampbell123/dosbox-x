
#include <stdio.h>
#include <stdint.h>

class Intel8255 {
public:
    enum {
        PortA=0,
        PortB=1,
        PortC=2
    };
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
    inline const char*  inPortName(const unsigned int port,const unsigned int i) const {
        return inPortNames[port][i];
    }
    inline const char*  outPortName(const unsigned int port,const unsigned int i) const {
        return outPortNames[port][i];
    }
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
    const char*         inPortNames[3/*port*/][8/*bit*/];
    const char*         outPortNames[3/*port*/][8/*bit*/];
public:
    uint8_t             outPortA,outPortB,outPortC;
    uint8_t             mode;
    /* bit[7:7] = 1             mode set flag
     * bit[6:5] = mode select   00=mode 0  01=mode 1  1x=mode 2
     * bit[4:4] = Port A        1=input  0=output
     * bit[3:3] = Port C upper  1=input  0=output
     * bit[2:2] = mode select   0=mode 0   1=mode 1
     * bit[1:1] = Port B        1=input  0=output
     * bit[0:0] = Port C lower  1=input  0=output */
};

