/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997, 1998, 1999, 2000, 2001  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@devolution.com
*/

/*
 * This file was derived from SDL's SDL_audiocvt.c and is an attempt to
 * address the shortcomings of it.
 *
 * Perhaps we can adapt some good filters from SoX?
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "SDL_sound.h"
#define __SDL_SOUND_INTERNAL__
#include "SDL_sound_internal.h"

/* Functions for audio drivers to perform runtime conversion of audio format */


/*
 * Toggle endianness. This filter is, of course, only applied to 16-bit
 * audio data.
 */

void Sound_ConvertEndian(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *data, tmp;

    /* SNDDBG(("Converting audio endianness\n")); */

    data = cvt->buf;

    for (i = cvt->len_cvt / 2; i; --i)
    {
        tmp = data[0];
        data[0] = data[1];
        data[1] = tmp;
        data += 2;
    } /* for */

    *format = (*format ^ 0x1000);
} /* Sound_ConvertEndian */


/*
 * Toggle signed/unsigned. Apparently this is done by toggling the most
 * significant bit of each sample.
 */

static void Sound_ConvertSign(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *data;

    /* SNDDBG(("Converting audio signedness\n")); */

    data = cvt->buf;

        /* 16-bit sound? */
    if ((*format & 0xFF) == 16)
    {
            /* Little-endian? */
        if ((*format & 0x1000) != 0x1000)
            ++data;

        for (i = cvt->len_cvt / 2; i; --i)
        {
            *data ^= 0x80;
            data += 2;
        } /* for */
    } /* if */
    else
    {
        for (i = cvt->len_cvt; i; --i)
            *data++ ^= 0x80;
    } /* else */

    *format = (*format ^ 0x8000);
} /* Sound_ConvertSign */


/*
 * Convert 16-bit to 8-bit. This is done by taking the most significant byte
 * of each 16-bit sample.
 */

static void Sound_Convert8(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *src, *dst;

    /* SNDDBG(("Converting to 8-bit\n")); */

    src = cvt->buf;
    dst = cvt->buf;

        /* Little-endian? */
    if ((*format & 0x1000) != 0x1000)
        ++src;

    for (i = cvt->len_cvt / 2; i; --i)
    {
        *dst = *src;
        src += 2;
        dst += 1;
    } /* for */

    *format = ((*format & ~0x9010) | AUDIO_U8);
    cvt->len_cvt /= 2;
} /* Sound_Convert8 */


/* Convert 8-bit to 16-bit - LSB */

static void Sound_Convert16LSB(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *src, *dst;

    /* SNDDBG(("Converting to 16-bit LSB\n")); */

    src = cvt->buf + cvt->len_cvt;
    dst = cvt->buf + cvt->len_cvt * 2;

    for (i = cvt->len_cvt; i; --i)
    {
        src -= 1;
        dst -= 2;
        dst[1] = *src;
        dst[0] = 0;
    } /* for */

    *format = ((*format & ~0x0008) | AUDIO_U16LSB);
    cvt->len_cvt *= 2;
} /* Sound_Convert16LSB */


/* Convert 8-bit to 16-bit - MSB */

static void Sound_Convert16MSB(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *src, *dst;

    /* SNDDBG(("Converting to 16-bit MSB\n")); */

    src = cvt->buf + cvt->len_cvt;
    dst = cvt->buf + cvt->len_cvt * 2;

    for (i = cvt->len_cvt; i; --i)
    {
        src -= 1;
        dst -= 2;
        dst[0] = *src;
        dst[1] = 0;
    } /* for */

    *format = ((*format & ~0x0008) | AUDIO_U16MSB);
    cvt->len_cvt *= 2;
} /* Sound_Convert16MSB */


/* Duplicate a mono channel to both stereo channels */

