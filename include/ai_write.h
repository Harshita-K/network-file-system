#ifndef AI_WRITE_H
#define AI_WRITE_H

#include "common.h"

/**
 * AI Write command - handles WRITE <filename> <sentenceindex> -AI
 * 
 * This function provides an AI-powered write mode where the user enters
 * a single prompt, which is sent to the Gemini API. The AI-generated
 * response is then written to the specified sentence in the file.
 * 
 * @param filename The name of the file to write to
 * @param sentence_num_str The sentence number as a string
 * @param nm_socket Socket connected to the Name Server
 * @param username Username of the current client
 */
void cmd_write_ai(char *filename, char *sentence_num_str, int nm_socket, const char *username);

/**
 * Call Gemini API with a prompt and get AI-generated text
 * 
 * @param prompt The user's prompt for the AI
 * @param api_key The Gemini API key
 * @return Dynamically allocated string with AI response (caller must free), or NULL on error
 */
char* call_gemini_api(const char *prompt, const char *api_key);

#endif // AI_WRITE_H
