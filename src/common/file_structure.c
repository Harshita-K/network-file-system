#include "../../include/file_structure.h"
#include <ctype.h>

// Create new word node
WordNode *create_word_node(const char *word)
{
    WordNode *node = (WordNode *)malloc(sizeof(WordNode));
    if (!node)
        return NULL;

    strncpy(node->word, word, MAX_WORD_LENGTH - 1);
    node->word[MAX_WORD_LENGTH - 1] = '\0';
    node->next = NULL;

    return node;
}

// Create new sentence node
SentenceNode *create_sentence_node()
{
    SentenceNode *node = (SentenceNode *)malloc(sizeof(SentenceNode));
    if (!node)
        return NULL;

    node->words = NULL;
    node->word_count = 0;
    pthread_mutex_init(&node->lock, NULL);
    node->next = NULL;

    return node;
}

// Create new file structure
FileStructure *create_file_structure()
{
    FileStructure *file = (FileStructure *)malloc(sizeof(FileStructure));
    if (!file)
        return NULL;

    file->sentences = NULL;
    file->sentence_count = 0;
    file->total_words = 0;
    file->total_chars = 0;

    return file;
}

// Free word list
void free_word_list(WordNode *head)
{
    while (head)
    {
        WordNode *temp = head;
        head = head->next;
        free(temp);
    }
}

// Free sentence list
void free_sentence_list(SentenceNode *head)
{
    while (head)
    {
        SentenceNode *temp = head;
        head = head->next;
        free_word_list(temp->words);
        pthread_mutex_destroy(&temp->lock);
        free(temp);
    }
}

// Free file structure
void free_file_structure(FileStructure *file)
{
    if (!file)
        return;
    free_sentence_list(file->sentences);
    free(file);
}

// Add word to sentence
void add_word_to_sentence(SentenceNode *sentence, const char *word)
{
    if (!sentence || !word || strlen(word) == 0)
        return;

    WordNode *new_word = create_word_node(word);
    if (!new_word)
        return;

    if (!sentence->words)
    {
        sentence->words = new_word;
    }
    else
    {
        WordNode *curr = sentence->words;
        while (curr->next)
        {
            curr = curr->next;
        }
        curr->next = new_word;
    }

    sentence->word_count++;
}

// Check if character is sentence delimiter
int is_sentence_delimiter(char c)
{
    return (c == '.' || c == '!' || c == '?');
}

// Add sentence to file
void add_sentence_to_file(FileStructure *file, SentenceNode *sentence)
{
    if (!file || !sentence)
        return;

    if (!file->sentences)
    {
        file->sentences = sentence;
    }
    else
    {
        SentenceNode *curr = file->sentences;
        while (curr->next)
        {
            curr = curr->next;
        }
        curr->next = sentence;
    }

    file->sentence_count++;

    // Count only non-delimiter words
    WordNode *word = sentence->words;
    while (word)
    {
        if (!(is_sentence_delimiter(word->word[0]) && strlen(word->word) == 1))
        {
            file->total_words++;
        }
        word = word->next;
    }
}

// Parse text and create file structure
FileStructure *parse_text_to_file(const char *text)
{
    if (!text)
        return NULL;

    FileStructure *file = create_file_structure();
    if (!file)
        return NULL;

    SentenceNode *current_sentence = create_sentence_node();
    if (!current_sentence)
    {
        free_file_structure(file);
        return NULL;
    }

    char word_buffer[MAX_WORD_LENGTH];
    int word_index = 0;
    int i = 0;

    while (text[i] != '\0')
    {
        if (text[i] == ' ' || text[i] == '\n' || text[i] == '\t' || text[i] == '\r')
        {
            // Space or whitespace - end of word
            if (word_index > 0)
            {
                word_buffer[word_index] = '\0';
                add_word_to_sentence(current_sentence, word_buffer);
                word_index = 0;
            }
        }
        else if (is_sentence_delimiter(text[i]))
        {
            // Sentence delimiter - add current word then delimiter as separate word
            if (word_index > 0)
            {
                word_buffer[word_index] = '\0';
                add_word_to_sentence(current_sentence, word_buffer);
                word_index = 0;
            }

            // Add delimiter as a word
            char delimiter[2] = {text[i], '\0'};
            add_word_to_sentence(current_sentence, delimiter);

            // Add sentence to file if it has words
            if (current_sentence->word_count > 0)
            {
                add_sentence_to_file(file, current_sentence);
                current_sentence = create_sentence_node();
            }
        }
        else
        {
            // Regular character - add to word buffer
            if (word_index < MAX_WORD_LENGTH - 1)
            {
                word_buffer[word_index++] = text[i];
            }
        }
        i++;
    }

    // Add last word if exists
    if (word_index > 0)
    {
        word_buffer[word_index] = '\0';
        add_word_to_sentence(current_sentence, word_buffer);
    }

    // Add last sentence if it has words
    if (current_sentence->word_count > 0)
    {
        add_sentence_to_file(file, current_sentence);
    }
    else
    {
        free(current_sentence);
    }

    // Update total character count
    file->total_chars = strlen(text);

    return file;
}

