/*
 * Copyright (c) 2001-2012 Tony Bybell.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef DEFS_LXT_H
#define DEFS_LXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>
#include <bzlib.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef HAVE_FSEEKO
#define fseeko fseek
#define ftello ftell
#endif


typedef struct dslxt_tree_node dslxt_Tree;
struct dslxt_tree_node {
    dslxt_Tree * left, * right;
    char *item;
    unsigned int val;
};


#define LT_HDRID (0x0138)
#define LT_VERSION (0x0004)
#define LT_TRLID (0xB4)

#define LT_CLKPACK (4)
#define LT_CLKPACK_M (2)

#define LT_MVL_2	(1<<0)
#define LT_MVL_4	(1<<1)
#define LT_MVL_9	(1<<2)

#define LT_MINDICTWIDTH (16)

enum 	lt_zmode_types 	{ LT_ZMODE_NONE, LT_ZMODE_GZIP, LT_ZMODE_BZIP2 };


#ifndef _MSC_VER
typedef uint64_t                lxttime_t;
#define ULLDescriptor(x) x##ULL
typedef int64_t                 lxtotime_t;
#else
typedef unsigned __int64        lxttime_t;
#define ULLDescriptor(x) x##i64
typedef          __int64        lxtotime_t;
#endif


struct lt_timetrail
{
struct lt_timetrail *next;
lxttime_t timeval;
unsigned int position;
};


#define LT_SYMPRIME 500009

#define LT_SECTION_END				(0)
#define LT_SECTION_CHG				(1)
#define LT_SECTION_SYNC_TABLE			(2)
#define LT_SECTION_FACNAME			(3)
#define LT_SECTION_FACNAME_GEOMETRY		(4)
#define LT_SECTION_TIMESCALE			(5)
#define	LT_SECTION_TIME_TABLE			(6)
#define LT_SECTION_INITIAL_VALUE		(7)
#define LT_SECTION_DOUBLE_TEST			(8)
#define	LT_SECTION_TIME_TABLE64			(9)
#define LT_SECTION_ZFACNAME_PREDEC_SIZE		(10)
#define LT_SECTION_ZFACNAME_SIZE		(11)
#define LT_SECTION_ZFACNAME_GEOMETRY_SIZE 	(12)
#define LT_SECTION_ZSYNC_SIZE			(13)
#define LT_SECTION_ZTIME_TABLE_SIZE		(14)
#define LT_SECTION_ZCHG_PREDEC_SIZE		(15)
#define LT_SECTION_ZCHG_SIZE			(16)
#define LT_SECTION_ZDICTIONARY			(17)
#define LT_SECTION_ZDICTIONARY_SIZE		(18)
#define LT_SECTION_EXCLUDE_TABLE		(19)
#define LT_SECTION_TIMEZERO			(20)

struct lt_trace
{
FILE *handle;
gzFile zhandle;

dslxt_Tree *dict;		/* dictionary manipulation */
unsigned int mindictwidth;
unsigned int num_dict_entries;
unsigned int dict_string_mem_required;
dslxt_Tree **sorted_dict;

/* assume dict8_offset == filepos zero */
unsigned int dict16_offset;
unsigned int dict24_offset;
unsigned int dict32_offset;


int (*lt_emit_u8)(struct lt_trace *lt, int value);
int (*lt_emit_u16)(struct lt_trace *lt, int value);
int (*lt_emit_u24)(struct lt_trace *lt, int value);
int (*lt_emit_u32)(struct lt_trace *lt, int value);
int (*lt_emit_u64)(struct lt_trace *lt, int valueh, int valuel);
int (*lt_emit_double)(struct lt_trace *lt, double value);
int (*lt_emit_string)(struct lt_trace *lt, char *value);

unsigned int position;
unsigned int zfacname_predec_size, zfacname_size, zfacgeometry_size, zsync_table_size, ztime_table_size, zdictionary_size;
unsigned int zpackcount, zchg_table_size, chg_table_size;

struct lt_symbol *sym[LT_SYMPRIME];
struct lt_symbol **sorted_facs;
struct lt_symbol *symchain;
int numfacs, numfacs_bytes;
int numfacbytes;
int longestname;
lxttime_t mintime, maxtime;
int timescale;
int initial_value;

struct lt_timetrail *timehead, *timecurr, *timebuff;
int timechangecount;

struct lt_timetrail *dumpoffhead, *dumpoffcurr;
int dumpoffcount;

