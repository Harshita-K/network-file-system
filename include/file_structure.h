#ifndef FILE_STRUCTURE_H
#define FILE_STRUCTURE_H

#include "common.h"

// Create new word node
WordNode* create_word_node(const char* word);

// Create new sentence node
SentenceNode* create_sentence_node();

// Create new file structure
FileStructure* create_file_structure();

// Free file structure
void free_file_structure(FileStructure* file);

// Free word list
void free_word_list(WordNode* head);

// Add word to sentence
void add_word_to_sentence(SentenceNode* sentence, const char* word);

// Add sentence to file
void add_sentence_to_file(FileStructure* file, SentenceNode* sentence);

// Parse text and create file structure
FileStructure* parse_text_to_file(const char* text);

// Convert file structure to text
char* file_to_text(FileStructure* file);

// Get specific sentence
SentenceNode* get_sentence(FileStructure* file, int sentence_num);

// Get specific word from sentence
WordNode* get_word(SentenceNode* sentence, int word_index);

// Update word in sentence
int update_word(SentenceNode* sentence, int word_index, const char* new_word);

// Insert word at position
int insert_word(SentenceNode* sentence, int word_index, const char* new_word);

// Delete word from sentence
int delete_word(SentenceNode* sentence, int word_index);

// Get word count in sentence
int get_word_count(SentenceNode* sentence);

// Get sentence count in file
int get_sentence_count(FileStructure* file);

// Get total word count in file
int get_total_word_count(FileStructure* file);

// Get total character count in file
int get_total_char_count(FileStructure* file);

// Print file structure (for debugging)
void print_file_structure(FileStructure* file);

// Clone file structure (for undo functionality)
FileStructure* clone_file_structure(FileStructure* file);

// Check if character is sentence delimiter
int is_sentence_delimiter(char c);

#endif // FILE_STRUCTURE_H
