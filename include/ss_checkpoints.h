#ifndef SS_CHECKPOINTS_H
#define SS_CHECKPOINTS_H

#include "ss_state.h"

// Checkpoint operations
int handle_save_checkpoint(const char *filename, const char *tag);
int handle_view_checkpoint(const char *filename, const char *tag, char *content);
int handle_revert_checkpoint(const char *filename, const char *tag);
int handle_list_checkpoints(const char *filename, char *output);

#endif // SS_CHECKPOINTS_H
