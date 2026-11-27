//---------------------------------------------------------
// file:	questions.h
// author:  Krishen T Selvan
// email:	K.selvan@digipen.edu
// 
// brief:	Loads multiple-choice questions from a JSON file
// 
// SYSTEM OVERVIEW
// 
// The question system is a self contained module that owns every multiple-choice question
// used by the game. It is responsible for exactly three things:
// 
// 1. loading questions from a JSON data file (Questions.json)
// 2. Handing out random questions on request without repeating one until every question has been seen.
// 3. Reporting what happened during loading
// 
// CONTENT CREATION
// 
// Questions are data, not code. A designed adds, removes or edits questions by editing the json file only.
// There is no need to recompile, the file is read once at start-up.
// 
//---------------------------------------------------------
#ifndef QUESTIONS_H
#define QUESTIONS_H

//Maximum number of questions kept in the memory. 
//Questions in the JSON file beyond this memory amount is ignored.
#define QUESTION_MAX       256

//Size in bytes of the question text buffer, including the '/0' terminator.
//Longer questions are cut off to fit.
#define QUESTION_TEXT_LEN  256

//Size in bytes of each answer-option buffer, including the '/0' terminator
//Longer options are cut off to fit.
#define OPTION_TEXT_LEN    96


//One multiple choice question always has exactly four options.
/*
Mirrors one object in questions.json:
{"question":"...","options":["a","b","c","d"],"answer":0}
*/
typedef struct
{   
    //Question shown to the player
    char text[QUESTION_TEXT_LEN];

    //Answewr choices shown in display order
    char options[4][OPTION_TEXT_LEN];

    //index 0-3 into options of the current choice
    int  answer;                      
} Question;


/* Reads the file once (later calls do nothing). Pass just the file name.
   Returns -1 = file not found, 0 = found but no valid questions, else the count. */
int Questions_Load(const char* path);

/* Random question, never repeating until all have been used. NULL if none loaded. */
const Question* Questions_Pick(void);

/* text describing which files were tried, for on-screen debugging */
const char* Questions_DebugInfo(void);

#endif