static void Sound_ConvertStereo(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;

    /* SNDDBG(("Converting to stereo\n")); */

        /* 16-bit sound? */
    if ((*format & 0xFF) == 16)
    {
        Uint16 *src, *dst;

        src = (Uint16 *) (cvt->buf + cvt->len_cvt);
        dst = (Uint16 *) (cvt->buf + cvt->len_cvt * 2);

        for (i = cvt->len_cvt/2; i; --i)
        {
            dst -= 2;
            src -= 1;
            dst[0] = src[0];
            dst[1] = src[0];
        } /* for */
    } /* if */
    else
    {
        Uint8 *src, *dst;

        src = cvt->buf + cvt->len_cvt;
        dst = cvt->buf + cvt->len_cvt * 2;

        for (i = cvt->len_cvt; i; --i)
        {
            dst -= 2;
            src -= 1;
            dst[0] = src[0];
            dst[1] = src[0];
        } /* for */
    } /* else */

    cvt->len_cvt *= 2;
} /* Sound_ConvertStereo */


/* Effectively mix right and left channels into a single channel */

static void Sound_ConvertMono(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Sint32 sample;
    Uint8 *u_src, *u_dst;
    Sint8 *s_src, *s_dst;

    /* SNDDBG(("Converting to mono\n")); */

    switch (*format)
    {
        case AUDIO_U8:
            u_src = cvt->buf;
            u_dst = cvt->buf;

            for (i = cvt->len_cvt / 2; i; --i)
            {
                sample = u_src[0] + u_src[1];
                *u_dst = (sample > 255) ? 255 : sample;
                u_src += 2;
                u_dst += 1;
            } /* for */
            break;

        case AUDIO_S8:
            s_src = (Sint8 *) cvt->buf;
            s_dst = (Sint8 *) cvt->buf;

            for (i = cvt->len_cvt / 2; i; --i)
            {
                sample = s_src[0] + s_src[1];
                if (sample > 127)
                    *s_dst = 127;
                else if (sample < -128)
                    *s_dst = -128;
                else
                    *s_dst = sample;

                s_src += 2;
                s_dst += 1;
            } /* for */
            break;

        case AUDIO_U16MSB:
            u_src = cvt->buf;
            u_dst = cvt->buf;

            for (i = cvt->len_cvt / 4; i; --i)
            {
                sample = (Uint16) ((u_src[0] << 8) | u_src[1])
                       + (Uint16) ((u_src[2] << 8) | u_src[3]);
                if (sample > 65535)
                {
                    u_dst[0] = 0xFF;
                    u_dst[1] = 0xFF;
                } /* if */
                else
                {
                    u_dst[1] = (sample & 0xFF);
                    sample >>= 8;
                    u_dst[0] = (sample & 0xFF);
                } /* else */
                u_src += 4;
                u_dst += 2;
            } /* for */
            break;

        case AUDIO_U16LSB:
            u_src = cvt->buf;
            u_dst = cvt->buf;

            for (i = cvt->len_cvt / 4; i; --i)
            {
                sample = (Uint16) ((u_src[1] << 8) | u_src[0])
                       + (Uint16) ((u_src[3] << 8) | u_src[2]);
                if (sample > 65535)
                {
                    u_dst[0] = 0xFF;
                    u_dst[1] = 0xFF;
                } /* if */
                else
                {
                    u_dst[0] = (sample & 0xFF);
                    sample >>= 8;
                    u_dst[1] = (sample & 0xFF);
                } /* else */
                u_src += 4;
                u_dst += 2;
            } /* for */
            break;

        case AUDIO_S16MSB:
            u_src = cvt->buf;
            u_dst = cvt->buf;

            for (i = cvt->len_cvt / 4; i; --i)
            {
                sample = (Sint16) ((u_src[0] << 8) | u_src[1])
                       + (Sint16) ((u_src[2] << 8) | u_src[3]);
                if (sample > 32767)
                {
                    u_dst[0] = 0x7F;
                    u_dst[1] = 0xFF;
                } /* if */
                else if (sample < -32768)
                {
                    u_dst[0] = 0x80;
                    u_dst[1] = 0x00;
                } /* else if */
                else
                {
                    u_dst[1] = (sample & 0xFF);
                    sample >>= 8;
                    u_dst[0] = (sample & 0xFF);
                } /* else */
                u_src += 4;
                u_dst += 2;
            } /* for */
            break;

        case AUDIO_S16LSB:
            u_src = cvt->buf;
            u_dst = cvt->buf;

            for (i = cvt->len_cvt / 4; i; --i)
            {
                sample = (Sint16) ((u_src[1] << 8) | u_src[0])
                       + (Sint16) ((u_src[3] << 8) | u_src[2]);
                if (sample > 32767)
                {
                    u_dst[1] = 0x7F;
                    u_dst[0] = 0xFF;
                } /* if */
                else if (sample < -32768)
                {
                    u_dst[1] = 0x80;
                    u_dst[0] = 0x00;
                } /* else if */
                else
                {
                    u_dst[0] = (sample & 0xFF);
                    sample >>= 8;
                    u_dst[1] = (sample & 0xFF);
                } /* else */
                u_src += 4;
                u_dst += 2;
            } /* for */
            break;
    } /* switch */

    cvt->len_cvt /= 2;
} /* Sound_ConvertMono */


