/**
 * @file db.c
 * @author Jackal Lin
 * @date 2 Feb 2018
 * @brief The implementation of DBF type database file parser.
 * 
 * @see https://en.wikipedia.org/wiki/.dbf
 * @see https://msdn.microsoft.com/zh-tw/library/windows/hardware/aa975374
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "db.h"

#define USE_GREGORIAN_CALENDAR

#define swap_byte(a, b) \
    do { \
        a^=b; \
        b^=a; \
        a^=b; \
    }while(0)

struct db_field
{
    char s_name[12];
    uint8_t u8_type;
    uint8_t u8_len;
    uint32_t u32_acc_len;
    //uint8_t u8_flag;
    //uint16_t u16_auto_inc_next;
    //uint8_t u8_auto_inc_step;

    struct db_field *pst_next;
};

struct db_field_info
{
    uint8_t u8_field_num;
    struct db_field *pst_field;
    struct db_field **a_field;
};

struct db_file_hdr
{
    uint8_t u8_type;
    uint8_t au8_last_update[3];
    uint16_t u16_hdr_len; //can treat as offset of first record data
    uint32_t u32_rec_num;
    uint16_t u16_rec_len;
    uint8_t u8_enc;
    uint8_t u8_flag;
    uint8_t u8_code_page;
};

struct db_memo_hdr
{
    uint32_t u32_next_free;
    uint16_t u16_blk_size;
};

struct db_itor
{
    uint32_t u32_buf_len;
    uint8_t *pu8_buf;

    db_pf_itor pf_itor;
    void *pv_usr_data;
};

struct db
{
    char *s_db_name;
    FILE *pf_db;
    char *s_memo_name;
    FILE *pf_memo;

    struct db_file_hdr st_file_hdr;
    struct db_memo_hdr st_memo_hdr;

    struct db_field_info st_field_info;

    /* cache for record data */
    uint8_t *pu8_rec_cache;

    /* iterator object */
    struct db_itor *pst_itor;

    /* data buffer for find function */
    uint8_t *pu8_find_buf;
};

struct db_date
{
    uint16_t u16_year;
    uint8_t u8_month;
    uint8_t u8_day;
    uint8_t u8_day_of_week;

    uint8_t u8_hour;
    uint8_t u8_min;
    uint8_t u8_sec;
};

static void _remove_trailing_space(char *s_str)
{
    char *p_str;
    size_t t_size;

    if(s_str == NULL)
        return;

    t_size = strlen(s_str);
    if(0 == t_size)
        return;

    p_str = s_str+t_size-1;
    while(p_str > s_str)
    {
        if(*p_str != ' ')
        {
            p_str[1] = '\0';
            break;
        }

        p_str--;
    }
}

static void _db_JD_to_date(uint8_t *au8_jd, struct db_date *pst_date)
{
    uint32_t u32_jdn;
    uint32_t u32_ms;
    uint32_t u32_e, u32_f, u32_g, u32_h;

    u32_jdn = (uint32_t)(au8_jd[3]<<24) | au8_jd[2]<<16 | au8_jd[1]<<8 | au8_jd[0];
    u32_ms = (uint32_t)(au8_jd[7]<<24) | au8_jd[6]<<16 | au8_jd[5]<<8 | au8_jd[4];

#ifdef USE_GREGORIAN_CALENDAR
    u32_f = u32_jdn+1401+(((4*u32_jdn+274277)/146097)*3/4)-38;
#else
    u32_f = u32_jdn+1401;
#endif
    u32_e = 4*u32_f+3;
    u32_g = (u32_e%1461)/4;
    u32_h = 5*u32_g+2;

    pst_date->u8_day = (u32_h%153)/5+1;
    pst_date->u8_month = (u32_h/153+2)%12+1;
    pst_date->u16_year = (u32_e/1461)-4716+((12+2-pst_date->u8_month)/12);
    pst_date->u8_day_of_week = (u32_jdn+1)%7;

    pst_date->u8_hour = u32_ms/3600000;
    u32_ms %= 3600000;
    pst_date->u8_min = u32_ms/60000;
    u32_ms %= 60000;
    pst_date->u8_sec = u32_ms/1000;
}

