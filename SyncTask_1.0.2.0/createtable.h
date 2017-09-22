#ifndef _CREATETABLE_H
#define	_CREATETABLE_H

#ifdef __cplusplus
extern "C" {
#endif

int CreateTableByBaseTable(char *base_table,char *table_name,char *table_pid);
int CreateTableFieldByTableName(char *table_name,char *table_field);


#ifdef __cplusplus
} // extern "C"
#endif
#endif