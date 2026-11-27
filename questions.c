//---------------------------------------------------------
// file:	questions.c
// author:  Krishen T Selvan
// email:	K.selvan@digipen.edu
//
// brief:	Minimal JSON reader for questions.json.
//          Accepts either {"questions":[ ... ]} or a bare [ ... ].
//          Each entry: {"question":"...","options":["a","b","c","d"],"answer":0}
// 
//HOW FILE IS ORGANISED:
// 
// 1. Module DATA - the static variables that hold all state.
// 
// 2. Debug text - a log of what the loader did
// 
// 3. JSON helpers - tiny functions that read one JSON piece
// 
// 4. Question parsing - Uses the helpers to build question structs
// 
// 5. File reading - disk access and text-encoding conversion
// 
// 6. Public functions - Questions_Load / Questions_Pick
// 
// each layer only calls the layer above it, so the parser can be read and tested one piece at a time
// 
//---------------------------------------------------------
#include "cprocessing.h"
#include "questions.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//The question bank; only the first questionCount entries are valid
static Question questions[QUESTION_MAX];

//used[i] = 1 once question i has been picked
static int      used[QUESTION_MAX];

//how many valid questions are stored in questions[]
static int      questionCount;

//1 after the first Questions_Load call, so we never read twice
static int      loaded;

// -1 = not found, 0 = no valid question, >0 = count
static int      loadResult;         

//log text returned by Questions_DebugInfo
static char     debugInfo[512];

/* ------------------------------------------------------------------ */
/* Debug text                                                          */
/* ------------------------------------------------------------------ */


/*
Appends a printf-style message to debugInfo. Never overflows: 
if the buffer is full the message is dropped, and vsnprintf truncates a message
that only partly fits
*/
static void DebugLog(const char* fmt, ...)
{   
    //Current end of the log: a new text is written here
    size_t  len = strlen(debugInfo);

    //holds the variable arguments that follow fmt
    va_list args;

    //no room left (must keep 1 byte for '/0'
    if (len + 1 >= sizeof debugInfo)
        return;

    //start reading the aruguments after fmt
    va_start(args, fmt);

    //writes agter existing tect, never past the end
    vsnprintf(debugInfo + len, sizeof debugInfo - len, fmt, args);

    //clean up the argument list
    va_end(args);
}
//gives caller read only access to the log
const char* Questions_DebugInfo(void)
{
    return debugInfo;
}

/* ------------------------------------------------------------------ */
/* Tiny JSON helpers. Every function takes a pointer into the text and */
/* returns the pointer just past what it read (NULL if malformed).     */
/* ------------------------------------------------------------------ */

/*
Moves p forward over spaces, tabs and newlines (JSON ignores these between tokens)
and returns the first non-whitespace character.

The cast to unsigned char is required because isspace is undefined for negative values, which 
UTF-8 bytes would otherwise produce
*/
static const char* SkipWs(const char* p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

/* Skips whitespace, then requires the next character to be 'c'. 
Returns the pointer just past 'c' on success, or NULl if a different character
was found. Used for structural characters like {} [] : 
*/
static const char* Expect(const char* p, char c)
{
    p = SkipWs(p);
    return (*p == c) ? p + 1 : NULL;
}

/* Reads a "string" into out (if out is not NULL), handling escapes.
-cap : size of out in bytes. At most cap-1 characters are stored so there is always room for
'/0'; extra characters are read but discarded.

-Passing out = NULL reads the string without storing it, which is how SkipValue skips strings
it does not need.

Returns the pointer just past the closing quote, or NULL if the string is malformed( missing opening quote, etc..
*/
static const char* ParseString(const char* p, char* out, int cap)
{   
    //number of characters written out so far
    int n = 0;

    //must start with an opening quote
    p = Expect(p, '"');
    if (!p) return NULL;

    //stop at the closing quote or at the end of text
    while (*p && *p != '"')
    {   
        //take the current character and advance
        char c = *p++;

        //a backslash starts an escape sequence
        if (c == '\\')
        {   
            //text ended straight after a backslash: malformed
            c = *p;
            if (c == '\0') return NULL;
            p++;

            // \n -> newline
            if (c == 'n')      c = '\n';

            // \t -> tab
            else if (c == 't') c = '\t';

            // \uXXXX not supported: use'?'
            else if (c == 'u')              
            {   
                //placeholder character stored instead
                c = '?';

                //skip the 4 hex digits of the code
                for (int i = 0; i < 4 && *p; i++) p++;
            }
            /* anything else (\" \\ \/) is just the character itself */
        }

        //store only if we have a buffer and it has room
        if (out && n < cap - 1)
            out[n++] = c;
    }

    //loop ended on end-of-text: no closing quote
    if (*p != '"') return NULL;

    //terminate the stored text
    if (out) out[n] = '\0';

    //step over the closing quote
    return p + 1;
}

/* Skips any JSON value (used for keys we don't care about). 
Handles strings, nested objects/arrays (by calling itself, i.e. recursion)
and bare values such as numbers, true, false and null
*/
static const char* SkipValue(const char* p)
{
    p = SkipWs(p);

    //string: reuse ParseString without storing
    if (*p == '"')
        return ParseString(p, NULL, 0);

    //object or array
    if (*p == '{' || *p == '[')
    {   
        //the characte that ends this container
        char close = (*p == '{') ? '}' : ']';

        //objects have key/value pairs, arrays only values
        int  isObject = (*p == '{');

        //step past the opening bracket
        p = SkipWs(p + 1);
        
        //empty container: {} or []
        if (*p == close) return p + 1;
        
        //one loop pass per element
        for (;;)
        {   
            /* objects have "key": before each value */
            if (isObject)                       
            {   
                //skip the key
                p = ParseString(p, NULL, 0);

                //then the colon (only if the key was valid)
                p = p ? Expect(p, ':') : NULL;
               
                if (!p) return NULL;
            }
            //skip the value itself
            p = SkipValue(p);
            if (!p) return NULL;

            p = SkipWs(p);

            //comma : another element follows
            if (*p == ',') { p++; continue; }

            //closing bracket: container finished
            if (*p == close) return p + 1;

            //anything else is malformed
            return NULL;
        }
    }

    /* number, true, false, null */
    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p))
        p++;

    //these end at a comma, closing bracket or white space.
    return p;
}

