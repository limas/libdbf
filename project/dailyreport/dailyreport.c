#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "db.h"

struct context
{
    hdb h_ship_db;
    hdb h_cust_db;
    hdb h_item_db;

    char *date;
    char date2[11];
    char *type;
    db_pf_itor itor;
};

static bool _cmp_id(
    hdb h_db,
    const uint8_t *pu8_data1,
    const uint8_t *pu8_data2,
    void *pv_usr_data)
{
    if(0 == strcmp((char *)pu8_data1, (char *)pu8_data2))
        return true;

    return false;
}

const char s_deliver[]={0xb0, 0x65, 0xb3, 0x66, 0x00};
const char s_taichung[]={0xa5, 0x78, 0xa4, 0xa4, 0xa5, 0xab};

static bool _dump_service(
    hdb h_db,
    const struct db_record *pst_record,
    void *pv_data)
{
    struct db_var st_date;
    struct db_var st_cust_id;
    struct db_var st_cust_name;
    struct db_var st_deliv_addr;
    struct db_var st_serv_item;
    struct db_record st_rec_cust;
    struct db_record st_rec_item;
    struct context *pst_ctx;
    char *s_item;
    bool b_ret = true;

    pst_ctx = (struct context *)pv_data;
    st_date = db_field_map_data(
            h_db,
            pst_record->pu8_data,
            53);

    if(NULL == st_date.pv_data)
        return true;

    if(0 == strncmp((char *)st_date.pv_data, pst_ctx->date, 10))
    {
        st_cust_id = db_field_map_data(
                h_db,
                pst_record->pu8_data,
                0);

        st_rec_cust = db_record_find(
                pst_ctx->h_cust_db,
                0,
                1,
                st_cust_id.pv_data,
                _cmp_id,
                NULL);

        if(0 == st_rec_cust.u32_data_len)
        {
            printf("fail to find customer id [%s].", (char *)st_cust_id.pv_data);
            b_ret = false;
        }
        else
        {
            st_cust_name = db_field_map_data(
                    pst_ctx->h_cust_db,
                    st_rec_cust.pu8_data,
                    4);

            st_deliv_addr = db_field_map_data(
                    h_db,
                    pst_record->pu8_data,
                    11);

            st_serv_item = db_field_map_data(
                    h_db,
                    pst_record->pu8_data,
                    52);

            if(0 == st_serv_item.u32_data_len)
            {
                db_field_unmap_data(h_db, &st_serv_item);

                st_serv_item = db_field_map_data(
                        h_db,
                        pst_record->pu8_data,
                        51);

                st_rec_item = db_record_find(
                        pst_ctx->h_item_db,
                        0,
                        0,
                        st_serv_item.pv_data,
                        _cmp_id,
                        NULL);

                db_field_unmap_data(h_db, &st_serv_item);

                if (0 != st_rec_item.u32_data_len)
                {
                    st_serv_item = db_field_map_data(
                            pst_ctx->h_item_db,
                            st_rec_item.pu8_data,
                            1);
                }
            }

            s_item = (char *)st_serv_item.pv_data;
            if(NULL == s_item)
            {
                s_item = "";
            }

            printf("'%s','%s','%s'",
                    (char *)st_cust_name.pv_data,
                    (char *)st_deliv_addr.pv_data,
                    s_item
                  );

            printf("\n");

            db_field_unmap_data(h_db, &st_cust_id);
            db_field_unmap_data(pst_ctx->h_item_db, &st_serv_item);
            db_field_unmap_data(h_db, &st_deliv_addr);
            db_field_unmap_data(pst_ctx->h_cust_db, &st_cust_name);
        }
    }

    db_field_unmap_data(h_db, &st_date);

    return b_ret;
}

