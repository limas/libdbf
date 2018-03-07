
struct config
{
    char *(*pf_get)(struct config *pst_config, char *s_key);
    bool (*pf_set)(struct config *pst_config, char *s_key, char *s_value);
    void *pv_config_data;
};

struct config *config_init(char *s_file_name);
bool config_deinit(struct config *pst_config);