/* ------------------------------------------------------------------ */
/* Question parsing                                                    */
/* ------------------------------------------------------------------ */

/* Reads ["a","b","c","d"] into q->options. Extra entries are ignored. 
*count receives how many options were found(the caller needs exactly 4)
Returns the pointer past the closing ']' or NULL if malformed
*/
static const char* ParseOptions(const char* p, Question* q, int* count)
{
    *count = 0;

    //options must be a JSON array
    p = Expect(p, '[');
    if (!p) return NULL;

    p = SkipWs(p);
    //empty array: count stays 0, question will be rejected later
    if (*p == ']') return p + 1;

    for (;;)
    {   
        //room left: store into options[count], then count++
        if (*count < 4)
            p = ParseString(p, q->options[(*count)++], OPTION_TEXT_LEN);
        else

            //already have 4: read and discard the extra entry
            p = SkipValue(p);
        if (!p) return NULL;

        p = SkipWs(p);

        //comma: another option follows
        if (*p == ',') { p++; continue; }

        //end of the array
        if (*p == ']') return p + 1;

        //anything else is malformed
        return NULL;
    }
}

/* Reads one {...} object into q. *valid is set to 1 only if it has a
   question, exactly 4 options, and an answer between 0 and 3. 
   
   Returns the pointer past the closing '}' or NULL if the object is malformed
   (in which case parsing of the whole array stops)
   */
static const char* ParseQuestion(const char* p, Question* q, int* valid)
{
    //flags: which required fields we have seen
    int haveText = 0, haveAnswer = 0, optionCount = 0;

    //start from a claen, all-zero question
    memset(q, 0, sizeof * q);

    //invalid until proven otherwise at the end
    *valid = 0;

    p = Expect(p, '{');
    if (!p) return NULL;

    //empty object
    p = SkipWs(p);
    if (*p == '}') return p + 1;              

    //one loop pass per "key": value pair
    for (;;)
    {   
        //key names are short; longer ones are truncated and will not match
        char key[32];

        //read the key
        p = ParseString(p, key, sizeof key);

        //then the colon
        p = p ? Expect(p, ':') : NULL;
        if (!p) return NULL;
        
        //the question text
        if (strcmp(key, "question") == 0)
        {
            p = ParseString(SkipWs(p), q->text, QUESTION_TEXT_LEN);
            haveText = 1;
        }
        //the array of answer choices
        else if (strcmp(key, "options") == 0)
        {
            p = ParseOptions(p, q, &optionCount);
        }
        //index of correct options
        else if (strcmp(key, "answer") == 0)
        {   
            //conver the number text into int (atoi stops at the first non-digit
            q->answer = atoi(SkipWs(p));
            haveAnswer = 1;

            //atoi does not move p, so skip past the number manually
            p = SkipValue(p);
        }
        else
        {
            p = SkipValue(p);                   /* unknown key: ignore it */
        }
        if (!p) return NULL;

        p = SkipWs(p);

        //comma: another pair follows
        if (*p == ',') { p++; continue; }

        //end of the object
        if (*p == '}') { p++; break; }

        //anything else is malformed
        return NULL;
    }

    //the question is usable only if every required field was present and in-range
    *valid = haveText && haveAnswer && optionCount == 4 &&
        q->answer >= 0 && q->answer < 4;
    return p;
}

