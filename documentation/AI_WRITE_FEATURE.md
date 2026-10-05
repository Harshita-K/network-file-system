# AI Write Feature

## Quick Reference

```bash
# Setup (one-time)
sudo apt-get install libcurl4-openssl-dev libcjson-dev
export GEMINI_API_KEY="your-api-key-here"

# Usage
WRITE <filename> <sentence_number> -AI
> [Enter your prompt]

# Example
WRITE story.txt 1 -AI
> Write a haiku about concurrent programming
```

---

## Overview

The AI Write feature allows users to generate content using Google's Gemini AI and write it directly to a file in the Network File System. Instead of manually entering word-by-word edits, users can provide a single prompt to the AI, which will generate content that gets written to the specified sentence.

## Command Syntax

```
WRITE <filename> <sentence_index> -AI
```

- `<filename>`: The name of the file to write to
- `<sentence_index>`: The sentence number where the AI-generated content will be written (1-indexed)
- `-AI`: Flag to activate AI write mode

## Setup

### Prerequisites

Before using the AI write feature, you need to install the required dependencies:

```bash
# On Ubuntu/Debian
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev libcjson-dev

# On macOS (using Homebrew)
brew install curl cjson
```

### API Key Configuration

You need a Google Gemini API key to use this feature:

1. Get your API key from [Google AI Studio](https://makersuite.google.com/app/apikey)
2. Set the environment variable:

```bash
export GEMINI_API_KEY="your-api-key-here"
```

You can add this to your `~/.bashrc` or `~/.zshrc` to make it permanent:

```bash
echo 'export GEMINI_API_KEY="your-api-key-here"' >> ~/.bashrc
source ~/.bashrc
```

## Usage Example

1. **Connect to the system:**
   ```
   ./client
   Username: alice
   ```

2. **Use AI write mode:**
   ```
   > WRITE myfile.txt 1 -AI
   Lock acquired for sentence 1 (token: abc123)
   
   === AI Write Mode ===
   Enter your prompt for Gemini AI (the response will be written to sentence 1):
   > Write a haiku about programming in C
   
   Calling Gemini API...
   
   === AI Response ===
   Pointers and structs,
   Memory carefully managed,
   Code runs lightning fast.
   
   AI-generated content written successfully to sentence 1
   Lock released. AI write operation completed.
   ```

3. **Verify the content:**
   ```
   > READ myfile.txt
   ```

## How It Works

1. **Lock Acquisition**: The system acquires an exclusive lock on the specified sentence to prevent conflicts
2. **Prompt Input**: User enters a single prompt describing what they want
3. **API Call**: The prompt is sent to Google's Gemini API
4. **Content Generation**: Gemini generates text based on the prompt
5. **Write Operation**: The AI-generated content is written to the file at the specified sentence index
6. **Lock Release**: The lock is released, allowing other users to edit the file

## Features

- **Simple Interface**: Just provide one prompt instead of multiple word-by-word edits
- **AI-Powered**: Leverages Google's Gemini AI for intelligent content generation
- **Safe Concurrency**: Uses the same locking mechanism as regular writes to prevent conflicts
- **Error Handling**: Comprehensive error handling for API failures, network issues, etc.

## Error Messages

- `ERROR: GEMINI_API_KEY environment variable not set` - You need to set your API key
- `ERROR: Sentence X is currently locked by another user` - Someone else is editing that sentence
- `ERROR: Failed to get response from Gemini API` - API call failed (check your internet connection and API key)
- `ERROR: Invalid sentence number` - Sentence number must be >= 1

## Implementation Details

### Files

- `src/client/ai_write.c` - Main implementation of AI write functionality
- `include/ai_write.h` - Header file with function declarations
- Integration in `src/client/client.c` - Command parsing and routing

### Dependencies

- **libcurl**: For making HTTPS requests to the Gemini API
- **cjson**: For parsing JSON responses from the API

### API Integration

The feature uses the Gemini Pro model via Google's REST API:
- Endpoint: `https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent`
- Method: POST
- Authentication: API key in URL parameters
- Request/Response: JSON format

## Limitations

- Requires internet connection to access Gemini API
- API rate limits apply based on your Google Cloud project settings
- Maximum prompt length is limited by MAX_BUFFER_SIZE (8192 bytes)
- API responses are truncated if they exceed MAX_BUFFER_SIZE

## Security Considerations

- Store your API key securely and never commit it to version control
- The API key is read from an environment variable to avoid hardcoding
- Network communication with Gemini API uses HTTPS/TLS encryption
- The same access control and locking mechanisms apply as with regular writes

## Troubleshooting

### Build Errors

If you get linking errors about curl or cjson:
```bash
# Verify libraries are installed
pkg-config --libs libcurl
pkg-config --libs libcjson
```

### Runtime Errors

If the API call fails:
1. Check your internet connection
2. Verify your API key is valid
3. Check if you've exceeded API rate limits
4. Review the Google Cloud Console for any API restrictions

## Future Enhancements

Possible improvements for future versions:
- Support for different AI models (GPT, Claude, etc.)
- Streaming responses for long-form content
- Context-aware prompts that include surrounding sentences
- Caching of API responses
- Batch AI operations on multiple sentences
