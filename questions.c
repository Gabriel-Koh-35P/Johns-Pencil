//---------------------------------------------------------
// file:	questions.c
// author:  Krishen T Selvan
// email:	K.selvan@digipen.edu
//
// brief:	Minimal JSON reader for questions.json.
//          Accepts either {"questions":[ ... ]} or a bare [ ... ].
//          Each entry: {"question":"...","options":["a","b","c","d"],"answer":0}
//---------------------------------------------------------
#include "cprocessing.h"
#include "questions.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Question questions[QUESTION_MAX];
static int      used[QUESTION_MAX];     /* used[i] = 1 once question i has been picked */
static int      questionCount;
static int      loaded;
static int      loadResult;             /* -1 = not found, 0 = no valid questions, >0 = count */
static char     debugInfo[512];

/* ------------------------------------------------------------------ */
/* Debug text                                                          */
/* ------------------------------------------------------------------ */

static void DebugLog(const char* fmt, ...)
{
    size_t  len = strlen(debugInfo);
    va_list args;

    if (len + 1 >= sizeof debugInfo)
        return;

    va_start(args, fmt);
    vsnprintf(debugInfo + len, sizeof debugInfo - len, fmt, args);
    va_end(args);
}

const char* Questions_DebugInfo(void)
{
    return debugInfo;
}

/* ------------------------------------------------------------------ */
/* Tiny JSON helpers. Every function takes a pointer into the text and */
/* returns the pointer just past what it read (NULL if malformed).     */
/* ------------------------------------------------------------------ */

static const char* SkipWs(const char* p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

/* Skips whitespace, then requires the next character to be 'c'. */
static const char* Expect(const char* p, char c)
{
    p = SkipWs(p);
    return (*p == c) ? p + 1 : NULL;
}

/* Reads a "string" into out (if out is not NULL), handling escapes. */
static const char* ParseString(const char* p, char* out, int cap)
{
    int n = 0;

    p = Expect(p, '"');
    if (!p) return NULL;

    while (*p && *p != '"')
    {
        char c = *p++;

        if (c == '\\')
        {
            c = *p;
            if (c == '\0') return NULL;
            p++;

            if (c == 'n')      c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'u')                 /* \uXXXX not supported: use '?' */
            {
                c = '?';
                for (int i = 0; i < 4 && *p; i++) p++;
            }
            /* anything else (\" \\ \/) is just the character itself */
        }

        if (out && n < cap - 1)
            out[n++] = c;
    }

    if (*p != '"') return NULL;
    if (out) out[n] = '\0';
    return p + 1;
}

/* Skips any JSON value (used for keys we don't care about). */
static const char* SkipValue(const char* p)
{
    p = SkipWs(p);

    if (*p == '"')
        return ParseString(p, NULL, 0);

    if (*p == '{' || *p == '[')
    {
        char close = (*p == '{') ? '}' : ']';
        int  isObject = (*p == '{');

        p = SkipWs(p + 1);
        if (*p == close) return p + 1;

        for (;;)
        {
            if (isObject)                       /* objects have "key": before each value */
            {
                p = ParseString(p, NULL, 0);
                p = p ? Expect(p, ':') : NULL;
                if (!p) return NULL;
            }
            p = SkipValue(p);
            if (!p) return NULL;

            p = SkipWs(p);
            if (*p == ',') { p++; continue; }
            if (*p == close) return p + 1;
            return NULL;
        }
    }

    /* number, true, false, null */
    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p))
        p++;
    return p;
}

/* ------------------------------------------------------------------ */
/* Question parsing                                                    */
/* ------------------------------------------------------------------ */

/* Reads ["a","b","c","d"] into q->options. Extra entries are ignored. */
static const char* ParseOptions(const char* p, Question* q, int* count)
{
    *count = 0;

    p = Expect(p, '[');
    if (!p) return NULL;

    p = SkipWs(p);
    if (*p == ']') return p + 1;

    for (;;)
    {
        if (*count < 4)
            p = ParseString(p, q->options[(*count)++], OPTION_TEXT_LEN);
        else
            p = SkipValue(p);
        if (!p) return NULL;

        p = SkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') return p + 1;
        return NULL;
    }
}

/* Reads one {...} object into q. *valid is set to 1 only if it has a
   question, exactly 4 options, and an answer between 0 and 3. */
static const char* ParseQuestion(const char* p, Question* q, int* valid)
{
    int haveText = 0, haveAnswer = 0, optionCount = 0;

    memset(q, 0, sizeof * q);
    *valid = 0;

    p = Expect(p, '{');
    if (!p) return NULL;

    p = SkipWs(p);
    if (*p == '}') return p + 1;                /* empty object */

    for (;;)
    {
        char key[32];

        p = ParseString(p, key, sizeof key);
        p = p ? Expect(p, ':') : NULL;
        if (!p) return NULL;

        if (strcmp(key, "question") == 0)
        {
            p = ParseString(SkipWs(p), q->text, QUESTION_TEXT_LEN);
            haveText = 1;
        }
        else if (strcmp(key, "options") == 0)
        {
            p = ParseOptions(p, q, &optionCount);
        }
        else if (strcmp(key, "answer") == 0)
        {
            q->answer = atoi(SkipWs(p));
            haveAnswer = 1;
            p = SkipValue(p);
        }
        else
        {
            p = SkipValue(p);                   /* unknown key: ignore it */
        }
        if (!p) return NULL;

        p = SkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        return NULL;
    }

    *valid = haveText && haveAnswer && optionCount == 4 &&
        q->answer >= 0 && q->answer < 4;
    return p;
}