/* Reads [ {...}, {...}, ... ] and stores every valid question. 
Invalid questions are skipped silently; a structurally broken file stops the parse
but keeps the questions already read.
*/
static void ParseQuestionArray(const char* p)
{
    p = Expect(p, '[');
    if (!p) return;

    p = SkipWs(p);
    //empty array: nothing to load
    if (*p == ']') return;

    for (;;)
    {   
        //temporary: copied into the bank only if valid
        Question q;
        int      valid;

        p = ParseQuestion(p, &q, &valid);
        //malformed object: stop here
        if (!p) return;

        if (valid && questionCount < QUESTION_MAX)

            //store it and increase the count
            questions[questionCount++] = q;

        p = SkipWs(p);
        //']' or garbage: we're done
        if (*p != ',') return;        

        //step over the comman to the next object
        p++;
    }
}

/* ------------------------------------------------------------------ */
/* File reading                                                        */
/* ------------------------------------------------------------------ */

/* Reads a whole file into a new null-terminated buffer (caller frees).
   Returns NULL if the file can't be opened. *size is 0 for an empty file. 
   
   The file is opened in binary mode so Windows does not translate line endings.
   Otherwise, the size from ftell would not match what fread returns
   */
static char* ReadWholeFile(const char* path, long* size)
{
    FILE* f = NULL;
    char* buf;

    //visual studio warns that fopen is unsafe so use fopen_s there
#ifdef _MSC_VER
    if (fopen_s(&f, path, "rb") != 0) f = NULL;
#else

    //other compilers: standard fopen
    f = fopen(path, "rb");
#endif

    //could not open: file does not exist here
    if (!f) return NULL;

    //jump to the end
    fseek(f, 0, SEEK_END);

    //the position there is the file size
    *size = ftell(f);

    //jump back to the start to read it
    fseek(f, 0, SEEK_SET);

    //empty file or size error
    if (*size <= 0)
    {
        *size = 0;
        fclose(f);
        //empty but "found"
        return (char*)calloc(1, 1);            
    }

    //+1 byte for the '/0' terminator
    buf = (char*)malloc((size_t)*size + 1);
    if (buf)
    {   
        // read it all; keep the coount actually read
        *size = (long)fread(buf, 1, (size_t)*size, f);

        //terminate so string functions are safe
        buf[*size] = '\0';
    }
    fclose(f);
    //NULL here means alloc failed
    return buf;
}

