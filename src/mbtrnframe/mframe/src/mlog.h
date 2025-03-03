///
/// @file mlog.h
/// @authors k. headley
/// @date 06 nov 2012
 
/// General purpose application message logging
/// configurable segmentation and rotation
/// enables formatted and timestamped output

/// @sa doxygen-examples.c for more examples of Doxygen markup
 

/////////////////////////
// Terms of use 
/////////////////////////
/*
 Copyright Information
 
 Copyright 2000-2018 MBARI
 Monterey Bay Aquarium Research Institute, all rights reserved.
 
 Terms of Use
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version. You can access the GPLv3 license at
 http://www.gnu.org/licenses/gpl-3.0.html
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details 
 (http://www.gnu.org/licenses/gpl-3.0.html)
 
 MBARI provides the documentation and software code "as is", with no warranty,
 express or implied, as to the software, title, non-infringement of third party 
 rights, merchantability, or fitness for any particular purpose, the accuracy of
 the code, or the performance or results which you may obtain from its use. You 
 assume the entire risk associated with use of the code, and you agree to be 
 responsible for the entire cost of repair or servicing of the program with 
 which you are using the code.
 
 In no event shall MBARI be liable for any damages, whether general, special,
 incidental or consequential damages, arising out of your use of the software, 
 including, but not limited to, the loss or corruption of your data or damages 
 of any kind resulting from use of the software, any prohibited use, or your 
 inability to use the software. You agree to defend, indemnify and hold harmless
 MBARI and its officers, directors, and employees against any claim, loss, 
 liability or expense, including attorneys' fees, resulting from loss of or 
 damage to property or the injury to or death of any person arising out of the 
 use of the software.
 
 The MBARI software is provided without obligation on the part of the 
 Monterey Bay Aquarium Research Institute to assist in its use, correction, 
 modification, or enhancement.
 
 MBARI assumes no responsibility or liability for any third party and/or 
 commercial software required for the database or applications. Licensee agrees 
 to obtain and maintain valid licenses for any additional third party software 
 required.
 */

// include guard
#ifndef MLOG_H
/// @def MLOG_H
/// @brief include guard
#define MLOG_H

/////////////////////////
// Includes 
/////////////////////////
#include "mfile.h"
#include "mlist.h"

/////////////////////////
// Type Definitions
/////////////////////////

/// @typedef enum mlog_flag_bits_t mlog_flag_bits_t
/// @brief mlog attribute flags
/// ML_NOLIMIT no-limit value
/// ML_MONO    monolithic log (single segment)
/// ML_DIS     disable log output
/// ML_OVWR    enable segment overwrite (rotatation)
/// ML_OSEG    segment log
/// ML_LIMLEN  limit segments by length
/// ML_LIMTIME limit segments by time
//typedef enum{ML_NOLIMIT=-1,ML_MONO=0,ML_DIS=0x1,ML_OVWR=0x2,ML_OSEG=0x4,ML_LIMLEN=0x8,ML_LIMTIME=0x10} mlog_flags_t;
typedef enum{
    ML_NOLIMIT=0x40,
    ML_MONO=0x20,
    ML_DIS=0x1,
    ML_OVWR=0x2,
    ML_OSEG=0x4,
    ML_LIMLEN=0x8,
    ML_LIMTIME=0x10
} mlog_flag_bits_t;
typedef uint32_t mlog_flags_t;

/// @typedef enum mlog_dest_bits_s mlog_dest_bits_t
/// @brief log destination flags. May be logically OR'd to use multiple
/// ML_NODEST none
/// ML_SERR   stderr
/// ML_SOUT   stdout
/// ML_FILE   file
typedef enum{
    ML_NODEST=0,
    ML_SERR=0x8,
    ML_SOUT=0x4,
    ML_FILE=0x2
} mlog_dest_bits_t;

typedef uint32_t mlog_oset_t;

typedef enum{
    ML_NONE=0,
    ML_ERR,
    ML_WARN,
    ML_INFO,
    ML_DEBUG
}mlog_level_t;

typedef struct map_entry_s{
    const char *channel;
    int level;
    mlog_oset_t dfl_set;
    mlog_oset_t dest_set;
}map_entry_t;