unsigned int change_field_offset;
unsigned int facname_offset;
unsigned int facgeometry_offset;
unsigned int time_table_offset;
unsigned int sync_table_offset;
unsigned int initial_value_offset;
unsigned int timescale_offset;
unsigned int double_test_offset;
unsigned int dictionary_offset;
unsigned int exclude_offset;
unsigned int timezero_offset;

char *compress_fac_str;
int compress_fac_len;

lxttime_t timeval; 			/* for clock induction, current time */
lxtotime_t timezero;			/* for allowing negative values */

unsigned dumpoff_active : 1;		/* when set we're not dumping */
unsigned double_used : 1;
unsigned do_strip_brackets : 1;
unsigned clock_compress : 1;
unsigned dictmode : 1;			/* dictionary compression enabled */
unsigned zmode : 2;			/* for value changes */
unsigned emitted : 1;			/* gate off change field zmode changes when set */
};


struct lt_symbol
{
struct lt_symbol *next;
struct lt_symbol *symchain;
char *name;
int namlen;

int facnum;
struct lt_symbol *aliased_to;

unsigned int rows;
int msb, lsb;
int len;
int flags;

unsigned int last_change;

lxttime_t	clk_delta;
lxttime_t	clk_prevtrans;
int		clk_numtrans;
int 		clk_prevval;
int 		clk_prevval1;
int 		clk_prevval2;
int 		clk_prevval3;
int 		clk_prevval4;
unsigned char	clk_mask;
};

#define LT_SYM_F_BITS           (0)
#define LT_SYM_F_INTEGER        (1<<0)
#define LT_SYM_F_DOUBLE         (1<<1)
#define LT_SYM_F_STRING         (1<<2)
#define LT_SYM_F_ALIAS          (1<<3)


struct lt_trace *	lt_init(const char *name);
void 			lt_close(struct lt_trace *lt);

struct lt_symbol *	lt_symbol_find(struct lt_trace *lt, const char *name);
struct lt_symbol *	lt_symbol_add(struct lt_trace *lt, const char *name, unsigned int rows, int msb, int lsb, int flags);
struct lt_symbol *	lt_symbol_alias(struct lt_trace *lt, const char *existing_name, const char *alias, int msb, int lsb);
void			lt_symbol_bracket_stripping(struct lt_trace *lt, int doit);

			/* lt_set_no_interlace implies bzip2 compression.  if you use lt_set_chg_compress before this,    */
			/* less efficient gzip compression will be used instead so make sure lt_set_no_interlace is first */
			/* if you are using it!                                                                           */

void			lt_set_no_interlace(struct lt_trace *lt);

void 			lt_set_chg_compress(struct lt_trace *lt);
void 			lt_set_clock_compress(struct lt_trace *lt);
void			lt_set_dict_compress(struct lt_trace *lt, unsigned int minwidth);
void 			lt_set_initial_value(struct lt_trace *lt, char value);
void 			lt_set_timescale(struct lt_trace *lt, int timescale);
void			lt_set_timezero(struct lt_trace *lt, lxtotime_t timeval);

int 			lt_set_time(struct lt_trace *lt, unsigned int timeval);
int 			lt_inc_time_by_delta(struct lt_trace *lt, unsigned int timeval);
int 			lt_set_time64(struct lt_trace *lt, lxttime_t timeval);
int 			lt_inc_time_by_delta64(struct lt_trace *lt, lxttime_t timeval);

			/* allows blackout regions in LXT files */

void			lt_set_dumpoff(struct lt_trace *lt);
void			lt_set_dumpon(struct lt_trace *lt);

/*
 * value change functions..note that if the value string len for
 * lt_emit_value_bit_string() is shorter than the symbol length
 * it will be left justified with the rightmost character used as
 * a repeat value that will be propagated to pad the value string out:
 *
 * "10x" for 8 bits becomes "10xxxxxx"
 * "z" for 8 bits becomes   "zzzzzzzz"
 */
int 			lt_emit_value_int(struct lt_trace *lt, struct lt_symbol *s, unsigned int row, int value);
int 			lt_emit_value_double(struct lt_trace *lt, struct lt_symbol *s, unsigned int row, double value);
int 			lt_emit_value_string(struct lt_trace *lt, struct lt_symbol *s, unsigned int row, char *value);
int 			lt_emit_value_bit_string(struct lt_trace *lt, struct lt_symbol *s, unsigned int row, char *value);

#ifdef __cplusplus
}
#endif

#endif

