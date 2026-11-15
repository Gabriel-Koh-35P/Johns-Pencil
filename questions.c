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
static int      used[QUESTION_MAX];
static int      questionCount;
static int      loaded;
static int      loadResult;   /* -1 = file not found, 0 = found but no valid questions, >0 = count */

static const char* SkipWs(const char* p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

/* p is at an opening quote. Copies the (unescaped) text into out if out != NULL.
   Returns the position after the closing quote, or NULL on malformed input. */
static const char* ParseString(const char* p, char* out, int cap)
{
    int n = 0;

    if (*p != '"')
        return NULL;
    p++;

    while (*p && *p != '"')
    {
        char c = *p++;

        if (c == '\\')
        {
            char e = *p++;
            switch (e)
            {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'u': c = '?'; if (*p) p += 4; break;   /* \uXXXX not supported: placeholder */
            case 0:   return NULL;
            default:  c = e; break;                      /* \" \\ \/ */
            }
        }
        if (out && n < cap - 1)
            out[n++] = c;
    }
    if (*p != '"')
        return NULL;
    if (out)
        out[n] = '\0';
    return p + 1;
}

/* skips any JSON value (used for keys we don't care about) */
static const char* SkipValue(const char* p)
{
    p = SkipWs(p);

    if (*p == '"')
        return ParseString(p, NULL, 0);

    if (*p == '{' || *p == '[')
    {
        char open = *p;
        char close = (open == '{') ? '}' : ']';

        p = SkipWs(p + 1);
        if (*p == close)
            return p + 1;

        for (;;)
        {
            if (open == '{')
            {
                p = ParseString(SkipWs(p), NULL, 0);
                if (!p) return NULL;
                p = SkipWs(p);
                if (*p != ':') return NULL;
                p++;
            }
            p = SkipValue(p);
            if (!p) return NULL;
            p = SkipWs(p);
            if (*p == ',') { p++; continue; }
            if (*p == close) return p + 1;
            return NULL;
        }
    }

    while (*p && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p))
        p++;
    return p;
}

static const char* ParseOptions(const char* p, Question* q, int* count)
{
    p = SkipWs(p);
    if (*p != '[') return NULL;
    p = SkipWs(p + 1);
    *count = 0;

    if (*p == ']') return p + 1;

    for (;;)
    {
        if (*count < 4)
            p = ParseString(SkipWs(p), q->options[(*count)++], OPTION_TEXT_LEN);
        else
            p = SkipValue(p);
        if (!p) return NULL;
        p = SkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') return p + 1;
        return NULL;
    }
}

/* p is at '{'. Fills q; *valid says whether it has everything a question needs. */
static const char* ParseQuestion(const char* p, Question* q, int* valid)
{
    int haveText = 0, optCount = 0, haveAnswer = 0;

    memset(q, 0, sizeof * q);
    *valid = 0;

    p = SkipWs(p);
    if (*p != '{') return NULL;
    p = SkipWs(p + 1);
    if (*p == '}') return p + 1;

    for (;;)
    {
        char key[32];

        p = ParseString(SkipWs(p), key, sizeof key);
        if (!p) return NULL;
        p = SkipWs(p);
        if (*p != ':') return NULL;
        p = SkipWs(p + 1);

        if (strcmp(key, "question") == 0)
        {
            p = ParseString(p, q->text, QUESTION_TEXT_LEN);
            haveText = 1;
        }
        else if (strcmp(key, "options") == 0)
            p = ParseOptions(p, q, &optCount);
        else if (strcmp(key, "answer") == 0)
        {
            q->answer = atoi(p);
            haveAnswer = 1;
            p = SkipValue(p);
        }
        else
            p = SkipValue(p);

        if (!p) return NULL;
        p = SkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        return NULL;
    }

    *valid = haveText && optCount == 4 && haveAnswer && q->answer >= 0 && q->answer < 4;
    return p;
}

static const char* ParseQuestionArray(const char* p)
{
    p = SkipWs(p);
    if (*p != '[') return NULL;
    p = SkipWs(p + 1);
    if (*p == ']') return p + 1;

    for (;;)
    {
        Question q;
        int      valid;

        p = ParseQuestion(p, &q, &valid);
        if (!p) return NULL;
        if (valid && questionCount < QUESTION_MAX)
            questions[questionCount++] = q;

        p = SkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') return p + 1;
        return NULL;
    }
}

static char debugInfo[512];

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

/* takes file contents already in memory (UTF-8, UTF-8 with BOM, or UTF-16),
   fills the question list, and frees buf */