/// @typedef struct mlog_config_s mlog_config_t
/// @brief mlog configuration structure
typedef struct mlog_config_s
{
    /// @var mlog_config_s::lim_b
    /// @brief log segment size limit (bytes)
    uint32_t lim_b;
    /// @var mlog_config_s::lim_s
    /// @brief log segment count limit
    uint32_t lim_s;
    /// @var mlog_config_s::lim_t
    /// @brief log segment time limit
    time_t lim_t;
    /// @var mlog_config_s::flags
    /// @brief log attribute flags
    mlog_flags_t flags;
    /// @var mlog_config_s::dest
    /// @brief log output destination flags
    mlog_oset_t dest;
    /// @var mlog_config_s::tfmt
    /// @brief log timestamp format
    char *tfmt;
    /// @var mlog_config_s::del
    /// @brief log record delimiter (between timestamp, data)
    char *del;
}mlog_config_t;

/// @typedef struct mlog_s mlog_t
/// @brief mlog structure
typedef struct mlog_s
{
    /// @var mlog_s::file
    /// @brief underlying log file
    mfile_file_t *file;
    /// @var mlog_s::path
    /// @brief log filename path component
    char *path;
    /// @var mlog_s::name
    /// @brief log filename name component
    char *name;
    /// @var mlog_s::ext
    /// @brief log filename extension component
    char *ext;
    /// @var mlog_s::cfg
    /// @brief mlog configuration reference
    mlog_config_t *cfg;
    /// @var mlog_s::stime
    /// @brief log start time
    time_t stime;
    /// @var mlog_s::seg_len
    /// @brief current segment length (bytes)
    uint32_t seg_len;
    /// @var mlog_s::seg_count
    /// @brief number of log segments
    uint16_t seg_count;
    /// @var mlog_s::cur_seg
    /// @brief current active segment number
    uint16_t cur_seg;
    /// @var mlog_s::map_impl
    /// @brief map channel, level to output destination(s)
    mlist_t *omap_channel_map;
}mlog_t;

/// @typedef struct log_info_s mlog_info_t
/// @brief mlog information
typedef struct mlog_info_s
{
    /// @var log_info_s::seg_count
    /// @brief number of segments
    uint16_t seg_count;
    /// @var log_info_s::seg_min
    /// @brief least segment number
    uint16_t seg_min;
    /// @var log_info_s::seg_max
    /// @brief greatest segment number
    uint16_t seg_max;
    /// @var log_info_s::seg_b
    /// @brief oldest segment
    uint16_t seg_b;
    /// @var log_info_s::seg_e
    /// @brief newest segment
    uint16_t seg_e;
    /// @var log_info_s::tb
    /// @brief oldest segment start time (epoch seconds)
    time_t tb;
    /// @var log_info_s::te
    /// @brief newest segment start time (epoch seconds)
    time_t te;
}mlog_info_t;

/// @typedef int32_t mlog_id_t
/// @brief TBD
typedef int32_t mlog_id_t;
#define MLOG_ID_INVALID -1
/////////////////////////
// Macros
/////////////////////////

#define ML_LOG_DFL_CHANNEL "MLOG"
#define ML_LOG_DFL_EXT ".dfl"

#if defined(__unix__) || defined(__APPLE__)
/// @def ML_SYS_PATH_DEL
/// @brief path delimiter
#define ML_SYS_PATH_DEL '/'
/// @def ML_SYS_EXT_DEL
/// @brief extension delimiter
#define ML_SYS_EXT_DEL '.'
#elif defined(__WIN32) || defined(_WIN64)
#define ML_SYS_PATH_DEL '\\'
#define ML_SYS_EXT_DEL '.'
#else
#define ML_SYS_PATH_DEL '/'
#define ML_SYS_EXT_DEL '.'
#endif

/// @def ML_MAX_TS_BYTES
/// @brief max timestamp length (bytes)
#define ML_MAX_TS_BYTES    64
/// @def ML_MAX_SEG_WIDTH
/// @brief segment number digits (string width)
#define ML_MAX_SEG_WIDTH    4
/// @def ML_MAX_SEG
/// @brief max segment number
#define ML_MAX_SEG       9999

/// @def ML_TFMT_ISO8601
/// @brief ISO8601 time format specifier
#define ML_TFMT_ISO8601 "%FT%H:%M:%SZ"
#define ML_TFMT_ISO1806 ML_TFMT_ISO8601
/// @def ML_TFMT_POSIX_SEC
/// @brief Posix epoch sec
#define ML_TFMT_POSIX_SEC "%s"
/// @def ML_DFL_TFMT
/// @brief default timestamp format specifier
#define ML_DFL_TFMT     ML_TFMT_ISO8601
/// @def ML_DFL_DEL
/// @brief default record delimiter (between timestamp, data)
#define ML_DFL_DEL      ","
/// @def ML_SEG_FMT
/// @brief segment number parsing format
#define ML_SEG_FMT      "%04hd"

