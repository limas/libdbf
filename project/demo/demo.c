#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "db.h"
#include "config.h"

static void _dump_record(
        hdb h_db,
        const struct db_record *pst_record)
{
    uint8_t u8_field_num;
    struct db_var st_var={};

    u8_field_num = db_field_get_num(h_db);

    for(int idx=0; idx<u8_field_num; idx++)
    {
        st_var = db_field_map_data(
                    h_db,
                    pst_record->pu8_data,
                    idx);

        printf("'%s',", (char *)((st_var.pv_data)?(st_var.pv_data):("")));

        db_field_unmap_data(h_db, &st_var);
    }
}

static bool _dump_db_info(hdb h_db)
{
    struct db_info st_info;

    if(false == db_get_info(h_db, &st_info))
    {
        printf("fail to get table info.\n");
        return false;
    }

    printf("db type: 0x%02x\n", st_info.u8_type);

    printf(
            "last update: %02d/%02d/%02d\n",
            st_info.au8_last_update[0],
            st_info.au8_last_update[1],
            st_info.au8_last_update[2]);

    printf("header length: %d\n", st_info.u16_hdr_len);
    printf("number of records: %d\n", st_info.u32_rec_num);
    printf("byte(s) per records: %d\n", st_info.u16_rec_len);
    printf("is db encrypted: %s\n", st_info.u8_enc?"yes":"no");

    return true;
}

static void _dump_db_fields(hdb h_db)
{
    printf("field description:\n");
    db_dump_field_desc(h_db);
}

static bool _record_iterator(
        hdb h_db,
        const struct db_record *pst_record,
        void *pv_data)
{
    _dump_record(h_db, pst_record);
    printf("\n");

    return true;
}

static void _dump_db_records(hdb h_db)
{
    if(true == db_itor_init(h_db, _record_iterator, NULL))
    {
        db_itor_start(h_db);
        db_itor_deinit(h_db);
    }
}

int main(int argc, char **argv)
{
    struct config *pst_config;
    hdb h_db;

    pst_config = config_init(argv[1]);
    if(!pst_config)
    {
        printf("fail to init config.\n");
        return 1;
    }

    h_db = db_open(pst_config->pf_get(pst_config, "db_name"));

    _dump_db_info(h_db);
    _dump_db_fields(h_db);
    _dump_db_records(h_db);

    db_close(h_db);

    config_deinit(pst_config);

    return 0;
}
