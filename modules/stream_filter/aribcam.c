/*****************************************************************************
 * aribcam.c : ARIB STB-B25 software CAM stream filter
 *****************************************************************************
 * Copyright (C) 2014 VideoLAN and authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

#include <inttypes.h>
#include <assert.h>

#include <aribb25/arib_std_b25.h>
#include <aribb25/arib_std_b25_error_code.h>
#include <aribb25/b_cas_card.h>
#include <aribb25/b_cas_card_error_code.h>

static int  Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin ()
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_STREAM_FILTER)
    set_capability ("stream_filter", 0)
    add_shortcut("aribcam")
    set_description (N_("ARIB STD-B25 Cam module"))
    set_callbacks (Open, Close)
vlc_module_end ()

struct error_messages_s
{
    const int8_t i_error;
    const char * const psz_error;
};

static const struct error_messages_s const b25_errors[] =
{
    { ARIB_STD_B25_ERROR_INVALID_PARAM, "Invalid parameter" },
    { ARIB_STD_B25_ERROR_NO_ENOUGH_MEMORY , "Not enough memory" },
    { ARIB_STD_B25_ERROR_NON_TS_INPUT_STREAM, "Non TS input stream" },
    { ARIB_STD_B25_ERROR_NO_PAT_IN_HEAD_16M, "No PAT in first 16MB" },
    { ARIB_STD_B25_ERROR_NO_PMT_IN_HEAD_32M, "No PMT in first 32MB" },
    { ARIB_STD_B25_ERROR_NO_ECM_IN_HEAD_32M, "No ECM in first 32MB" },
    { ARIB_STD_B25_ERROR_EMPTY_B_CAS_CARD, "Empty BCAS card" },
    { ARIB_STD_B25_ERROR_INVALID_B_CAS_STATUS, "Invalid BCAS status" },
    { ARIB_STD_B25_ERROR_ECM_PROC_FAILURE, "ECM Proc failure" },
    { ARIB_STD_B25_ERROR_DECRYPT_FAILURE, "Decryption failure" },
    { ARIB_STD_B25_ERROR_PAT_PARSE_FAILURE, "PAT Parsing failure" },
    { ARIB_STD_B25_ERROR_PMT_PARSE_FAILURE, "PMT Parsing failure" },
    { ARIB_STD_B25_ERROR_ECM_PARSE_FAILURE, "ECM Parsing failure" },
    { ARIB_STD_B25_ERROR_CAT_PARSE_FAILURE, "CAT Parsing failure" },
    { ARIB_STD_B25_ERROR_EMM_PARSE_FAILURE, "EMM Parsing failure" },
    { ARIB_STD_B25_ERROR_EMM_PROC_FAILURE, "EMM Proc failure" },
    { 0, NULL },
};

static const struct error_messages_s const bcas_errors[] =
{
    { B_CAS_CARD_ERROR_INVALID_PARAMETER, "Invalid parameter" },
    { B_CAS_CARD_ERROR_NOT_INITIALIZED, "Card not initialized" },
    { B_CAS_CARD_ERROR_NO_SMART_CARD_READER, "No smart card reader" },
    { B_CAS_CARD_ERROR_ALL_READERS_CONNECTION_FAILED, "Reader connection failed" },
    { B_CAS_CARD_ERROR_NO_ENOUGH_MEMORY, "Not enough memory" },
    { B_CAS_CARD_ERROR_TRANSMIT_FAILED, "Transmission failed" },
    { 0, NULL },
};

struct stream_sys_t
{
    ARIB_STD_B25 *p_b25;
    B_CAS_CARD   *p_bcas;
    struct
    {
        uint8_t *p_buf;
        size_t   i_size;
        block_t *p_list;
    } remain;

    size_t i_pos;
    bool   b_unitsizeset;
};

static int Peek( stream_t *, const uint8_t **, unsigned int );

static const char * GetErrorMessage( const int i_error,
                               const struct error_messages_s const *p_errors_messages )
{
    int i = 0;
    while( p_errors_messages[i].psz_error )
    {
        if ( p_errors_messages[i].i_error == i_error )
            return p_errors_messages[i].psz_error;
        i++;
    }
    return "unkown error";
}

static size_t RemainRead( stream_t *p_stream, uint8_t *p_data, size_t i_toread, bool b_peek )
{
    stream_sys_t *p_sys = p_stream->p_sys;

    size_t i_total = 0;

    while( p_sys->remain.p_list && i_toread )
    {
        size_t i_copy = __MIN( i_toread, p_sys->remain.p_list->i_buffer );
        memcpy( p_data, p_sys->remain.p_list->p_buffer, i_copy );

        i_toread -= i_copy;
        i_total += i_copy;
        p_data += i_copy;

        if ( !b_peek )
        {
            /* update block data pointer and release if no longer needed */
            p_sys->remain.p_list->i_buffer -= i_copy;
            p_sys->remain.p_list->p_buffer += i_copy;
            p_sys->remain.i_size -= i_copy;

            if ( p_sys->remain.p_list->i_buffer == 0 )
            {
                block_t *p_prevhead = p_sys->remain.p_list;
                p_sys->remain.p_list = p_sys->remain.p_list->p_next;
                block_Release( p_prevhead );
            }
        }
        else
            p_sys->remain.p_list = p_sys->remain.p_list->p_next;
    }
    return i_total;
}