/* Convert rate up by multiple of 2 */

static void Sound_RateMUL2(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *src, *dst;

    /* SNDDBG(("Converting audio rate * 2\n")); */

    src = cvt->buf + cvt->len_cvt;
    dst = cvt->buf + cvt->len_cvt*2;

        /* 8- or 16-bit sound? */
    switch (*format & 0xFF)
    {
        case 8:
            for (i = cvt->len_cvt; i; --i)
            {
                src -= 1;
                dst -= 2;
                dst[0] = src[0];
                dst[1] = src[0];
            } /* for */
            break;

        case 16:
            for (i = cvt->len_cvt / 2; i; --i)
            {
                src -= 2;
                dst -= 4;
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[1];
            } /* for */
            break;
    } /* switch */

    cvt->len_cvt *= 2;
} /* Sound_RateMUL2 */


/* Convert rate down by multiple of 2 */

static void Sound_RateDIV2(Sound_AudioCVT *cvt, Uint16 *format)
{
    int i;
    Uint8 *src, *dst;

    /* SNDDBG(("Converting audio rate / 2\n")); */

    src = cvt->buf;
    dst = cvt->buf;

        /* 8- or 16-bit sound? */
    switch (*format & 0xFF)
    {
        case 8:
            for (i = cvt->len_cvt / 2; i; --i)
            {
                dst[0] = src[0];
                src += 2;
                dst += 1;
            } /* for */
            break;

        case 16:
            for (i = cvt->len_cvt / 4; i; --i)
            {
                dst[0] = src[0];
                dst[1] = src[1];
                src += 4;
                dst += 2;
            }
            break;
    } /* switch */

    cvt->len_cvt /= 2;
} /* Sound_RateDIV2 */


/* Very slow rate conversion routine */

