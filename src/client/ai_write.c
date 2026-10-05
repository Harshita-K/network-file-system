#include "../../include/common.h"
#include <curl/curl.h>
#include <cjson/cJSON.h>

// Structure to hold response data from curl
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback function for curl to write response data
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        printf("ERROR: Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Function to call Gemini API and get AI-generated text
char* call_gemini_api(const char *prompt, const char *api_key)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    char *response_text = NULL;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // Construct the API URL
        char url[512];
        snprintf(url, sizeof(url), 
                "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s",
                api_key);

        // Create JSON payload
        cJSON *root = cJSON_CreateObject();
        cJSON *contents = cJSON_CreateArray();
        cJSON *content_item = cJSON_CreateObject();
        cJSON *parts = cJSON_CreateArray();
        cJSON *part = cJSON_CreateObject();

        cJSON_AddStringToObject(part, "text", prompt);
        cJSON_AddItemToArray(parts, part);
        cJSON_AddItemToObject(content_item, "parts", parts);
        cJSON_AddItemToArray(contents, content_item);
        cJSON_AddItemToObject(root, "contents", contents);

        char *json_payload = cJSON_Print(root);

        // Set curl options
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "ERROR: curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse the JSON response
            cJSON *response_json = cJSON_Parse(chunk.memory);
            if (response_json) {
                cJSON *candidates = cJSON_GetObjectItem(response_json, "candidates");
                if (candidates && cJSON_IsArray(candidates) && cJSON_GetArraySize(candidates) > 0) {
                    cJSON *candidate = cJSON_GetArrayItem(candidates, 0);
                    cJSON *content = cJSON_GetObjectItem(candidate, "content");
                    if (content) {
                        cJSON *parts_resp = cJSON_GetObjectItem(content, "parts");
                        if (parts_resp && cJSON_IsArray(parts_resp) && cJSON_GetArraySize(parts_resp) > 0) {
                            cJSON *part_resp = cJSON_GetArrayItem(parts_resp, 0);
                            cJSON *text = cJSON_GetObjectItem(part_resp, "text");
                            if (text && cJSON_IsString(text)) {
                                response_text = strdup(cJSON_GetStringValue(text));
                            }
                        }
                    }
                }
                
                // Check for error in response
                if (!response_text) {
                    cJSON *error = cJSON_GetObjectItem(response_json, "error");
                    if (error) {
                        cJSON *message = cJSON_GetObjectItem(error, "message");
                        if (message && cJSON_IsString(message)) {
                            fprintf(stderr, "ERROR: API Error: %s\n", cJSON_GetStringValue(message));
                        }
                    }
                }
                
                cJSON_Delete(response_json);
            }
        }

        // Cleanup
        curl_slist_free_all(headers);
        free(json_payload);
        cJSON_Delete(root);
        curl_easy_cleanup(curl);
    }

    free(chunk.memory);
    curl_global_cleanup();

    return response_text;
}