static void _db_read_field_desc(struct db *pst_db)
{
    FILE *fp = pst_db->pf_db;
    struct db_field_info *pst_info = &pst_db->st_field_info;
    struct db_field *pst_field;
    struct db_field **ppst_field = &pst_info->pst_field;
    uint8_t u8_idx;
    uint32_t u32_acc_len = 1;
    char data[32];

    fseek(fp, 32, SEEK_SET);
    
    while(32 == fread((void *)data, 1, 32, fp))
    {
        if(0x0d == data[0])
            break;

        pst_field = (struct db_field *)calloc(1, sizeof(struct db_field));
        if(!pst_field)
            break;

        memcpy((void *)pst_field->s_name, (void *)data, 11);
        pst_field->u8_type = data[11];
        pst_field->u8_len = data[16];
        pst_field->u32_acc_len = u32_acc_len;

        *ppst_field = pst_field;
        u32_acc_len += pst_field->u8_len;
        ppst_field = &pst_field->pst_next;
        pst_info->u8_field_num++;
    }

    pst_info->a_field = (struct db_field **)malloc(pst_info->u8_field_num*sizeof(struct db_field *));
    if(!pst_info->a_field)
        return;

    u8_idx = 0;
    pst_field = pst_info->pst_field;
    while(pst_field)
    {
        pst_info->a_field[u8_idx] = pst_field;

        u8_idx++;
        pst_field = pst_field->pst_next;
    }
}

static void _db_read_file_header(struct db *pst_db)
{
    FILE *fp = pst_db->pf_db;
    struct db_file_hdr *pst_hdr = &pst_db->st_file_hdr;
    uint8_t data[32];

    fseek(fp, 0, SEEK_SET);
    fread((void *)data, 1, 32, fp);

    pst_hdr->u8_type = data[0];
    memcpy((void *)pst_hdr->au8_last_update, (void *)&data[1], 3);
    pst_hdr->u32_rec_num = data[7]<<24 | data[6]<<16 | data[5]<<8 | data[4];
    pst_hdr->u16_hdr_len = (uint16_t)(data[9]<<8) | data[8];
    pst_hdr->u16_rec_len = (uint16_t)(data[11]<<8) | data[10];
    pst_hdr->u8_enc = data[15];
    pst_hdr->u8_flag = data[28];
    pst_hdr->u8_code_page = data[29];
}

static void _db_read_memo_header(struct db *pst_db)
{
    FILE *fp = pst_db->pf_memo;
    struct db_memo_hdr *pst_hdr = &pst_db->st_memo_hdr;
    uint8_t data[8];

    fseek(fp, 0, SEEK_SET);
    fread((void *)data, 1, 8, fp);

    pst_hdr->u32_next_free = data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
    pst_hdr->u16_blk_size = (uint16_t)(data[6]<<8) | data[7];
}

static uint32_t _db_memo_read(
        struct db *pst_db,
        uint32_t u32_blk_idx,
        uint8_t *pu8_buf,
        uint32_t u32_buf_len)
{
    FILE *fp = pst_db->pf_memo;
    uint16_t u16_blk_size = pst_db->st_memo_hdr.u16_blk_size;
    uint32_t u32_len = 0;
    uint8_t au8_data[8];

    do
    {
        if(!fp)
            break;

        if(0 != fseek(fp, u16_blk_size*u32_blk_idx, SEEK_SET))
            break;

        fread((void *)au8_data, 1, 8, fp);
        u32_len = au8_data[4]<<24 |
                  au8_data[5]<<16 |
                  au8_data[6]<<8 |
                  au8_data[7];

        if((NULL != pu8_buf) && (0 != u32_buf_len))
        {
            if(u32_len > u32_buf_len)
                u32_len = u32_buf_len;

            if(u32_len != fread((void *)pu8_buf, 1, u32_len, fp))
                break;
        }

        return u32_len;
    }while(0);

    return 0;
}


