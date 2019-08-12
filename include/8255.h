
#include <stdio.h>
#include <stdint.h>

//! \brief Intel 8255 base emulation class
//!
//! \description Intel 8255 Programmable Peripheral Interface emulation class.
//!              The base class handles the functions and register I/O to
//!              emulate the 8255, while the subclass implements behavior
//!              of hardware attached to the 8255.
//!
//!              All emulation is written to follow Intel's datasheet as
//!              closely as possible.
//!
//!              <a target="_blank" href="http://hackipedia.org/browse/Hardware/By%20company/Intel/8255,%20Programmable%20Peripheral%20Interface/8255A,%208255A-5%20Programmable%20Peripheral%20Interface%20(1991-08).pdf">Intel 8255A datasheet</a>
//!
//!              NOTE: Mode 2 emulation has NOT been tested yet!
class Intel8255 {
public:
    //!
    //! Port enumeration (A, B, and C)
    //!
    enum {
        //! Port A (input or output)
        PortA=0,        
        //! Port B (input or output)
        PortB=1,        
        //! Port C (input, output, or half input half output)
        PortC=2         
    };
public:
    //! Constructor
                        Intel8255();
    //! Destructor
    virtual            ~Intel8255();
public:
    //! Reset state (as if activating reset signal)
    void                reset(void);                    

    //! External acknowledgement of port A
    void                ackPortA(void);                 
    //! External acknowledgement of port B
    void                ackPortB(void);                 

    //! Strobed Input (latch to port A)
    virtual void        strobePortA(void);              
    //! Strobed Input (latch to port B)
    virtual void        strobePortB(void);              

    //! Called when CPU reads port A
    uint8_t             readPortA(void);                
    //! Called when CPU reads port B
    uint8_t             readPortB(void);                
    //! Called when CPU reads port C
    uint8_t             readPortC(void);                
    //! Called when CPU reads control port
    uint8_t             readControl(void);              

    //! Called when CPU reads from the chip
    uint8_t             readByPort(const uint8_t p03);  

    //! Called when CPU writes port A
    void                writePortA(const uint8_t data,uint8_t mask=0xFFU);  
    //! Called when CPU writes port B
    void                writePortB(const uint8_t data,uint8_t mask=0xFFU);  
    //! Called when CPU writes port C
    void                writePortC(const uint8_t data,uint8_t mask=0xFFU);  
    //! Called when CPU writes control port
    void                writeControl(const uint8_t data);                   

    //! Called when CPU writes to the chip
    void                writeByPort(const uint8_t p03,const uint8_t data);  
public:
    //! Called by 8255 emulation to latch from port A pins
    virtual uint8_t     inPortA(void) const;            
    //! Called by 8255 emulation to latch from port B pins
    virtual uint8_t     inPortB(void) const;            
    //! Called by 8255 emulation to latch from port C pins
    virtual uint8_t     inPortC(void) const;            
public:
    //! Called by 8255 emulation when latching to port A pins
    virtual void        outPortA(const uint8_t mask);   
    //! Called by 8255 emulation when latching to port B pins
    virtual void        outPortB(const uint8_t mask);   
    //! Called by 8255 emulation when latching to port C pins
    virtual void        outPortC(const uint8_t mask);   
public:
    //! Internal 8255 emulation code to update INTR A signal
    void                updateINTR_A(void);             
    //! Internal 8255 emulation code to update INTR B signal
    void                updateINTR_B(void);             
public:
    //! Internal 8255 emulation code to check INTR A change and dispatch signal
    void                checkINTR_A(void);              
    //! Internal 8255 emulation code to check INTR B change and dispatch signal
    void                checkINTR_B(void);              
public:
    //! Called by 8255 emulation when INTR A signal changes to dispatch signal
    virtual void        sigINTR_A(void);                
    //! Called by 8255 emulation when INTR B signal changes to dispatch signal
    virtual void        sigINTR_B(void);                
public:
    //! \brief Retrieve the name of this chip (for debug/UI purposes)
    inline const char*  getName(void) const {
        return nil_if_null(ppiName);
    }
public:
    //! \brief Retrieve the name of a pin on the chip related to port A, B, or C (what it's connected to) (for debug/UI purposes)
    inline const char*  pinName(const unsigned int port,const unsigned int i) const {
        return nil_if_null(pinNames[port][i]);
    }
    //! \brief Retrieve the name of a port (A, B, or C) (for debug/UI purposes)
    inline const char*  portName(const unsigned int port) const {
        return nil_if_null(portNames[port]);
    }
public:
    //! Port A write mask. Controls which bits are writeable
    uint8_t             portAWriteMask = 0;
    //! Port B write mask. Controls which bits are writeable
    uint8_t             portBWriteMask = 0;
    //! Port C write mask. Controls which bits are writeable
    uint8_t             portCWriteMask = 0;
public:
    //! PPI chip name (for debug/UI purposes)
    const char*         ppiName;
public:
    //! Pin names (for debug/UI purposes)
    const char*         pinNames[3/*port*/][8/*bit*/] = {};
    //! Port names (for debug/UI purposes)
    const char*         portNames[3/*port*/] = {};
public:
    //! Port A output latch
    uint8_t             latchOutPortA = 0;
    //! Port B output latch
    uint8_t             latchOutPortB = 0;
    //! Port C output latch
    uint8_t             latchOutPortC = 0;
    //! PPI mode byte
    uint8_t             mode = 0;
    /* bit[7:7] = 1             mode set flag
     * bit[6:5] = mode select   00=mode 0  01=mode 1  1x=mode 2
     * bit[4:4] = Port A        1=input  0=output
     * bit[3:3] = Port C upper  1=input  0=output
     * bit[2:2] = mode select   0=mode 0   1=mode 1
     * bit[1:1] = Port B        1=input  0=output
     * bit[0:0] = Port C lower  1=input  0=output */
public:
    //! Input Buffer Full, port A contains information (port A, Mode 1)
    bool                IBF_A = false;
    //! Input Buffer Full, port B contains information (port B, Mode 1)
    bool                IBF_B = false;
    //! Output Buffer Full, port A contains information for the external device (port A, Mode 1)
    bool                OBF_A = false;
    //! Output Buffer Full, port B contains information for the external device (port B, Mode 1)
    bool                OBF_B = false;
public:
    //! Interrupt Request A (to the microprocessor)
    bool                INTR_A = false;
    //! Interrupt Request B (to the microprocessor)
    bool                INTR_B = false;
    //! Previous Interrupt Request A state (for change detection)
    bool                pINTR_A = false;
    //! Previous Interrupt Request B state (for change detection)
    bool                pINTR_B = false;
public:
    //! Interrupt 1 enable (mode 2)
    bool                INTE_1 = false;
    //! Interrupt 2 enable (mode 2)
    bool                INTE_2 = false; /* mode 2 */
    //! Interrupt A enable
    bool                INTE_A = false;
    //! Interrupt B enable
    bool                INTE_B = false;
protected:
    //! Return string "str", or "" (empty string) if str == NULL
    static inline const char *nil_if_null(const char *str) {
        return (str != NULL) ? str : "";
    }
};

