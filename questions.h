//---------------------------------------------------------
// file:	questions.h
// author:  Krishen T Selvan
// email:	K.selvan@digipen.edu
// 
// brief:	Loads multiple-choice questions from a JSON file
//---------------------------------------------------------
#ifndef QUESTIONS_H
#define QUESTIONS_H

#define QUESTION_MAX       256
#define QUESTION_TEXT_LEN  256
#define OPTION_TEXT_LEN    96

typedef struct
{
    char text[QUESTION_TEXT_LEN];
    char options[4][OPTION_TEXT_LEN];
    int  answer;                      /* 0-3: index into options */
} Question;

/* Reads the file once (later calls do nothing). Pass just the file name.
   Returns -1 = file not found, 0 = found but no valid questions, else the count. */
int Questions_Load(const char* path);

/* Random question, never repeating until all have been used. NULL if none loaded. */
const Question* Questions_Pick(void);

/* text describing which files were tried, for on-screen debugging */
const char* Questions_DebugInfo(void);

#endif