static void Sound_RateSLOW(Sound_AudioCVT *cvt, Uint16 *format)
{
    double ipos;
    int i, clen;
    Uint8 *output8;
    Uint16 *output16;

    /* SNDDBG(("Converting audio rate * %4.4f\n", 1.0/cvt->rate_incr)); */

    clen = (int) ((double) cvt->len_cvt / cvt->rate_incr);

    if (cvt->rate_incr > 1.0)
    {
            /* 8- or 16-bit sound? */
        switch (*format & 0xFF)
        {
            case 8:
                output8 = cvt->buf;

                ipos = 0.0;
                for (i = clen; i; --i)
                {
                    *output8 = cvt->buf[(int) ipos];
                    ipos += cvt->rate_incr;
                    output8 += 1;
                } /* for */
                break;

            case 16:
                output16 = (Uint16 *) cvt->buf;

                clen &= ~1;
                ipos = 0.0;
                for (i = clen / 2; i; --i)
                {
                    *output16 = ((Uint16 *) cvt->buf)[(int) ipos];
                    ipos += cvt->rate_incr;
                    output16 += 1;
                } /* for */
                break;
        } /* switch */
    } /* if */
    else
    {
            /* 8- or 16-bit sound */
        switch (*format & 0xFF)
        {
            case 8:
                output8 = cvt->buf + clen;

                ipos = (double) cvt->len_cvt;
                for (i = clen; i; --i)
                {
                    ipos -= cvt->rate_incr;
                    output8 -= 1;
                    *output8 = cvt->buf[(int) ipos];
                } /* for */
                break;

            case 16:
                clen &= ~1;
                output16 = (Uint16 *) (cvt->buf + clen);
                ipos = (double) cvt->len_cvt / 2;
                for (i = clen / 2; i; --i)
                {
                    ipos -= cvt->rate_incr;
                    output16 -= 1;
                    *output16 = ((Uint16 *) cvt->buf)[(int) ipos];
                } /* for */
                break;
        } /* switch */
    } /* else */

    cvt->len_cvt = clen;
} /* Sound_RateSLOW */


int Sound_ConvertAudio(Sound_AudioCVT *cvt)
{
    Uint16 format;

        /* Make sure there's data to convert */
    if (cvt->buf == NULL)
    {
        __Sound_SetError("No buffer allocated for conversion");
        return(-1);
    } /* if */

        /* Return okay if no conversion is necessary */
    cvt->len_cvt = cvt->len;
    if (cvt->filters[0] == NULL)
        return(0);

        /* Set up the conversion and go! */
    format = cvt->src_format;
    for (cvt->filter_index = 0; cvt->filters[cvt->filter_index];
         cvt->filter_index++)
    {
        cvt->filters[cvt->filter_index](cvt, &format);
    }
    return(0);
} /* Sound_ConvertAudio */


/*
 * Creates a set of audio filters to convert from one format to another.
 * Returns -1 if the format conversion is not supported, or 1 if the
 * audio filter is set up.
 */