static void ParseText(char* buf, long size)
{
    const char* p;

    if (size >= 2 &&
        (((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) ||
            ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF)))
    {
        int    bigEndian = ((unsigned char)buf[0] == 0xFE);
        long   units = (size - 2) / 2;
        char* utf8 = (char*)malloc((size_t)units * 3 + 1);
        long   i, n = 0;

        if (!utf8) { free(buf); return; }

        for (i = 0; i < units; i++)
        {
            unsigned char lo = (unsigned char)buf[2 + i * 2 + (bigEndian ? 1 : 0)];
            unsigned char hi = (unsigned char)buf[2 + i * 2 + (bigEndian ? 0 : 1)];
            unsigned int  c = (unsigned int)lo | ((unsigned int)hi << 8);

            if (c < 0x80)        utf8[n++] = (char)c;
            else if (c < 0x800) { utf8[n++] = (char)(0xC0 | (c >> 6)); utf8[n++] = (char)(0x80 | (c & 0x3F)); }
            else { utf8[n++] = (char)(0xE0 | (c >> 12)); utf8[n++] = (char)(0x80 | ((c >> 6) & 0x3F)); utf8[n++] = (char)(0x80 | (c & 0x3F)); }
        }
        utf8[n] = '\0';
        free(buf);
        buf = utf8;
    }

    p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;                                 /* UTF-8 BOM */
    p = SkipWs(p);

    if (*p == '[')
        ParseQuestionArray(p);
    else if (*p == '{')
    {
        p = SkipWs(p + 1);
        while (p && *p == '"')
        {
            char key[32];

            p = ParseString(p, key, sizeof key);
            if (!p) break;
            p = SkipWs(p);
            if (*p != ':') break;
            p++;
            p = (strcmp(key, "questions") == 0) ? ParseQuestionArray(p) : SkipValue(p);
            if (!p) break;
            p = SkipWs(p);
            if (*p == ',') p = SkipWs(p + 1);
            else break;
        }
    }

    free(buf);
}

/* Describes what the loader tried, for showing on screen while debugging */
const char* Questions_DebugInfo(void)
{
    return debugInfo;
}

/* pass just the file name, e.g. "questions.json". Tries it in several folders and
   skips copies that are empty or hold no valid questions, so an empty stray copy
   can't hide the real one. Returns -1 if no copy was found, 0 if copies were found
   but none held valid questions, else the number of questions loaded. */
int Questions_Load(const char* name)
{
    static const char* prefixes[] = { "", "Assets/", "../Assets/", "../", "../../Assets/", "Bouncy Bullet/Assets/" };
    int found = 0;
    int k;

    if (loaded)
        return loadResult;
    loaded = 1;
    loadResult = -1;
    debugInfo[0] = '\0';

    for (k = 0; k < (int)(sizeof prefixes / sizeof prefixes[0]) && questionCount == 0; k++)
    {
        char  full[260];
        FILE* f = NULL;
        long  size;
        char* buf;
        int   b0 = 0, b1 = 0, b2 = 0;

        snprintf(full, sizeof full, "%s%s", prefixes[k], name);
#ifdef _MSC_VER
        if (fopen_s(&f, full, "rb") != 0)
            f = NULL;
#else
        f = fopen(full, "rb");
#endif
        if (!f)
            continue;

        found = 1;
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0)
        {
            DebugLog("[%s: EMPTY] ", full);
            fclose(f);
            continue;
        }

        buf = (char*)malloc((size_t)size + 1);
        if (!buf) { fclose(f); continue; }
        size = (long)fread(buf, 1, (size_t)size, f);
        buf[size] = '\0';
        fclose(f);

        if (size > 0) b0 = (unsigned char)buf[0];
        if (size > 1) b1 = (unsigned char)buf[1];
        if (size > 2) b2 = (unsigned char)buf[2];

        ParseText(buf, size);
        DebugLog("[%s: %ld bytes, starts %02X %02X %02X -> %d questions] ",
            full, size, b0, b1, b2, questionCount);
    }

    if (questionCount > 0)
        loadResult = questionCount;
    else if (found)
        loadResult = 0;

    return loadResult;
}

const Question* Questions_Pick(void)
{
    int i, free_count = 0, pick;

    if (questionCount == 0)
        return NULL;

    for (i = 0; i < questionCount; i++)
        if (!used[i]) free_count++;

    if (free_count == 0)                       /* seen them all: start over */
    {
        for (i = 0; i < questionCount; i++) used[i] = 0;
        free_count = questionCount;
    }

    pick = (int)CP_Random_RangeFloat(0.0f, (float)free_count);
    if (pick >= free_count) pick = free_count - 1;

    for (i = 0; i < questionCount; i++)
    {
        if (used[i]) continue;
        if (pick-- == 0)
        {
            used[i] = 1;
            return &questions[i];
        }
    }
    return NULL;
}