static bool _dump_deliver(
    hdb h_db,
    const struct db_record *pst_record,
    void *pv_data)
{
    struct db_var st_date;
    struct db_var st_cust_id;
    struct db_var st_cust_name;
    struct db_var st_deliv_addr;
    struct db_record st_rec_cust;
    struct context *pst_ctx;
    bool b_ret = true;

    pst_ctx = (struct context *)pv_data;
    st_date = db_field_map_data(
            h_db,
            pst_record->pu8_data,
            5);

    if(NULL == st_date.pv_data)
        return true;

    if(0 == strncmp((char *)st_date.pv_data, pst_ctx->date2, 10))
    {
        st_cust_id = db_field_map_data(
                h_db,
                pst_record->pu8_data,
                0);

        st_rec_cust = db_record_find(
                pst_ctx->h_cust_db,
                0,
                1,
                st_cust_id.pv_data,
                _cmp_id,
                NULL);

        if(0 == st_rec_cust.u32_data_len)
        {
            printf("fail to find customer id [%s].", (char *)st_cust_id.pv_data);
            b_ret = false;
        }
        else
        {
            st_cust_name = db_field_map_data(
                    pst_ctx->h_cust_db,
                    st_rec_cust.pu8_data,
                    4);

            st_deliv_addr = db_field_map_data(
                    h_db,
                    pst_record->pu8_data,
                    11);

            printf("'%s','%s','%s'",
                    (char *)st_cust_name.pv_data,
                    (char *)st_deliv_addr.pv_data,
                    s_deliver
                  );

            printf("\n");

            db_field_unmap_data(h_db, &st_cust_id);
            db_field_unmap_data(h_db, &st_deliv_addr);
            db_field_unmap_data(pst_ctx->h_cust_db, &st_cust_name);
        }
    }

    db_field_unmap_data(h_db, &st_date);

    return b_ret;
}

static bool _iterator(
    hdb h_db,
    const struct db_record *pst_record,
    void *pv_data)
{
    bool b_ret = true;
    struct context *pst_ctx = (struct context *)pv_data;
    struct db_var st_type;

    st_type = db_field_map_data(
            h_db,
            pst_record->pu8_data,
            1);

    if(0 == strncmp((char *)st_type.pv_data, pst_ctx->type, 2))
    {
        b_ret = pst_ctx->itor(h_db, pst_record, pv_data);
    }

    db_field_unmap_data(h_db, &st_type);

    return b_ret;
}

static void _convert_date_format(char *s_src, char *s_trg)
{
    int year;
    int month;
    int day;
    char *s_str = strdup(s_src);

    year = atoi(strtok(s_str, "/")) - 1911;
    month = atoi(strtok(NULL, "/"));
    day = atoi(strtok(NULL, "/"));

    sprintf(s_trg, "%04d.%02d.%02d", year, month, day);
}

int main(int argc, char **argv)
{
    struct context ctx = {0};
    char s_type_service[]={0xaa, 0x41, 0x00};
    char s_type_deliver[]={0xa5, 0x58, 0x00};

    char *s_db_ship=argv[2];
    char *s_db_cust=argv[3];
    char *s_db_item=argv[4];

    if(5 > argc)
    {
        printf("incorrect number of argument.\n");
        return 1;
    }

    ctx.h_ship_db = db_open(s_db_ship);
    if(ctx.h_ship_db == INVALID_DB_HANDLE)
    {
        printf("fail to open shippment db.\n");
        return 2;
    }

    ctx.h_cust_db = db_open(s_db_cust);
    if(ctx.h_cust_db == INVALID_DB_HANDLE)
    {
        printf("fail to open customer db.\n");
        return 2;
    }

    ctx.h_item_db = db_open(s_db_item);
    if(ctx.h_item_db == INVALID_DB_HANDLE)
    {
        printf("fail to open item db.\n");
        return 2;
    }

    ctx.date = argv[1];
    _convert_date_format(argv[1], ctx.date2);

    /* service type */
    ctx.type = s_type_service;
    ctx.itor = _dump_service;

    if(false == db_itor_init(ctx.h_ship_db, _iterator, &ctx))
        return 4;

    db_itor_start(ctx.h_ship_db);
    db_itor_deinit(ctx.h_ship_db);

    /* deliver type */
    ctx.type = s_type_deliver;
    ctx.itor = _dump_deliver;

    if(false == db_itor_init(ctx.h_ship_db, _iterator, &ctx))
        return 4;

    db_itor_start(ctx.h_ship_db);
    db_itor_deinit(ctx.h_ship_db);

    db_close(ctx.h_item_db);
    db_close(ctx.h_cust_db);
    db_close(ctx.h_ship_db);

    return 0;
}