int Sound_BuildAudioCVT(Sound_AudioCVT *cvt,
                        Uint16 src_format, Uint8 src_channels, Uint32 src_rate,
                        Uint16 dst_format, Uint8 dst_channels, Uint32 dst_rate,
                        Uint32 dst_size)
{
        /* Start off with no conversion necessary */
    cvt->needed = 0;
    cvt->filter_index = 0;
    cvt->filters[0] = NULL;
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;

        /* First filter:  Endian conversion from src to dst */
    if ((src_format & 0x1000) != (dst_format & 0x1000) &&
       ((src_format & 0xff) != 8))
    {
        SNDDBG(("Adding filter: Sound_ConvertEndian\n"));
        cvt->filters[cvt->filter_index++] = Sound_ConvertEndian;
    } /* if */

        /* Second filter: Sign conversion -- signed/unsigned */
    if ((src_format & 0x8000) != (dst_format & 0x8000))
    {
        SNDDBG(("Adding filter: Sound_ConvertSign\n"));
        cvt->filters[cvt->filter_index++] = Sound_ConvertSign;
    } /* if */

        /* Next filter:  Convert 16 bit <--> 8 bit PCM. */
    if ((src_format & 0xFF) != (dst_format & 0xFF))
    {
        switch (dst_format & 0x10FF)
        {
            case AUDIO_U8:
                SNDDBG(("Adding filter: Sound_Convert8\n"));
                cvt->filters[cvt->filter_index++] = Sound_Convert8;
                cvt->len_ratio /= 2;
                break;

            case AUDIO_U16LSB:
                SNDDBG(("Adding filter: Sound_Convert16LSB\n"));
                cvt->filters[cvt->filter_index++] = Sound_Convert16LSB;
                cvt->len_mult *= 2;
                cvt->len_ratio *= 2;
                break;

            case AUDIO_U16MSB:
                SNDDBG(("Adding filter: Sound_Convert16MSB\n"));
                cvt->filters[cvt->filter_index++] = Sound_Convert16MSB;
                cvt->len_mult *= 2;
                cvt->len_ratio *= 2;
                break;
        } /* switch */
    } /* if */

        /* Next filter:  Mono/Stereo conversion */
    if (src_channels != dst_channels)
    {
        while ((src_channels * 2) <= dst_channels)
        {
            SNDDBG(("Adding filter: Sound_ConvertStereo\n"));
            cvt->filters[cvt->filter_index++] = Sound_ConvertStereo;
            cvt->len_mult *= 2;
            src_channels *= 2;
            cvt->len_ratio *= 2;
        } /* while */

        /* This assumes that 4 channel audio is in the format:
         *     Left {front/back} + Right {front/back}
         * so converting to L/R stereo works properly.
         */
        while (((src_channels % 2) == 0) &&
               ((src_channels / 2) >= dst_channels))
        {
            SNDDBG(("Adding filter: Sound_ConvertMono\n"));
            cvt->filters[cvt->filter_index++] = Sound_ConvertMono;
            src_channels /= 2;
            cvt->len_ratio /= 2;
        } /* while */

        if ( src_channels != dst_channels ) {
            /* Uh oh.. */;
        } /* if */
    } /* if */

    /* Do rate conversion */
    cvt->rate_incr = 0.0;
    if ((src_rate / 100) != (dst_rate / 100))
    {
        Uint32 hi_rate, lo_rate;
        int len_mult;
        double len_ratio;
        void (*rate_cvt)(Sound_AudioCVT *cvt, Uint16 *format);

        if (src_rate > dst_rate)
        {
            hi_rate = src_rate;
            lo_rate = dst_rate;
            SNDDBG(("Adding filter: Sound_RateDIV2\n"));
            rate_cvt = Sound_RateDIV2;
            len_mult = 1;
            len_ratio = 0.5;
        } /* if */
        else
        {
            hi_rate = dst_rate;
            lo_rate = src_rate;
            SNDDBG(("Adding filter: Sound_RateMUL2\n"));
            rate_cvt = Sound_RateMUL2;
            len_mult = 2;
            len_ratio = 2.0;
        } /* else */

            /* If hi_rate = lo_rate*2^x then conversion is easy */
        while (((lo_rate * 2) / 100) <= (hi_rate / 100))
        {
            cvt->filters[cvt->filter_index++] = rate_cvt;
            cvt->len_mult *= len_mult;
            lo_rate *= 2;
            cvt->len_ratio *= len_ratio;
        } /* while */

            /* We may need a slow conversion here to finish up */
        if ((lo_rate / 100) != (hi_rate / 100))
        {
            if (src_rate < dst_rate)
            {
                cvt->rate_incr = (double) lo_rate / hi_rate;
                cvt->len_mult *= 2;
                cvt->len_ratio /= cvt->rate_incr;
            } /* if */
            else
            {
                cvt->rate_incr = (double) hi_rate / lo_rate;
                cvt->len_ratio *= cvt->rate_incr;
            } /* else */
            SNDDBG(("Adding filter: Sound_RateSLOW\n"));
            cvt->filters[cvt->filter_index++] = Sound_RateSLOW;
        } /* if */
    } /* if */

        /* Set up the filter information */
    if (cvt->filter_index != 0)
    {
        cvt->needed = 1;
        cvt->src_format = src_format;
        cvt->dst_format = dst_format;
        cvt->len = 0;
        cvt->buf = NULL;
        cvt->filters[cvt->filter_index] = NULL;
    } /* if */

    return(cvt->needed);
} /* Sound_BuildAudioCVT */

/* end of audio_convert.c ... */