static bool RemainAdd( stream_t *p_stream, const uint8_t *p_data, size_t i_size )
{
    stream_sys_t *p_sys = p_stream->p_sys;
    if ( i_size == 0 )
        return true;
    block_t *p_block = block_Alloc( i_size );
    if ( !p_block )
        return false;
    memcpy( p_block->p_buffer, p_data, i_size );
    p_block->i_buffer = i_size;
    block_ChainAppend( & p_sys->remain.p_list, p_block );
    p_sys->remain.i_size += i_size;
    return true;
}

static void RemainFlush( stream_sys_t *p_sys )
{
    block_ChainRelease( p_sys->remain.p_list );
    p_sys->remain.p_list = NULL;
    p_sys->remain.i_size = 0;
}

#define ALL_READY (UNIT_SIZE_READY|ECM_READY|PMT_READY)

static int DecoderRead( stream_t *p_stream, uint8_t *p_dst, int i_toread, bool b_peek )
{
    stream_sys_t *p_sys = p_stream->p_sys;
    ARIB_STD_B25_BUFFER getbuf = { NULL, 0 };
    int i_total_read = 0;
    int i_ret;

    if ( !p_dst || ! i_toread )
        return -1;

    /* Use data from previous reads */
    size_t i_fromremain = RemainRead( p_stream, p_dst, i_toread, b_peek );
    i_toread -= i_fromremain;
    i_total_read += i_fromremain;

    while( i_toread )
    {

        do
        {
            getbuf.size = 0;

            i_ret = p_sys->p_b25->get( p_sys->p_b25, &getbuf );
            if ( i_ret < 0 )
                msg_Err( p_stream, "decoder get failed: %s",
                         GetErrorMessage( i_ret, b25_errors ) );

            /* If the decoders needs buffering or data is not ready, push some */
            if ( i_ret == 0 && getbuf.size == 0 )
            {
                /* make use of the existing buffer,
               overwritten by decoder data later */
                int i_srcread = stream_Read( p_stream->p_source, p_dst, i_toread );
                if ( i_srcread > 0 )
                {
                    ARIB_STD_B25_BUFFER putbuf = { p_dst, i_srcread };
                    i_ret = p_sys->p_b25->put( p_sys->p_b25, &putbuf );
                    if ( i_ret < 0 )
                        msg_Err( p_stream, "decoder put failed: %s",
                                 GetErrorMessage( i_ret, b25_errors ) );
                }
                else
                {
                    if ( i_srcread < 0 )
                        msg_Err( p_stream, "Can't read %d bytes from source stream: %d", i_toread, i_srcread );
                    i_ret = -1;
                }
            }
        }
        while ( i_ret == 0 && getbuf.size == 0 );

        if ( i_ret < 0 )
            return -1;

        if ( b_peek )
        {
            /* put everything in remain */
            RemainAdd( p_stream, getbuf.data, getbuf.size );
        }
        else
        {
            memcpy( p_dst, getbuf.data, __MIN(getbuf.size, i_toread) );

            if ( getbuf.size > i_toread )
            {
                /* Hold remaining data for next call */
                RemainAdd( p_stream, getbuf.data + i_toread, getbuf.size - i_toread );
            }
        }

        i_total_read += __MIN(getbuf.size, i_toread);
        p_dst += __MIN(getbuf.size, i_toread);
        i_toread -= __MIN(getbuf.size, i_toread);

    }

    return i_total_read;
}

static int Read( stream_t *p_stream, void *p_buf, unsigned int i_toread )
{
    stream_sys_t *p_sys = p_stream->p_sys;

    /* We force unit size from 1st TS packet as the lib wants to auto detect
       size but requires larger amount of data for that purpose */
    if ( !p_sys->b_unitsizeset )
    {
        if ( i_toread >= 188 && i_toread <= 320 )
            p_sys->b_unitsizeset = ! p_sys->p_b25->set_unit_size( p_sys->p_b25, i_toread );
        if ( !p_sys->b_unitsizeset )
            msg_Warn( p_stream, "Unit size not set." );
        else
            msg_Dbg( p_stream, "Set unit size to %u", i_toread );
    }

    int i_read = DecoderRead( p_stream, p_buf, i_toread, false );
    if ( i_read < 0 )
        return -1;
    else
        p_sys->i_pos += i_read;
    return i_read;
}

/**
 *
 */