static bool _db_rec_cache_init(struct db *pst_db)
{
    struct db_file_hdr *pst_hdr = &pst_db->st_file_hdr;

    pst_db->pu8_rec_cache = (uint8_t *)malloc(pst_hdr->u32_rec_num*pst_hdr->u16_rec_len);
    if(NULL == pst_db->pu8_rec_cache)
        return false;

    fseek(pst_db->pf_db, pst_hdr->u16_hdr_len, SEEK_SET);
    fread((void *)pst_db->pu8_rec_cache, 1, pst_hdr->u32_rec_num*pst_hdr->u16_rec_len, pst_db->pf_db);

    return true;
}

static bool _db_rec_cache_deinit(struct db *pst_db)
{
    free(pst_db->pu8_rec_cache);
    return true;
}

static bool _db_rec_cache_read(
        struct db *pst_db,
        uint32_t u32_rec_index,
        uint8_t *pu8_data)
{
    uint32_t u32_size = pst_db->st_file_hdr.u16_rec_len;
    uint32_t u32_offset = u32_rec_index*u32_size;

    memcpy((void *)pu8_data, (void *)&pst_db->pu8_rec_cache[u32_offset], u32_size);
    return true;
}

#if 0
static bool _db_rec_cache_write(
        struct db *pst_db,
        uint32_t u32_rec_index,
        uint8_t *pu8_data)
{
    uint32_t u32_size = pst_db->st_file_hdr.u16_rec_len;
    uint32_t u32_offset = u32_rec_index*u32_size;

    memcpy((void *)&pst_db->pu8_rec_cache[u32_offset], (void *)pu8_data, u32_size);
    return true;
}

static bool _db_rec_cache_flush(struct db *pst_db)
{
    return true;
}
#endif

static bool _db_rec_read(struct db *pst_db, uint32_t u32_rec_idx, uint8_t *pu8_data)
{
    FILE *fp = pst_db->pf_db;
    uint16_t u16_rec_len = pst_db->st_file_hdr.u16_rec_len;

    if(u32_rec_idx >= pst_db->st_file_hdr.u32_rec_num)
        return false;

    if(false == _db_rec_cache_read(pst_db, u32_rec_idx, pu8_data))
    {
        /* error handle when cache miss */
        fseek(fp, u16_rec_len*u32_rec_idx, SEEK_SET);
        
    }

    return true;
}

#if 0
static bool _db_rec_write(
        struct db *pst_db,
        uint32_t u32_rec_index,
        uint8_t *pu8_data)
{
    return true;
}
#endif

hdb db_open(char *s_file_name)
{
    struct db *pst_db = NULL;

    do
    {
        /* open db file (.dbf) */
        if(!s_file_name)
            break;

        pst_db = (struct db *)calloc(sizeof(struct db), 1);
        if(!pst_db)
            break;

        pst_db->s_db_name = strdup(s_file_name);

        pst_db->pf_db = fopen(s_file_name, "rb");
        if(NULL == pst_db->pf_db)
            break;

        _db_read_file_header(pst_db);

        /* open memo file (.fpt) if available */
        if(pst_db->st_file_hdr.u8_flag & 0x02)
        {
            pst_db->s_memo_name = strdup(s_file_name);
            sprintf(strchr(pst_db->s_memo_name, '.'), ".FPT");

            pst_db->pf_memo = fopen(pst_db->s_memo_name, "rb");
            if(NULL == pst_db->pf_memo)
            {
                printf("warning: memo file missing.\n");
            }
            else
            {
                _db_read_memo_header(pst_db);
            }
        }

        /* parsing field description */
        _db_read_field_desc(pst_db);

        _db_rec_cache_init(pst_db);

        return (hdb)pst_db;
    }while(0);

    return INVALID_DB_HANDLE;
}

bool db_close(hdb h_db)
{
    struct db *pst_db = (struct db *)h_db;

    if(pst_db->s_db_name)
        free(pst_db->s_db_name);

    if(pst_db->s_memo_name)
        free(pst_db->s_memo_name);

    if(pst_db->pf_db)
        fclose(pst_db->pf_db);

    if(pst_db->pf_memo)
        fclose(pst_db->pf_memo);

    /* free find function buffer */
    free(pst_db->pu8_find_buf);

    /* free field info */
    {
        struct db_field_info *pst_info = &pst_db->st_field_info;
        struct db_field *pst_field = pst_info->pst_field;
        struct db_field *pst_next = pst_info->pst_field->pst_next;

        while(pst_field)
        {
            free(pst_field);
            pst_field = pst_next;

            if(pst_field)
                pst_next = pst_field->pst_next;
        }

        free(pst_info->a_field);
    }

    /* deinit record cache */
    _db_rec_cache_deinit(pst_db);

    memset((void *)pst_db, 0, sizeof(struct db));
    free(pst_db);

    return true;
}

