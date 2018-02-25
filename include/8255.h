
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

    virtual void        ackPortA(void);
    virtual void        ackPortB(void);

    virtual void        strobePortA(void);
    virtual void        strobePortB(void);

    uint8_t             readPortA(void);
    uint8_t             readPortB(void);
    uint8_t             readPortC(void);
    uint8_t             readControl(void);

    void                writePortA(uint8_t data,uint8_t mask);
    void                writePortB(uint8_t data,uint8_t mask);
    void                writePortC(uint8_t data,uint8_t mask);
    void                writeControl(uint8_t data);
public:
    virtual uint8_t     inPortA(void) const;
    virtual uint8_t     inPortB(void) const;
    virtual uint8_t     inPortC(void) const;
public:
    virtual void        outPortA(const uint8_t mask);
    virtual void        outPortB(const uint8_t mask);
    virtual void        outPortC(const uint8_t mask);
public:
    void                updateINTR_A(void);
    void                updateINTR_B(void);
public:
    virtual void        checkINTR_A(void);
    virtual void        checkINTR_B(void);
public:
    inline const char*  inPortName(const unsigned int port,const unsigned int i) const {
        return inPortNames[port][i];
    }
    inline const char*  outPortName(const unsigned int port,const unsigned int i) const {
        return outPortNames[port][i];
    }
public:
    uint8_t             portAWriteMask,portBWriteMask,portCWriteMask;
public:
    const char*         inPortNames[3/*port*/][8/*bit*/];
    const char*         outPortNames[3/*port*/][8/*bit*/];
public:
    uint8_t             latchOutPortA,latchOutPortB,latchOutPortC;
    uint8_t             mode;
    /* bit[7:7] = 1             mode set flag
     * bit[6:5] = mode select   00=mode 0  01=mode 1  1x=mode 2
     * bit[4:4] = Port A        1=input  0=output
     * bit[3:3] = Port C upper  1=input  0=output
     * bit[2:2] = mode select   0=mode 0   1=mode 1
     * bit[1:1] = Port B        1=input  0=output
     * bit[0:0] = Port C lower  1=input  0=output */
public:
    bool                IBF_A,IBF_B;
    bool                OBF_A,OBF_B;
public:
    bool                INTR_A,INTR_B;
public:
    bool                INTE_1,INTE_2; /* mode 2 */
    bool                INTE_A,INTE_B;
};

