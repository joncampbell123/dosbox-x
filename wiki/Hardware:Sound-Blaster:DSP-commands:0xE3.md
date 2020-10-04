|    |    |
|---:|:---|
|DSP command|0xE3|
|Supported on|Creative Sound Blaster Pro, some clones|
|Description|Ask the DSP for an ASCII copyright string|

# How to use

    /* NOTE: DSP_Read() in this example returns DSP data byte read OR
             returns -1 if DSP never signalled any data to read within
             a timeout */
    
    char tmp[128];
    int i=0,c;

    DSP_Write(0xE3);
    do {
        c = DSP_Read();
        if (c < 0) break; /* out of data, nothing more to read */
        tmp[i++] = c; /* else store the char */
    } while (i < 127);
    tmp[i] = 0;
    printf("DSP copyright string is '%s'\n",tmp);

Write command 0xE3, then keep reading data until the DSP no longer returns any bytes. The bytes you did read should be stored sequentially in a char[] array, they form an ASCII string with the DSP copyright string. Cards prior to the Sound Blaster Pro do not seem to return a copyright string. Most clones do not return a string, though some instead return their own copyright string or model name.

On clone cards the copyright string can be used to more accurately identify the chip/model of the card.

# External references
- [TFM's DSP commands](http://the.earth.li/~tfm/oldpage/sb_dsp.html)
- [Developer Kit for Sound Blaster Series, Second Edition](http://hackipedia.org/Platform/x86/Sound/Creative%20Labs/ISA,%20Sound%20Blaster/pdf/Sound%20Blaster%20Series%20Developer%20Kit%2c%20Second%20Edition.pdf)