/* Reads [ {...}, {...}, ... ] and stores every valid question. */
static void ParseQuestionArray(const char* p)
{
    p = Expect(p, '[');
    if (!p) return;

    p = SkipWs(p);
    if (*p == ']') return;

    for (;;)
    {
        Question q;
        int      valid;

        p = ParseQuestion(p, &q, &valid);
        if (!p) return;

        if (valid && questionCount < QUESTION_MAX)
            questions[questionCount++] = q;

        p = SkipWs(p);
        if (*p != ',') return;                  /* ']' or garbage: we're done */
        p++;
    }
}

/* ------------------------------------------------------------------ */
/* File reading                                                        */
/* ------------------------------------------------------------------ */

/* Reads a whole file into a new null-terminated buffer (caller frees).
   Returns NULL if the file can't be opened. *size is 0 for an empty file. */
static char* ReadWholeFile(const char* path, long* size)
{
    FILE* f = NULL;
    char* buf;

#ifdef _MSC_VER
    if (fopen_s(&f, path, "rb") != 0) f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (*size <= 0)
    {
        *size = 0;
        fclose(f);
        return (char*)calloc(1, 1);             /* empty but "found" */
    }

    buf = (char*)malloc((size_t)*size + 1);
    if (buf)
    {
        *size = (long)fread(buf, 1, (size_t)*size, f);
        buf[*size] = '\0';
    }
    fclose(f);
    return buf;
}

/* If buf is UTF-16 (has an FF FE / FE FF marker), converts it to UTF-8.
   Always returns a buffer the caller must free; buf itself is freed if replaced. */
static char* ToUtf8(char* buf, long size)
{
    unsigned char b0 = (unsigned char)buf[0];
    unsigned char b1 = (unsigned char)buf[1];
    int  bigEndian;
    long units, i, n = 0;
    char* utf8;

    if (size < 2 || !((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF)))
        return buf;                             /* already UTF-8 / ASCII */

    bigEndian = (b0 == 0xFE);
    units = (size - 2) / 2;
    utf8 = (char*)malloc((size_t)units * 3 + 1);
    if (!utf8) return buf;

    for (i = 0; i < units; i++)
    {
        unsigned char first = (unsigned char)buf[2 + i * 2];
        unsigned char second = (unsigned char)buf[3 + i * 2];
        unsigned int  c = bigEndian ? ((first << 8) | second) : ((second << 8) | first);

        if (c < 0x80)
        {
            utf8[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            utf8[n++] = (char)(0xC0 | (c >> 6));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            utf8[n++] = (char)(0xE0 | (c >> 12));
            utf8[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            utf8[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    utf8[n] = '\0';
    free(buf);
    return utf8;
}

/* Turns file text into questions. Takes ownership of buf and frees it. */
static void ParseText(char* buf, long size)
{
    const char* p;

    buf = ToUtf8(buf, size);
    p = buf;

    /* skip UTF-8 BOM if present */
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;
    p = SkipWs(p);

    if (*p == '{')                              /* {"questions":[ ... ]} -> jump to the array */
    {
        p = strstr(p, "\"questions\"");
        p = p ? strchr(p, '[') : NULL;
    }
    if (p && *p == '[')
        ParseQuestionArray(p);

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
    static const char* folders[] = { "", "Assets/", "../Assets/", "../", "../../Assets/", "Bouncy Bullet/Assets/" };
    int folderCount = (int)(sizeof folders / sizeof folders[0]);
    int fileFound = 0;
    int i;

    if (loaded)
        return loadResult;
    loaded = 1;
    loadResult = -1;
    debugInfo[0] = '\0';

    for (i = 0; i < folderCount && questionCount == 0; i++)
    {
        char  path[260];
        long  size = 0;
        char* buf;

        snprintf(path, sizeof path, "%s%s", folders[i], name);

        buf = ReadWholeFile(path, &size);
        if (!buf)
            continue;                           /* not in this folder */

        fileFound = 1;

        if (size == 0)
        {
            DebugLog("[%s: EMPTY] ", path);
            free(buf);
            continue;
        }

        /* grab the first bytes for the debug text before parsing frees buf */
        {
            int b0 = (unsigned char)buf[0];
            int b1 = (size > 1) ? (unsigned char)buf[1] : 0;
            int b2 = (size > 2) ? (unsigned char)buf[2] : 0;

            ParseText(buf, size);
            DebugLog("[%s: %ld bytes, starts %02X %02X %02X -> %d questions] ",
                path, size, b0, b1, b2, questionCount);
        }
    }

    if (questionCount > 0) loadResult = questionCount;
    else if (fileFound)    loadResult = 0;

    return loadResult;
}

/* Random question that hasn't been used yet. Once all have been used, the
   list resets and starts over. */
const Question* Questions_Pick(void)
{
    int unusedCount = 0;
    int target, i;

    if (questionCount == 0)
        return NULL;

    for (i = 0; i < questionCount; i++)
        if (!used[i]) unusedCount++;

    if (unusedCount == 0)                       /* seen them all: start over */
    {
        for (i = 0; i < questionCount; i++) used[i] = 0;
        unusedCount = questionCount;
    }

    /* choose the Nth unused question, N in [0, unusedCount-1] */
    target = (int)CP_Random_RangeFloat(0.0f, (float)unusedCount);
    if (target >= unusedCount) target = unusedCount - 1;

    for (i = 0; i < questionCount; i++)
    {
        if (used[i]) continue;
        if (target == 0)
        {
            used[i] = 1;
            return &questions[i];
        }
        target--;
    }
    return NULL;
}