static int Peek( stream_t *p_stream, const uint8_t **pp_buf, unsigned int i_len )
{
    stream_sys_t *p_sys = p_stream->p_sys;
    i_len = __MAX(ARIB_STD_B25_TS_PROBING_MIN_DATA, i_len);

    if ( i_len > p_sys->remain.i_size )
    {
        uint8_t *p_tmpbuf = malloc( i_len - p_sys->remain.i_size );
        DecoderRead( p_stream, p_tmpbuf, i_len - p_sys->remain.i_size, true );
        free( p_tmpbuf );
    }

    if ( !p_sys->remain.p_list )
    {
        assert(p_sys->remain.i_size == 0);
        /* might not be enough data in the stream to allow returning data */
        return 0;
    }

    if ( p_sys->remain.p_list->i_buffer < i_len )
    {
        p_sys->remain.p_list = block_ChainGather( p_sys->remain.p_list );
        if ( !p_sys->remain.p_list )
        {
            p_sys->remain.i_size = 0;
            return 0;
        }
    }

    *pp_buf = p_sys->remain.p_list->p_buffer;

    return __MIN(i_len, p_sys->remain.i_size);
}

static int Seek( stream_t *p_stream, uint64_t i_pos )
{
    int i_ret = stream_Seek( p_stream->p_source, i_pos );
    if ( i_ret == VLC_SUCCESS )
    {
        RemainFlush( p_stream->p_sys );
        p_stream->p_sys->i_pos = i_pos;
    }
    return i_ret;
}

/**
 *
 */
static int Control( stream_t *p_stream, int i_query, va_list args )
{
    switch( i_query )
    {
    case STREAM_GET_POSITION:
        *va_arg( args, uint64_t *) = p_stream->p_sys->i_pos;
        return VLC_SUCCESS;
    case STREAM_SET_POSITION:
        return Seek( p_stream, va_arg( args, uint64_t ) );
    default:
        return stream_vaControl( p_stream->p_source, i_query, args );
    }
}

static int Open( vlc_object_t *p_object )
{
    stream_t *p_stream = (stream_t *) p_object;

    int64_t i_stream_size = stream_Size( p_stream->p_source );
    if ( i_stream_size > 0 && i_stream_size < ARIB_STD_B25_TS_PROBING_MIN_DATA )
        return VLC_EGENERIC;

    stream_sys_t *p_sys = p_stream->p_sys = calloc( 1, sizeof(*p_sys) );
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->p_b25 = create_arib_std_b25();
    if ( p_sys->p_b25 )
    {
        if ( p_sys->p_b25->set_multi2_round( p_sys->p_b25, 4 ) < 0 )
            msg_Warn( p_stream, "cannot set B25 round number" );

        if ( p_sys->p_b25->set_strip( p_sys->p_b25, 0 ) < 0 )
            msg_Warn( p_stream, "cannot set B25 strip option" );

        if ( p_sys->p_b25->set_emm_proc( p_sys->p_b25, 0 ) < 0 )
            msg_Warn( p_stream, "cannot set B25 emm_proc" );

        p_sys->p_bcas = create_b_cas_card();
        if ( p_sys->p_bcas )
        {
            int i_code = p_sys->p_bcas->init( p_sys->p_bcas );
            if ( i_code < 0 )
            {
                /* Card could be just missing */
                msg_Warn( p_stream, "cannot initialize BCAS card (missing ?): %s",
                          GetErrorMessage( i_code, bcas_errors ) );
                goto error;
            }

            B_CAS_ID bcasid;
            if ( p_sys->p_bcas->get_id( p_sys->p_bcas, &bcasid ) == 0 )
            {
                for ( int32_t i=0; i<bcasid.count; i++)
                {
                    msg_Dbg( p_stream, "BCAS card id 0x%"PRId64" initialized",
                             bcasid.data[i] );
                }
            }

            B_CAS_INIT_STATUS bcas_status;
            if ( p_sys->p_bcas->get_init_status( p_sys->p_bcas, &bcas_status ) == 0 )
            {
                msg_Dbg( p_stream, "BCAS card system id 0x%"PRIx32,
                         bcas_status.ca_system_id );
            }

            i_code = p_sys->p_b25->set_b_cas_card( p_sys->p_b25, p_sys->p_bcas );
            if ( i_code < 0 )
            {
                msg_Err( p_stream, "cannot attach BCAS card to decoder: %s",
                         GetErrorMessage( i_code, bcas_errors ) );
                goto error;
            }
        }
        else
            msg_Err( p_stream, "cannot create BCAS card" );
    }
    else
    {
        msg_Err( p_stream, "cannot create B25 instance" );
        goto error;
    }

    p_sys->i_pos = stream_Tell( p_stream->p_source );

    p_stream->pf_read = Read;
    p_stream->pf_peek = Peek;
    p_stream->pf_control = Control;

    return VLC_SUCCESS;

error:
    Close( VLC_OBJECT(p_stream) );
    return VLC_EGENERIC;
}

static void Close ( vlc_object_t *p_object )
{
    stream_t *p_stream = (stream_t *)p_object;
    stream_sys_t *p_sys = p_stream->p_sys;

    if ( p_sys->p_bcas )
        p_sys->p_bcas->release( p_sys->p_bcas );

    if ( p_sys->p_b25 )
        p_sys->p_b25->release( p_sys->p_b25 );

    free( p_sys );
}