// AI Write command - handles WRITE <filename> <sentenceindex> -AI
void cmd_write_ai(char *filename, char *sentence_num_str, int nm_socket, const char *username)
{
    if (!filename || !sentence_num_str) {
        printf("Usage: WRITE <filename> <sentence_number> -AI\n");
        return;
    }

    int sentence_num = atoi(sentence_num_str);
    if (sentence_num < 1) {
        printf("ERROR: Invalid sentence number\n");
        return;
    }

    // Get API key from environment variable
    const char *api_key = getenv("GEMINI_API_KEY");
    if (!api_key || strlen(api_key) == 0) {
        printf("ERROR: GEMINI_API_KEY environment variable not set.\n");
        printf("Please set it with: export GEMINI_API_KEY=\"your-api-key-here\"\n");
        return;
    }

    // Step 1: Acquire lock from Name Server
    Packet lock_request = {0};
    lock_request.operation = OP_ACQUIRE_LOCK;
    strncpy(lock_request.username, username, MAX_USERNAME - 1);
    strncpy(lock_request.filename, filename, MAX_FILENAME - 1);
    lock_request.sentence_num = sentence_num;

    send_packet(nm_socket, &lock_request);

    Packet lock_response;
    receive_packet(nm_socket, &lock_response);

    if (lock_response.error_code != SUCCESS) {
        if (lock_response.error_code == ERR_FILE_LOCKED) {
            printf("ERROR: Sentence %d is currently locked by another user. Please try again later.\n", sentence_num);
        } else {
            printf("ERROR: %s\n", get_error_message(lock_response.error_code));
        }
        return;
    }

    // Got lock! Save the token
    char lock_token[64];
    strncpy(lock_token, lock_response.lock_token, 64);
    printf("Lock acquired for sentence %d (token: %s)\n", sentence_num, lock_token);

    // Step 2: Connect to Storage Server
    int ss_socket = connect_to_server(lock_response.ss_ip, lock_response.ss_port);
    if (ss_socket < 0) {
        printf("ERROR: Failed to connect to Storage Server\n");
        
        // Release lock since we couldn't connect
        Packet unlock_req = {0};
        unlock_req.operation = OP_RELEASE_LOCK;
        strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
        unlock_req.sentence_num = sentence_num;
        strncpy(unlock_req.lock_token, lock_token, 64);
        send_packet(nm_socket, &unlock_req);
        return;
    }

    // Step 3: Establish write session on Storage Server with a test write
    Packet lock_test = {0};
    lock_test.operation = OP_WRITE;
    strncpy(lock_test.username, username, MAX_USERNAME - 1);
    strncpy(lock_test.filename, filename, MAX_FILENAME - 1);
    lock_test.sentence_num = sentence_num;
    lock_test.word_index = 0;  // Dummy write at position 0
    strcpy(lock_test.data, "");  // Empty content to test lock and create session
    strncpy(lock_test.lock_token, lock_token, 64);

    send_packet(ss_socket, &lock_test);

    Packet lock_test_resp;
    receive_packet(ss_socket, &lock_test_resp);

    if (lock_test_resp.error_code != SUCCESS) {
        if (lock_test_resp.error_code == ERR_FILE_LOCKED) {
            printf("ERROR: Sentence %d is currently locked by another user.\n", sentence_num);
        } else {
            printf("ERROR: %s\n", get_error_message(lock_test_resp.error_code));
        }
        close(ss_socket);
        
        // Release lock from name server
        Packet unlock_req = {0};
        unlock_req.operation = OP_RELEASE_LOCK;
        strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
        unlock_req.sentence_num = sentence_num;
        strncpy(unlock_req.lock_token, lock_token, 64);
        send_packet(nm_socket, &unlock_req);
        return;
    }

    printf("\n=== AI Write Mode ===\n");
    printf("Enter your prompt for Gemini AI (the response will be written to sentence %d):\n", sentence_num);
    printf("> ");

    // Get user prompt
    char prompt[MAX_BUFFER_SIZE];
    if (!fgets(prompt, sizeof(prompt), stdin)) {
        printf("ERROR: Failed to read prompt\n");
        close(ss_socket);
        
        // Release lock
        Packet unlock_req = {0};
        unlock_req.operation = OP_RELEASE_LOCK;
        strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
        unlock_req.sentence_num = sentence_num;
        strncpy(unlock_req.lock_token, lock_token, 64);
        send_packet(nm_socket, &unlock_req);
        return;
    }

    // Remove newline
    prompt[strcspn(prompt, "\n")] = 0;

    if (strlen(prompt) == 0) {
        printf("ERROR: Empty prompt\n");
        close(ss_socket);
        
        // Release lock
        Packet unlock_req = {0};
        unlock_req.operation = OP_RELEASE_LOCK;
        strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
        unlock_req.sentence_num = sentence_num;
        strncpy(unlock_req.lock_token, lock_token, 64);
        send_packet(nm_socket, &unlock_req);
        return;
    }

    // Step 3: Call Gemini API
    printf("\nCalling Gemini API...\n");
    char *ai_response = call_gemini_api(prompt, api_key);

    if (!ai_response) {
        printf("ERROR: Failed to get response from Gemini API\n");
        close(ss_socket);
        
        // Release lock
        Packet unlock_req = {0};
        unlock_req.operation = OP_RELEASE_LOCK;
        strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
        unlock_req.sentence_num = sentence_num;
        strncpy(unlock_req.lock_token, lock_token, 64);
        send_packet(nm_socket, &unlock_req);
        return;
    }

    printf("\n=== AI Response ===\n%s\n\n", ai_response);

    // Step 4: Write the AI response to the sentence (word index 0 to replace entire sentence)
    Packet write_req = {0};
    write_req.operation = OP_WRITE;
    strncpy(write_req.username, username, MAX_USERNAME - 1);
    strncpy(write_req.filename, filename, MAX_FILENAME - 1);
    write_req.sentence_num = sentence_num;
    write_req.word_index = 0;  // Replace entire sentence
    strncpy(write_req.data, ai_response, MAX_BUFFER_SIZE - 1);
    strncpy(write_req.lock_token, lock_token, 64);

    send_packet(ss_socket, &write_req);

    Packet write_resp;
    receive_packet(ss_socket, &write_resp);

    if (write_resp.error_code == SUCCESS) {
        printf("AI-generated content written successfully to sentence %d\n", sentence_num);
    } else {
        printf("ERROR: Failed to write AI content: %s\n", get_error_message(write_resp.error_code));
    }

    free(ai_response);

    // Step 5: Release the lock on storage server
    Packet ss_unlock_req = {0};
    ss_unlock_req.operation = OP_RELEASE_LOCK;
    strncpy(ss_unlock_req.filename, filename, MAX_FILENAME - 1);
    ss_unlock_req.sentence_num = sentence_num;
    strncpy(ss_unlock_req.lock_token, lock_token, 64);
    send_packet(ss_socket, &ss_unlock_req);
    
    Packet ss_unlock_resp;
    receive_packet(ss_socket, &ss_unlock_resp);

    close(ss_socket);
    
    // Step 6: Release the lock on name server
    Packet unlock_req = {0};
    unlock_req.operation = OP_RELEASE_LOCK;
    strncpy(unlock_req.filename, filename, MAX_FILENAME - 1);
    unlock_req.sentence_num = sentence_num;
    strncpy(unlock_req.lock_token, lock_token, 64);
    send_packet(nm_socket, &unlock_req);
    
    Packet unlock_resp;
    receive_packet(nm_socket, &unlock_resp);
    
    printf("Lock released. AI write operation completed.\n");
}