bool db_get_info(hdb h_db, struct db_info *pst_info)
{
    struct db_file_hdr *pst_hdr;

    if(INVALID_DB_HANDLE == h_db || NULL == pst_info)
    {
        return false;
    }

    pst_hdr = &((struct db *)(h_db))->st_file_hdr;

    pst_info->u8_type = pst_hdr->u8_type;
    memcpy((void *)pst_info->au8_last_update, (void *)pst_hdr->au8_last_update, 3);
    pst_info->u16_hdr_len = pst_hdr->u16_hdr_len;
    pst_info->u32_rec_num = pst_hdr->u32_rec_num;
    pst_info->u16_rec_len = pst_hdr->u16_rec_len;
    pst_info->u8_enc = pst_hdr->u8_enc;
    pst_info->u8_code_page = pst_hdr->u8_code_page;

    return true;
}

uint8_t db_field_get_num(hdb h_db)
{
    return ((struct db *)h_db)->st_field_info.u8_field_num;
}

int16_t db_field_get_idx(hdb h_db, char *s_name)
{
    struct db_field_info *pst_info = &((struct db *)h_db)->st_field_info;
    struct db_field *pst_field;
    uint8_t u8_idx = 0;

    pst_field = pst_info->pst_field;
    while(pst_field)
    {
        if(0 == strcmp(s_name, pst_field->s_name))
        {
            return u8_idx;
        }

        u8_idx++;
        pst_field = pst_field->pst_next;
    }

    return -1;
}

struct db_var db_field_map_data(
        hdb h_db,
        uint8_t *pu8_rec_data,
        uint32_t u32_field_idx)
{
    struct db_field_info *pst_info = &((struct db *)h_db)->st_field_info;
    struct db_field *pst_field;
    char *s_map_data = NULL;
    uint32_t u32_len = 0;

    if(u32_field_idx >= pst_info->u8_field_num)
        return (struct db_var){0};

    pst_field = pst_info->a_field[u32_field_idx];
    pu8_rec_data = pu8_rec_data + pst_field->u32_acc_len;

    switch(toupper(pst_field->u8_type))
    {
        case 'C':
            {
                size_t t_len;

                u32_len = pst_field->u8_len+2;
                s_map_data = (char *)calloc(1, u32_len);

                memcpy(
                    (void *)s_map_data,
                    (void *)pu8_rec_data,
                    pst_field->u8_len);

                _remove_trailing_space(s_map_data);

                t_len = strlen(s_map_data);
                if(t_len > 1)
                {
                    s_map_data[t_len] = ' ';
                    s_map_data[t_len+1] = '\0';
                }
            }
            break;
        case 'N':
            if(0x20 != pu8_rec_data[pst_field->u8_len-1])
            {
                u32_len = pst_field->u8_len+1;
                s_map_data = (char *)malloc(u32_len);

                memcpy((void *)s_map_data, (void *)pu8_rec_data, pst_field->u8_len);
                s_map_data[pst_field->u8_len] = '\0';
            }
            break;
        case 'D':
            if(0x20 != pu8_rec_data[0])
            {
                u32_len = 11;
                s_map_data = (char *)malloc(u32_len);

                sprintf(s_map_data, "%c%c%c%c/%c%c/%c%c", pu8_rec_data[0], pu8_rec_data[1], pu8_rec_data[2], pu8_rec_data[3], pu8_rec_data[4], pu8_rec_data[5], pu8_rec_data[6], pu8_rec_data[7]);
                s_map_data[10] = '\0';
            }
            break;
        case 'L':
            u32_len = 2;
            s_map_data = (char *)malloc(u32_len);

            s_map_data[0] = toupper(pu8_rec_data[0]);
            s_map_data[1] = '\0';
            break;
        case 'T':
            {
                struct db_date st_date;

                if((0 != pu8_rec_data[0]) && (0x20 != pu8_rec_data[0]))
                {
                    u32_len = 20;
                    s_map_data = (char *)malloc(u32_len);

                    _db_JD_to_date(pu8_rec_data, &st_date);
                    sprintf(
                        s_map_data,
                        "%d/%02d/%02d %02d:%02d:%02d",
                        st_date.u16_year,
                        st_date.u8_month,
                        st_date.u8_day,
                        st_date.u8_hour,
                        st_date.u8_min,
                        st_date.u8_sec);

                    s_map_data[19] = '\0';
                }
            }
            break;
        case 'M':
            {
                uint32_t u32_blk_idx;

                u32_blk_idx = (uint32_t)(pu8_rec_data[3]<<24) |
                                pu8_rec_data[2]<<16 |
                                pu8_rec_data[1]<<8 |
                                pu8_rec_data[0];

                if(0 == u32_blk_idx)
                {
                    s_map_data = NULL;
                    break;
                }

                u32_len = _db_memo_read(
                        (struct db *)h_db,
                        u32_blk_idx,
                        NULL,
                        0)+2;

                s_map_data = (char *)malloc(u32_len);

                _db_memo_read(
                        (struct db *)h_db,
                        u32_blk_idx,
                        (uint8_t *)s_map_data,
                        u32_len-2);

                s_map_data[u32_len-2] = ' ';
                s_map_data[u32_len-1] = '\0';
            }
            break;
        default:
            s_map_data = NULL;
            break;
    }

    return (struct db_var){.u8_type=pst_field->u8_type,.u32_data_len=u32_len,.pv_data=(void *)s_map_data};
}