/// @def ML_1M
/// @brief 1 MByte
#define ML_1M 1048576
/// @def ML_1G
/// @brief 1 GByte
#define ML_1G 1073741824

/// @def MLOG_IS_BYSIZE(f)
/// @brief return true if segment size limit flag set
#define MLOG_IS_BYSIZE(f)    ((f&ML_LIMLEN    != 0) ? true : false)
/// @def MLOG_IS_BYTIME(f)
/// @brief return true if segment time limit flag set
#define MLOG_IS_BYTIME(f)    ((f&ML_LIMTIME    != 0) ? true : false)
/// @def MLOG_IS_SEGMENTED(f)
/// @brief return true if segmentation flag set
#define MLOG_IS_SEGMENTED(f) ((f&ML_OSEG   != 0) ? true : false)
/// @def MLOG_IS_ROTATE(f)
/// @brief return true if segment overwrite flag set
#define MLOG_IS_ROTATE(f)    ((f&ML_OVWR    != 0) ? true : false)
/// @def MLOG_IS_ENABLED(f)
/// @brief return true if log disable flag clear
#define MLOG_IS_ENABLED(f)   ((f&ML_DIS   == 0) ? true : false)

/////////////////////////
// Exports
/////////////////////////
#ifdef __cplusplus
extern "C" {
#endif
    
    mlog_config_t *mlog_config_new(const char *tfmt,const char *del,
                                   mlog_flags_t flags, mlog_oset_t dest,
                                   int32_t lim_b, int32_t lim_s, int32_t lim_t);
    
    void mlog_config_destroy(mlog_config_t **pself);
    
    mlog_id_t mlog_get_instance(const char *file_path, mlog_config_t *config, char *name);
    void mlog_delete_instance(mlog_id_t id);
    void mlog_delete_list(bool delete_logs);
    
    int mlog_open(mlog_id_t id,mfile_flags_t flags, mfile_mode_t mode);
    int mlog_close(mlog_id_t id);
    const char *mlog_path(mlog_id_t id);
    int mlog_delete(mlog_id_t id);
    mlog_t *mlog_get(mlog_id_t id);
    void mlog_show(mlog_id_t, bool verbose, uint16_t indent);
    
    void mlog_set_dest(mlog_id_t log, mlog_oset_t dest);
    mlog_oset_t mlog_get_dest(mlog_id_t log);
    
    int mlog_flush(mlog_id_t id);
    int mlog_write(mlog_id_t id, byte *data, uint32_t len);
    int mlog_printf(mlog_id_t id, char *fmt, ...);
    int mlog_tprintf(mlog_id_t id, const char *fmt, ...);
    int mlog_xtprintf(mlog_id_t id, const char *channel, int level, const char *fmt, ...);
    int mlog_vprintf(mlog_id_t id, char *fmt, va_list args);
    int mlog_vtprintf(mlog_id_t id, const char *fmt, va_list args);
    int mlog_vxtprintf(mlog_id_t id, const char *channel, int level, const char *fmt, va_list args);
    int mlog_puts(mlog_id_t id, char *data);
    int mlog_putc(mlog_id_t id, char data);

    const char *mlog_deststr(mlog_oset_t dest_set);
    const char *mlog_levelstr(int level);
    void mlog_map_show(mlog_id_t id, bool verbose, int indent);
    int mlog_map_dfl(mlog_id_t id, int level, mlog_oset_t dest_set);
    int mlog_unmap_dfl(mlog_id_t id, int level);
    int mlog_map_channel(mlog_id_t id, const char *channel, int level, mlog_oset_t dest_set);
    int mlog_unmap_channel(mlog_id_t id, const char *channel, int level);
    int mlog_map_channel_dfl(mlog_id_t id, const char *channel, mlog_oset_t dest_set);
    int mlog_unmap_channel_dfl(mlog_id_t id, const char *channel);
    mlog_oset_t mlog_lookup_dest(mlog_id_t id, const char *channel, int level);


#ifdef WITH_MLOG_TEST
   int mlog_test(int verbose);
#endif
#ifdef __cplusplus
}
#endif

// include guard
#endif
