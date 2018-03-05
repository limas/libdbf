/**
 * @file db.h
 * @author Jackal Lin
 * @date 2 Feb 2018
 * @brief The implementation of DBF type database file parser.
 * 
 * @see https://en.wikipedia.org/wiki/.dbf
 * @see https://msdn.microsoft.com/zh-tw/library/windows/hardware/aa975374
 */

#ifndef _DB_H_
#define _DB_H_

#define INVALID_DB_HANDLE (NULL)
typedef void * hdb;
struct db_record;
struct db_collect;

typedef bool (*db_pf_itor)(
                    hdb,
                    const struct db_record *,
                    void *);

typedef bool (*db_pf_cmp)(
                    hdb,
                    const uint8_t *pu8_data1,
                    const uint8_t *pu8_data2,
                    void *pv_usr_data);

struct db_config
{
    const char *s_encode;
};

struct db_record
{
    uint32_t u32_rec_id;
    uint32_t u32_data_len;
    uint8_t *pu8_data;
};

struct db_info
{
    uint8_t u8_type;
    uint8_t au8_last_update[3];
    uint16_t u16_hdr_len;
    uint32_t u32_rec_num;
    uint16_t u16_rec_len;
    uint8_t u8_enc;
    uint8_t u8_code_page;
};

struct db_var
{
    uint8_t u8_type;
    uint32_t u32_data_len;
    void *pv_data;
};

/** @brief open database with specific name
 * 
 *  @param s_name the name of the database.
 *  @return database handle
 */
hdb db_open(char *s_name);

/** @brief close database
 * 
 *  @param h_db database handle.
 *  @return database handle
 */
bool db_close(hdb h_db);

/** @brief retrive related information to db
 * 
 *  @param h_db database handle.
 *  @param pst_info returned information.
 *  @return function call success or not
 */
bool db_get_info(hdb h_db, struct db_info *pst_info);

/** @brief get number of field
 * 
 *  @param h_db database handle
 *  @return an integer of total number of field
 */
uint8_t db_field_get_num(hdb h_db);

/** @brief lookup field index from field name
 * 
 *  @param h_db database handle.
 *  @param s_name the name of the target field.
 *  @return an integer of field index
 */
int16_t db_field_get_idx(hdb h_db, char *s_name);

/** @brief retrive data to specific field
 * 
 *  @param h_db database handle.
 *  @param pu8_data start address of record.
 *  @param u32_field_idx target field index.
 *  @return data after mapping
 *
 *  @note the mapped data/object return from this function
 *        is allocated in the context, so db_field_unmap_data
 *        must be called in paired once the data is done using
 *        to prevent memory leak.
 */
struct db_var db_field_map_data(
        hdb h_db,
        uint8_t *pu8_rec_data,
        uint32_t u32_field_idx);

/** @brief unmap mapped data
 * 
 *  @param h_db database handle.
 *  @param pst_var pointer to data which need to be unmapped.
 *  @return umapped successful
 */
bool db_field_unmap_data(
        hdb h_db,
        struct db_var *pst_var);

bool db_field_cmp(
        hdb h_db,
        const uint8_t *pu8_rec_data,
        uint32_t u32_field_idx,
        const uint8_t *pu8_cmp_data,
        uint8_t u8_len);

/** @brief find specific field data to a corresponding record
 * 
 *  @param h_db database handle.
 *  @param u32_start_idx
 *  @param u32_field_idx
 *  @param pu8_cmp_data
 *  @param pf_cmp
 *  @return
 */
struct db_record db_record_find(
        hdb h_db,
        uint32_t u32_start_idx,
        uint32_t u32_field_idx,
        const uint8_t *pu8_cmp_data,
        db_pf_cmp pf_cmp,
        void *pv_usr_data);

/* itertation function */
bool db_itor_init(hdb, db_pf_itor, void *);
bool db_itor_start(hdb);
bool db_itor_deinit(hdb);

/* debug function */
void db_dump_field_desc(hdb);
void db_dump_record(hdb);

#endif