// Convert file structure to text
char *file_to_text(FileStructure *file)
{
    if (!file)
        return NULL;

    // Allocate large buffer for text
    char *text = (char *)malloc(MAX_BUFFER_SIZE * 10);
    if (!text)
        return NULL;

    text[0] = '\0';
    int offset = 0;

    SentenceNode *sent = file->sentences;
    while (sent)
    {
        WordNode *word = sent->words;
        while (word)
        {
            // Add word to text
            int word_len = strlen(word->word);
            if (offset + word_len + 2 < MAX_BUFFER_SIZE * 10)
            {
                strcpy(text + offset, word->word);
                offset += word_len;

                // Add space after word unless it's a delimiter
                if (!is_sentence_delimiter(word->word[0]) && word->next)
                {
                    text[offset++] = ' ';
                }
            }
            word = word->next;
        }

        // Add space after sentence
        if (sent->next && offset < MAX_BUFFER_SIZE * 10 - 1)
        {
            text[offset++] = ' ';
        }

        sent = sent->next;
    }

    text[offset] = '\0';
    return text;
}

// Get specific sentence (1-indexed)
SentenceNode *get_sentence(FileStructure *file, int sentence_num)
{
    if (!file || sentence_num < 1 || sentence_num > file->sentence_count)
    {
        return NULL;
    }

    SentenceNode *curr = file->sentences;
    for (int i = 1; i < sentence_num && curr; i++)
    {
        curr = curr->next;
    }

    return curr;
}

// Get specific word from sentence (0-indexed)
WordNode *get_word(SentenceNode *sentence, int word_index)
{
    if (!sentence || word_index < 0 || word_index >= sentence->word_count)
    {
        return NULL;
    }

    WordNode *curr = sentence->words;
    for (int i = 0; i < word_index && curr; i++)
    {
        curr = curr->next;
    }

    return curr;
}

// Update word in sentence
int update_word(SentenceNode *sentence, int word_index, const char *new_word)
{
    WordNode *word = get_word(sentence, word_index);
    if (!word)
        return -1;

    strncpy(word->word, new_word, MAX_WORD_LENGTH - 1);
    word->word[MAX_WORD_LENGTH - 1] = '\0';

    return 0;
}

// Insert word at position
int insert_word(SentenceNode *sentence, int word_index, const char *new_word)
{
    if (!sentence)
        return -1;

    WordNode *new_node = create_word_node(new_word);
    if (!new_node)
        return -1;

    if (word_index == 0)
    {
        new_node->next = sentence->words;
        sentence->words = new_node;
    }
    else
    {
        WordNode *prev = get_word(sentence, word_index - 1);
        if (!prev)
        {
            free(new_node);
            return -1;
        }
        new_node->next = prev->next;
        prev->next = new_node;
    }

    sentence->word_count++;
    return 0;
}

// Delete word from sentence
int delete_word(SentenceNode *sentence, int word_index)
{
    if (!sentence || word_index < 0 || word_index >= sentence->word_count)
    {
        return -1;
    }

    WordNode *to_delete;

    if (word_index == 0)
    {
        to_delete = sentence->words;
        sentence->words = to_delete->next;
    }
    else
    {
        WordNode *prev = get_word(sentence, word_index - 1);
        if (!prev || !prev->next)
            return -1;

        to_delete = prev->next;
        prev->next = to_delete->next;
    }

    free(to_delete);
    sentence->word_count--;
    return 0;
}

// Get word count in sentence
int get_word_count(SentenceNode *sentence)
{
    return sentence ? sentence->word_count : 0;
}

// Get sentence count in file
int get_sentence_count(FileStructure *file)
{
    return file ? file->sentence_count : 0;
}

// Get total word count in file
int get_total_word_count(FileStructure *file)
{
    return file ? file->total_words : 0;
}

// Get total character count in file
int get_total_char_count(FileStructure *file)
{
    return file ? file->total_chars : 0;
}

// Print file structure (for debugging)
void print_file_structure(FileStructure *file)
{
    if (!file)
        return;

    printf("File Structure:\n");
    printf("Sentences: %d, Words: %d, Chars: %d\n",
           file->sentence_count, file->total_words, file->total_chars);

    int sent_num = 1;
    SentenceNode *sent = file->sentences;
    while (sent)
    {
        printf("Sentence %d (%d words): ", sent_num++, sent->word_count);
        WordNode *word = sent->words;
        while (word)
        {
            printf("%s", word->word);
            if (word->next)
                printf(" ");
            word = word->next;
        }
        printf("\n");
        sent = sent->next;
    }
}

// Clone word list
WordNode *clone_word_list(WordNode *head)
{
    if (!head)
        return NULL;

    WordNode *new_head = create_word_node(head->word);
    WordNode *curr_old = head->next;
    WordNode *curr_new = new_head;

    while (curr_old)
    {
        curr_new->next = create_word_node(curr_old->word);
        curr_new = curr_new->next;
        curr_old = curr_old->next;
    }

    return new_head;
}

// Clone sentence list
SentenceNode *clone_sentence_list(SentenceNode *head)
{
    if (!head)
        return NULL;

    SentenceNode *new_head = create_sentence_node();
    new_head->words = clone_word_list(head->words);
    new_head->word_count = head->word_count;

    SentenceNode *curr_old = head->next;
    SentenceNode *curr_new = new_head;

    while (curr_old)
    {
        curr_new->next = create_sentence_node();
        curr_new = curr_new->next;
        curr_new->words = clone_word_list(curr_old->words);
        curr_new->word_count = curr_old->word_count;
        curr_old = curr_old->next;
    }

    return new_head;
}

// Clone file structure (for undo functionality)
FileStructure *clone_file_structure(FileStructure *file)
{
    if (!file)
        return NULL;

    FileStructure *new_file = create_file_structure();
    if (!new_file)
        return NULL;

    new_file->sentences = clone_sentence_list(file->sentences);
    new_file->sentence_count = file->sentence_count;
    new_file->total_words = file->total_words;
    new_file->total_chars = file->total_chars;

    return new_file;
}
