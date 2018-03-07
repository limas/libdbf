#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "config.h"

struct config_data
{
    char *s_key;
    char *s_value;
    struct config_data *pst_next;
};

char *_config_get(struct config *pst_config, char *s_key)
{
    struct config_data *pst_data = (struct config_data *)pst_config->pv_config_data;

    while(pst_data)
    {
        if(0 == strcmp(pst_data->s_key, s_key))
            return pst_data->s_value;

        pst_data = pst_data->pst_next;
    }

    return NULL;
}

bool _config_set(struct config *pst_config, char *s_key, char *s_value)
{

    return true;
}

struct config *config_init(char *s_file_name)
{
    char buf[512];
    FILE *fp = NULL;
    struct config *pst_config = NULL;
    struct config_data **pp_config = NULL;

    do
    {
        fp = fopen(s_file_name, "r");
        if(!fp)
            break;;

        pst_config = (struct config *)malloc(sizeof(struct config));
        if(!pst_config)
            break;

        pp_config = (struct config_data **)(&pst_config->pv_config_data);

        while(!feof(fp))
        {
            if(NULL == fgets(buf, sizeof(buf)-1, fp))
                continue;

            *pp_config = (struct config_data *)malloc(sizeof(struct config_data));

            (*pp_config)->s_key = strdup(strtok(buf, "=\n"));
            (*pp_config)->s_value = strdup(strtok(NULL, "=\n"));

            pp_config = &((*pp_config)->pst_next);
        }

        pst_config->pf_get = _config_get;
        pst_config->pf_set = _config_set;

        if(fp)
            fclose(fp);

        return pst_config;
    }while(0);

    free(pst_config);
    if(fp)
        fclose(fp);

    return NULL;
}

bool config_deinit(struct config *pst_config)
{
    return true;
}