/* 
If buf is UTF-16 (has an FF FE / FE FF marker), converts it to UTF-8.
Always returns a buffer the caller must free; buf itself is freed if replaced.
*/
static char* ToUtf8(char* buf, long size)
{   

    //first two bytes: the byte-order mark
    unsigned char b0 = (unsigned char)buf[0];

    //safe to read: buf awlays has size+1 bytes and size >=1 here
    unsigned char b1 = (unsigned char)buf[1];

    //if the high byte of each character comes first
    int  bigEndian;

    //units = number of 16-bit characters; n = bytes written
    long units, i, n = 0;
    char* utf8;

    if (size < 2 || !((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)))
        //already UTF-8 / ASCII
        return buf;                         

    //FE FF =  big-endian, FF FE = little-endian
    bigEndian = (b0 == 0xFE);

    //skip the 2 BOM bytes; each character is 2 bytes
    units = (size - 2) / 2;

    //worst case is 3 UTF-8 bytes per character, +1 for '/0'
    utf8 = (char*)malloc((size_t)units * 3 + 1);

    //out of memory: fall back to the original text
    if (!utf8) return buf;

    for (i = 0; i < units; i++)
    {   
        //first byte for this 16-bit character
        unsigned char first = (unsigned char)buf[2 + i * 2];

        //second byte
        unsigned char second = (unsigned char)buf[3 + i * 2];

        //combine into one value in the right byte order
        unsigned int  c = bigEndian ? ((first << 8) | second) : ((second << 8) | first);

        //1-byte UTF-8(plain ASCII)
        if (c < 0x80)
        {
            utf8[n++] = (char)c;
        }

        //2-byte UTF-8
        else if (c < 0x800)
        {
            utf8[n++] = (char)(0xC0 | (c >> 6));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        //3-byte UTF-8
        else
        {
            utf8[n++] = (char)(0xE0 | (c >> 12));
            utf8[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    //terminate the converted text
    utf8[n] = '\0';
    //the UTF-16 original is no longer needed
    free(buf);
    return utf8;
}

/* Turns file text into questions. Takes ownership of buf and frees it. */
static void ParseText(char* buf, long size)
{   
    //read cursor buf is kept seperately so it can be freed
    const char* p;

    //normalise encoding first
    buf = ToUtf8(buf, size);
    p = buf;

    /* skip UTF-8 BOM if present */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    p = SkipWs(p);

    /* {"questions":[ ... ]} -> jump to the array */
    if (*p == '{')                            
    {   
        //find the questions key
        p = strstr(p, "\"questions\"");

        //then the '[' that starts its array
        p = p ? strchr(p, '[') : NULL;
    }
    //a bare array or one we just located
    if (p && *p == '[')
        ParseQuestionArray(p);

    //all needed data was copied into questions
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Public functions                                                    */
/* ------------------------------------------------------------------ */

/* Pass just the file name, e.g. "questions.json". It is tried in several
   folders; empty copies or copies with no valid questions are skipped so a
   stray empty file can't hide the real one.
   Returns -1 if no copy was found, 0 if copies were found but held no valid
   questions, else the number of questions loaded. */
int Questions_Load(const char* name)
{   
    /*Folders to search, in order. The game may be launched from the project
       folder, the build folder or Visual Studio, so several relative paths
       are tried. "" means the current working directory.
       */
    static const char* folders[] = { "", "Assets/", "../Assets/", "../", "../../Assets/", "Bouncy Bullet/Assets/" };

    /* number of entries in the array */
    int folderCount = (int)(sizeof folders / sizeof folders[0]);

    //becomes 1 if ANY copy of the file could be opened
    int fileFound = 0;

    int i;

    //already loaded once; return the saved result
    if (loaded)
        return loadResult;
    loaded = 1;
    //assume "not found" until proven otherwise
    loadResult = -1;

    //start with an empty debug log
    debugInfo[0] = '\0';

    //stop as soon as one folder gives at least one valid question
    for (i = 0; i < folderCount && questionCount == 0; i++)
    {   

        //full path = folder + file name (260 = Windows MAX_PATH)
        char  path[260];
        long  size = 0;
        char* buf;

        //build the path safely
        snprintf(path, sizeof path, "%s%s", folders[i], name);

        buf = ReadWholeFile(path, &size);
        if (!buf)
            continue;                           /* not in this folder */

        fileFound = 1;

        //file exists here but is empty
        if (size == 0)
        {
            DebugLog("[%s: EMPTY] ", path);
            free(buf);

            //keep searching the other folders
            continue;
        }

        /* grab the first bytes for the debug text before parsing frees buf */
        {
            int b0 = (unsigned char)buf[0];
            int b1 = (size > 1) ? (unsigned char)buf[1] : 0;
            int b2 = (size > 2) ? (unsigned char)buf[2] : 0;

            //parses and frees buf; fills questions[]
            ParseText(buf, size);
         
            /* first bytes reveal encoding problems (FF FE = UTF-16, EF BB BF = UTF-8 BOM) */
            DebugLog("[%s: %ld bytes, starts %02X %02X %02X -> %d questions] ",
                path, size, b0, b1, b2, questionCount);
        }
    }
    //success: report the count
    if (questionCount > 0) loadResult = questionCount;

    //files existed but none had valid questions
    else if (fileFound)    loadResult = 0;


    //otherwise stays -1
    return loadResult;
}

/* Random question that hasn't been used yet. Once all have been used, the
   list resets and starts over. This is a "shuffle bag": every question appears exactly
   once per cycle, so the player never sees an immediate repeat
   */
const Question* Questions_Pick(void)
{   
    //how many questions are still unpicked this cycle
    int unusedCount = 0;
    int target, i;

    //nothing loaded (or Question_Load not called)
    if (questionCount == 0)
        return NULL;

    //count the questions not yet used
    for (i = 0; i < questionCount; i++)
        if (!used[i]) unusedCount++;

    if (unusedCount == 0)                       /* seen them all: start over */
    {
        for (i = 0; i < questionCount; i++) used[i] = 0;
        unusedCount = questionCount;
    }

    /* choose the Nth unused question, N in [0, unusedCount-1] */
    target = (int)CP_Random_RangeFloat(0.0f, (float)unusedCount);
    //the range may include the upper bound
    if (target >= unusedCount) target = unusedCount - 1;

    //walk the bank, counting only unused entries
    for (i = 0; i < questionCount; i++)
    {
        if (used[i]) continue;
        //this is the Nth unused question
        if (target == 0)
        {   
            //mark it so it is not picked again this cycle
            used[i] = 1;
            return &questions[i];
        }
        target--;
    }
    //unreachable in practise
    return NULL;
}