bool db_field_unmap_data(hdb h_db, struct db_var *pst_var)
{
    free(pst_var->pv_data);

    pst_var->u8_type = 0;
    pst_var->u32_data_len = 0;
    pst_var->pv_data = NULL;

    return true;
}

bool db_field_cmp(
        hdb h_db,
        const uint8_t *pu8_rec_data,
        uint32_t u32_field_idx,
        const uint8_t *pu8_cmp_data,
        uint8_t u8_len)
{
    struct db_field_info *pst_info = &((struct db *)h_db)->st_field_info;
    struct db_field *pst_field;

    if(u32_field_idx >= pst_info->u8_field_num)
        return false;

    pst_field = pst_info->a_field[u32_field_idx];
    pu8_rec_data = pu8_rec_data + pst_field->u32_acc_len;

    if(u8_len > pst_field->u8_len)
        return false;

    if(0 == memcmp((void *)pu8_rec_data, (void *)pu8_cmp_data, u8_len))
        return true;

    return false;
}

struct db_record db_record_find(
        hdb h_db,
        uint32_t u32_start_idx,
        uint32_t u32_field_idx,
        const uint8_t *pu8_cmp_data,
        db_pf_cmp pf_cmp,
        void *pv_usr_data)
{
    struct db *pst_db = (struct db *)h_db;
    struct db_var st_id;
    uint16_t u16_data_len;
    uint8_t *pu8_data;
    bool b_equal;

    u16_data_len = pst_db->st_file_hdr.u16_rec_len;

    if(!pst_db->pu8_find_buf)
    {
        pst_db->pu8_find_buf = (uint8_t *)malloc(u16_data_len);
    }

    pu8_data = pst_db->pu8_find_buf;

    for(int idx=u32_start_idx; idx<pst_db->st_file_hdr.u32_rec_num; idx++)
    {
        _db_rec_read(pst_db, idx, pu8_data);

        st_id = db_field_map_data(
                h_db,
                pu8_data,
                u32_field_idx);

        b_equal = pf_cmp(h_db, st_id.pv_data, pu8_cmp_data, pv_usr_data);

        db_field_unmap_data(h_db, &st_id);

        if(true == b_equal)
            return (struct db_record){.u32_rec_id=idx,.u32_data_len=u16_data_len,.pu8_data=pu8_data};
    }

    return (struct db_record){0};
}

