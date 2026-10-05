#ifndef SS_METADATA_H
#define SS_METADATA_H

#include "ss_state.h"

// Metadata operations
int save_metadata_db(const char *filename, int version, const char *owner, time_t last_modified, int size);
FileVersion *load_metadata_db();
int get_file_version(const char *filename);
char *get_file_owner(const char *filename);
int increment_file_version(const char *filename, const char *owner);

// File saving and loading
int save_file_to_disk(const char *filename, FileStructure *file);
FileStructure *load_file_from_disk(const char *filename);

// Undo helper
int get_file_index_for_undo(const char *filename);

#endif // SS_METADATA_H
