#include "../../include/ss_state.h"
#include "../../include/ss_metadata.h"

int save_metadata_db(const char *filename, int version, const char *owner, time_t last_modified, int size)
{
    char dbpath[MAX_PATH];
    get_metadata_db_path(dbpath);
    
    // Read existing metadata
    FileVersion *head = NULL;
    FileVersion *curr = NULL;
    
    FILE *fp = fopen(dbpath, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            FileVersion *fv = (FileVersion *)malloc(sizeof(FileVersion));
            if (!fv) continue;
            
            // Parse: filename|version|owner|last_modified|size
            char *token = strtok(line, "|");
            if (token) strncpy(fv->filename, token, MAX_FILENAME - 1);
            
            token = strtok(NULL, "|");
            if (token) fv->version = atoi(token);
            
            token = strtok(NULL, "|");
            if (token) strncpy(fv->owner, token, MAX_USERNAME - 1);
            
            token = strtok(NULL, "|");
            if (token) fv->last_modified = (time_t)atol(token);
            
            token = strtok(NULL, "|");
            if (token) fv->size = atoi(token);
            
            fv->next = NULL;
            
            if (!head) {
                head = fv;
                curr = fv;
            } else {
                curr->next = fv;
                curr = fv;
            }
        }
        fclose(fp);
    }
    
    // Update or add the file version
    int found = 0;
    curr = head;
    while (curr) {
        if (strcmp(curr->filename, filename) == 0) {
            curr->version = version;
            strncpy(curr->owner, owner, MAX_USERNAME - 1);
            curr->last_modified = last_modified;
            curr->size = size;
            found = 1;
            break;
        }
        curr = curr->next;
    }
    
    if (!found) {
        FileVersion *new_fv = (FileVersion *)malloc(sizeof(FileVersion));
        if (new_fv) {
            strncpy(new_fv->filename, filename, MAX_FILENAME - 1);
            new_fv->version = version;
            strncpy(new_fv->owner, owner, MAX_USERNAME - 1);
            new_fv->last_modified = last_modified;
            new_fv->size = size;
            new_fv->next = head;
            head = new_fv;
        }
    }
    
    // Write back to file
    fp = fopen(dbpath, "w");
    if (!fp) {
        // Free memory
        while (head) {
            FileVersion *temp = head;
            head = head->next;
            free(temp);
        }
        return ERR_FILE_NOT_FOUND;
    }
    
    curr = head;
    while (curr) {
        fprintf(fp, "%s|%d|%s|%ld|%d\n", 
                curr->filename, curr->version, curr->owner, 
                (long)curr->last_modified, curr->size);
        curr = curr->next;
    }
    
    fclose(fp);
    
    // Free memory
    while (head) {
        FileVersion *temp = head;
        head = head->next;
        free(temp);
    }
    
    log_message("storage_server", "INFO", "Metadata saved for %s: version=%d", filename, version);
    return SUCCESS;
}

// Load metadata from database file
FileVersion *load_metadata_db()
{
    char dbpath[MAX_PATH];
    get_metadata_db_path(dbpath);
    
    FileVersion *head = NULL;
    FileVersion *curr = NULL;
    
    FILE *fp = fopen(dbpath, "r");
    if (!fp) {
        log_message("storage_server", "INFO", "No metadata.db found, starting fresh");
        return NULL;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        FileVersion *fv = (FileVersion *)malloc(sizeof(FileVersion));
        if (!fv) continue;
        
        // Parse: filename|version|owner|last_modified|size
        char *token = strtok(line, "|");
        if (token) strncpy(fv->filename, token, MAX_FILENAME - 1);
        
        token = strtok(NULL, "|");
        if (token) fv->version = atoi(token);
        
        token = strtok(NULL, "|");
        if (token) strncpy(fv->owner, token, MAX_USERNAME - 1);
        
        token = strtok(NULL, "|");
        if (token) fv->last_modified = (time_t)atol(token);
        
        token = strtok(NULL, "|");
        if (token) fv->size = atoi(token);
        
        fv->next = NULL;
        
        if (!head) {
            head = fv;
            curr = fv;
        } else {
            curr->next = fv;
            curr = fv;
        }
    }
    
    fclose(fp);
    log_message("storage_server", "INFO", "Metadata loaded from database");
    return head;
}

// Get file version from metadata
int get_file_version(const char *filename)
{
    FileVersion *metadata = load_metadata_db();
    int version = 0;
    
    FileVersion *curr = metadata;
    while (curr) {
        if (strcmp(curr->filename, filename) == 0) {
            version = curr->version;
            break;
        }
        curr = curr->next;
    }
    
    // Free memory
    while (metadata) {
        FileVersion *temp = metadata;
        metadata = metadata->next;
        free(temp);
    }
    
    return version;
}

// Get file owner from metadata
char *get_file_owner(const char *filename)
{
    FileVersion *metadata = load_metadata_db();
    char *owner = NULL;
    
    FileVersion *curr = metadata;
    while (curr) {
        if (strcmp(curr->filename, filename) == 0) {
            owner = strdup(curr->owner);
            break;
        }
        curr = curr->next;
    }
    
    // Free memory
    while (metadata) {
        FileVersion *temp = metadata;
        metadata = metadata->next;
        free(temp);
    }
    
    return owner ? owner : strdup("unknown");
}

// Increment file version (preserves original owner)
int increment_file_version(const char *filename, const char *owner)
{
    int version = get_file_version(filename);
    version++;
    
    // Get the original owner to preserve it
    char *original_owner = get_file_owner(filename);
    
    struct stat st;
    char filepath[MAX_PATH];
    get_file_path(filename, filepath);
    int size = 0;
    if (stat(filepath, &st) == 0) {
        size = st.st_size;
    }
    
    int result = save_metadata_db(filename, version, original_owner, time(NULL), size);
    free(original_owner);
    
    return result;
}

// Save file to disk
int save_file_to_disk(const char *filename, FileStructure *file)
{
    char filepath[MAX_PATH];
    get_file_path(filename, filepath);

    char *text = file_to_text(file);
    if (!text)
        return ERR_MEMORY_ERROR;

    FILE *fp = fopen(filepath, "w");
    if (!fp)
    {
        free(text);
        return ERR_INVALID_OPERATION;
    }

    fprintf(fp, "%s", text);
    fclose(fp);
    free(text);

    log_message("storage_server", "INFO", "File saved: %s", filename);
    return SUCCESS;
}

// Load file from disk
FileStructure *load_file_from_disk(const char *filename)
{
    char filepath[MAX_PATH];
    get_file_path(filename, filepath);

    FILE *fp = fopen(filepath, "r");
    if (!fp)
        return NULL;

    // Read entire file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *text = (char *)malloc(size + 1);
    if (!text)
    {
        fclose(fp);
        return NULL;
    }

    fread(text, 1, size, fp);
    text[size] = '\0';
    fclose(fp);

    FileStructure *file = parse_text_to_file(text);
    free(text);

    return file;
}

// Get file index for undo stack
int get_file_index_for_undo(const char *filename)
{
    for (int i = 0; i < ss.undo_count; i++)
    {
        if (strcmp(ss.undo_filenames[i], filename) == 0)
        {
            return i;
        }
    }

    // Add new file
    if (ss.undo_count < MAX_FILES_PER_SS)
    {
        int idx = ss.undo_count++;
        strncpy(ss.undo_filenames[idx], filename, MAX_FILENAME - 1);
        ss.undo_top[idx] = -1;
        return idx;
    }

    return -1;
}

// Push to undo stack