bool db_itor_init(hdb h_db, db_pf_itor pf_itor, void * pv_usr_data)
{
    struct db *pst_db = (struct db *)h_db;
    struct db_itor *pst_itor = NULL;

    if(NULL == pst_db->pst_itor)
    {
        pst_itor = (struct db_itor *)malloc(sizeof(struct db_itor));
        if(!pst_itor)
            return false;

        pst_itor->u32_buf_len = pst_db->st_file_hdr.u16_rec_len;
        pst_itor->pu8_buf = (uint8_t *)malloc(pst_itor->u32_buf_len);

        pst_itor->pf_itor = pf_itor;
        pst_itor->pv_usr_data = pv_usr_data;

        pst_db->pst_itor = pst_itor;
    }

    return true;
}

bool db_itor_start(hdb h_db)
{
    struct db *pst_db = (struct db *)h_db;
    struct db_itor *pst_itor;
    struct db_record st_record;
    db_pf_itor pf_itor;
    void *pv_usr_data;

    pst_itor = pst_db->pst_itor;
    st_record.u32_data_len = pst_itor->u32_buf_len;
    st_record.pu8_data = pst_itor->pu8_buf;

    pf_itor = pst_itor->pf_itor;
    pv_usr_data = pst_itor->pv_usr_data;

    for(int idx=0; idx<pst_db->st_file_hdr.u32_rec_num; idx++)
    {
        _db_rec_read(pst_db, idx, st_record.pu8_data);
        st_record.u32_rec_id = idx;

        if(false == pf_itor(h_db, &st_record, pv_usr_data))
            return false;
    }

    return true;
}

bool db_itor_deinit(hdb h_db)
{
    struct db_itor *pst_itor = ((struct db *)h_db)->pst_itor;

    if(pst_itor)
    {
        free(pst_itor->pu8_buf);
        free(pst_itor);
        ((struct db *)h_db)->pst_itor = NULL;
    }

    return true;
}

/* debug function */

void db_dump_field_desc(hdb h_db)
{
    struct db_field_info *pst_info = &((struct db *)h_db)->st_field_info;
    struct db_field *pst_field = pst_info->pst_field;
    uint8_t u8_count = 0;
    
    while(pst_field)
    {
        printf(
            "field %d (%s):\n"
            "  type: %c\n"
            "  length: %d\n",
            u8_count,
            pst_field->s_name,
            pst_field->u8_type,
            pst_field->u8_len);

        pst_field = pst_field->pst_next;
        u8_count++;
    }
}

static void _db_dump_record(struct db *pst_db, uint8_t *pu8_data)
{
    uint8_t u8_field_num;
    struct db_var st_var={};

    printf("'%s',",(pu8_data[0] == 0x2a)?("*"):(""));

    u8_field_num = db_field_get_num((hdb)pst_db);

    for(int idx=0; idx<u8_field_num; idx++)
    {
        st_var = db_field_map_data(
                    (hdb)pst_db,
                    pu8_data,
                    idx);

        printf("'%s',", (char *)((st_var.pv_data)?(st_var.pv_data):("")));

        db_field_unmap_data((hdb)pst_db, &st_var);
    }
}

static bool _dump_record_iter(
    hdb h_db,
    const struct db_record *pst_record,
    void *pv_data)
{
    _db_dump_record(h_db, pst_record->pu8_data);
    printf("\n");

    return true;
}

void db_dump_record(hdb h_db)
{
    FILE *fp = ((struct db *)h_db)->pf_db;
    struct db_file_hdr *pst_hdr = &((struct db *)h_db)->st_file_hdr;
    uint8_t *data = NULL;

    /* dump header */
    {
        struct db_field *pst_field = ((struct db *)h_db)->st_field_info.pst_field;

        printf("'delete',");

        while(pst_field)
        {
            printf("'%s(%c, %d)',", pst_field->s_name, pst_field->u8_type, pst_field->u8_len);
            pst_field = pst_field->pst_next;
        }

        printf("\n");
    }

    /* dump record */
    if(false == db_itor_init(h_db, _dump_record_iter, NULL))
        return;

    db_itor_start(h_db);
    db_itor_deinit(h_db);
}

