//---------------------------------------------------------
// file:	game.c
// author:	Gabriel Koh Quan Kai
// email:	gabrielquankai.koh@digipen.edu
//
// brief:	Logic for running the game
//
// Copyright � 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#include "cprocessing.h"
#include "game.h"
#include "levelcomplete.h"
#include "levelfailed.h"
#include "levelselect.h"
#include "progress.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>

#define PLAYER_DIAMETER  150.0f
#define BULLET_DIAMETER  16.0f
#define BULLET_DRAW_SIZE 100.0f   /* visual, can be bigger to show padding without affecting hits */
#define TARGET_DIAMETER  150.0f
#define TARGET_DRAW_SIZE 200.0f   /* visual, can be bigger to show padding without affecting hits */
#define BULLET_SPEED     600.0f
#define BUTTON_DIAMETER  40.0f

#define MAX_AMMO          3
#define AMMO_CAP          5
#define AMMO_ICON_W     160.0f
#define AMMO_ICON_H     80.0f
#define AMMO_ICON_STEP  30.0f


#define MAX_ENEMIES  3
#define MAX_WALLS    3
#define MAX_BOXES    3
#define MAX_DOORS    2
#define MAX_POWERUPS 3

#define POWERUP_BOX_DIAMETER 100.0f
#define POWERUP_BOX_DRAW_SIZE 300.0f

#define OPTION_COUNT 4
#define OPTION_BTN_W 140.0f
#define OPTION_BTN_H 80.0f
#define OPTION_GAP   30.0f

static CP_Image playerImage;
static CP_Image enemyImage;
static CP_Image powerupBoxImage;
static CP_Image bulletImage;
static CP_Image bulletPierceImage;

static int   awaitingQuestion;   /* 1 while the MCQ popup is showing, pauses the game */
static int   correctOption;      /* index 0-3 of the correct answer this question */
static int   pendingPierce;      /* 1: the next fired bullet starts piercing */

static float optionX[OPTION_COUNT];
static float optionY;

typedef struct
{
    CP_Vector pos;
    CP_Vector vel;
    int       active;
    int       piercing;   /* 1 for the rest of this flight once picked up */
} Bullet;

typedef struct
{
    CP_Vector pos;
    int       hit;
} Target;

typedef struct
{
    CP_Vector pos;
    float     w, h;
} Rect;   /* shared shape for walls and boxes */

typedef struct
{
    CP_Vector pos;
    float     w, h;
    int       open;
} Door;

typedef struct
{
    CP_Vector pos;
    int       pressed;
} Button;  /* buttons[i] always controls doors[i] */

typedef struct
{
    CP_Vector pos;
    int       active;
} Powerup;   /* always a powerup box now */

static Target  targets[MAX_ENEMIES];
static Rect    walls[MAX_WALLS];
static Rect    boxes[MAX_BOXES];
static Door    doors[MAX_DOORS];
static Button  buttons[MAX_DOORS];
static Powerup powerups[MAX_POWERUPS];

static int enemyCount, wallCount, boxCount, doorCount, powerupCount;

static CP_Vector playerPos;
static Bullet    bullet;
static int       aiming;
static int       ammo;

/* ---------------------------------------------------------- */
/* aiming */

static CP_Vector AimDirection(CP_Vector from, CP_Vector to)
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);

    if (len < 0.0001f)
        return CP_Vector_Set(0.0f, -1.0f);

    return CP_Vector_Set(dx / len, dy / len);
}

static void TriggerPowerupBox(void)
{
    unsigned int roll = CP_Random_RangeInt(1, 4);

    if (roll == 1)
    {
        ammo = (ammo + 2 >= AMMO_CAP) ? AMMO_CAP : ammo + 2;
    }
    else if (roll == 2)
    {
        pendingPierce = 1;
        ammo += 1;
    }
    else
    {
        /* rolls 3 and 4 together: 2 in 4, i.e. 1/2 chance */
        awaitingQuestion = 1;
        correctOption = 0;   /* "What is A?" -> option A, index 0 */
    }
}

/* ---------------------------------------------------------- */
/* collision: one shared rectangle-vs-bullet resolver, used for
   walls, boxes, and closed doors */

static void ResolveRectCollision(CP_Vector rectPos, float w, float h)
{
    float r = BULLET_DIAMETER / 2.0f;

    float left = rectPos.x - w / 2.0f - r;
    float right = rectPos.x + w / 2.0f + r;
    float top = rectPos.y - h / 2.0f - r;
    float bottom = rectPos.y + h / 2.0f + r;

    if (bullet.pos.x < left || bullet.pos.x > right ||
        bullet.pos.y < top || bullet.pos.y > bottom)
        return;

    float penLeft = bullet.pos.x - left;
    float penRight = right - bullet.pos.x;
    float penTop = bullet.pos.y - top;
    float penBottom = bottom - bullet.pos.y;

    float minPenX = (penLeft < penRight) ? penLeft : penRight;
    float minPenY = (penTop < penBottom) ? penTop : penBottom;

    if (minPenX < minPenY)
    {
        bullet.pos.x = (penLeft < penRight) ? left : right;
        bullet.vel.x = -bullet.vel.x;
    }
    else
    {
        bullet.pos.y = (penTop < penBottom) ? top : bottom;
        bullet.vel.y = -bullet.vel.y;
    }
}

static void UpdateBullet(float dt)
{
    float r = BULLET_DIAMETER / 2.0f;
    float w = (float)CP_System_GetWindowWidth();
    int   i;

    if (!bullet.active)
        return;

    bullet.pos.x += bullet.vel.x * dt;
    bullet.pos.y += bullet.vel.y * dt;

    /* window edges always bounce, piercing or not */
    if (bullet.pos.x - r < 0.0f) { bullet.pos.x = r;     bullet.vel.x = -bullet.vel.x; }
    else if (bullet.pos.x + r > w) { bullet.pos.x = w - r; bullet.vel.x = -bullet.vel.x; }
    if (bullet.pos.y - r < 0.0f) { bullet.pos.y = r;     bullet.vel.y = -bullet.vel.y; }

    /* walls, boxes, closed doors: skipped entirely while piercing */
    if (!bullet.piercing)
    {
        for (i = 0; i < wallCount; i++)
            ResolveRectCollision(walls[i].pos, walls[i].w, walls[i].h);

        for (i = 0; i < boxCount; i++)
            ResolveRectCollision(boxes[i].pos, boxes[i].w, boxes[i].h);

        for (i = 0; i < doorCount; i++)
            if (!doors[i].open)
                ResolveRectCollision(doors[i].pos, doors[i].w, doors[i].h);
    }

    /* buttons: always checked, piercing or not */
    for (i = 0; i < doorCount; i++)
    {
        if (!buttons[i].pressed &&
            AreCirclesIntersecting(bullet.pos.x, bullet.pos.y, r,
                buttons[i].pos.x, buttons[i].pos.y, BUTTON_DIAMETER / 2.0f))
        {
            buttons[i].pressed = 1;
            doors[i].open = 1;
        }
    }

        /* powerup boxes: always checked */
    for (i = 0; i < powerupCount; i++)
    {
        if (!powerups[i].active)
            continue;

        if (AreCirclesIntersecting(bullet.pos.x, bullet.pos.y, r,
            powerups[i].pos.x, powerups[i].pos.y, POWERUP_BOX_DIAMETER / 2.0f))
        {
            powerups[i].active = 0;
            bullet.active = 0;   /* the box always consumes the bullet */
            TriggerPowerupBox();
        }
    }

    /* missed: fell off the bottom */
    if (bullet.pos.y - r > (float)CP_System_GetWindowHeight())
        bullet.active = 0;

    /* targets */
    for (i = 0; i < enemyCount; i++)
    {
        if (!targets[i].hit &&
            AreCirclesIntersecting(bullet.pos.x, bullet.pos.y, r,
                targets[i].pos.x, targets[i].pos.y, TARGET_DIAMETER / 2.0f))
        {
            targets[i].hit = 1;
            bullet.active = 0;
        }
    }
}

/* ---------------------------------------------------------- */
/* drawing */

static void DrawAimLine(void)
{
    CP_Vector mouse = CP_Vector_Set(CP_Input_GetMouseX(), CP_Input_GetMouseY());

    CP_Settings_Stroke(CP_Color_Create(255, 255, 255, 255));
    CP_Settings_StrokeWeight(3.0f);
    CP_Graphics_DrawLine(playerPos.x, playerPos.y, mouse.x, mouse.y);
}

static void DrawWalls(void)
{
    int i;
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_Fill(CP_Color_Create(120, 120, 120, 255));

    for (i = 0; i < wallCount; i++)
        CP_Graphics_DrawRect(walls[i].pos.x, walls[i].pos.y, walls[i].w, walls[i].h);
}

static void DrawBoxes(void)
{
    int i;
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));
    CP_Settings_Fill(CP_Color_Create(150, 100, 50, 255));

    for (i = 0; i < boxCount; i++)
        CP_Graphics_DrawRect(boxes[i].pos.x, boxes[i].pos.y, boxes[i].w, boxes[i].h);
}

static void DrawDoorsAndButtons(void)
{
    int i;
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));

    for (i = 0; i < doorCount; i++)
    {
        if (!doors[i].open)
        {
            CP_Settings_Fill(CP_Color_Create(80, 60, 30, 255));
            CP_Graphics_DrawRect(doors[i].pos.x, doors[i].pos.y, doors[i].w, doors[i].h);
        }
        /* an open door is simply not drawn: it no longer blocks anything */

        CP_Settings_Fill(buttons[i].pressed ? CP_Color_Create(80, 220, 80, 255)
            : CP_Color_Create(230, 220, 0, 255));
        CP_Graphics_DrawCircle(buttons[i].pos.x, buttons[i].pos.y, BUTTON_DIAMETER);
    }
}

static void DrawPowerups(void)
{
    int i;

    for (i = 0; i < powerupCount; i++)
    {
        if (!powerups[i].active)
            continue;

        CP_Image_Draw(powerupBoxImage, powerups[i].pos.x, powerups[i].pos.y,
            POWERUP_BOX_DRAW_SIZE, POWERUP_BOX_DRAW_SIZE, 255);
    }
}

static void DrawTargets(void)
{
    int i;

    for (i = 0; i < enemyCount; i++)
    {
        if (targets[i].hit)
            continue;

        CP_Image_Draw(enemyImage, targets[i].pos.x, targets[i].pos.y,
            TARGET_DRAW_SIZE, TARGET_DRAW_SIZE, 255);
    }
}

static void DrawAmmo(void)
{
    int   i;
    float x = playerPos.x + 120.0f;
    float y = (float)CP_System_GetWindowHeight() - 60.0f;

    for (i = 0; i < ammo; i++)
    {
        CP_Image img = (i == 0 && pendingPierce) ? bulletPierceImage : bulletImage;

        CP_Image_DrawAdvanced(img, x + i * AMMO_ICON_STEP, y,
            AMMO_ICON_W, AMMO_ICON_H, 255, -90.0f);
    }
}

static void DrawQuestionPopup(void)
{
    const char* labels[OPTION_COUNT] = { "A", "B", "C", "D" };
    float centerX = (float)CP_System_GetWindowWidth() * 0.5f;
    float titleY = (float)CP_System_GetWindowHeight() * 0.3f;
    int   i;

    /* dim overlay over the frozen game scene */
    CP_Settings_RectMode(CP_POSITION_CENTER);
    CP_Settings_Fill(CP_Color_Create(0, 0, 0, 150));
    CP_Graphics_DrawRect(centerX, (float)CP_System_GetWindowHeight() * 0.5f,
        (float)CP_System_GetWindowWidth(), (float)CP_System_GetWindowHeight());

    CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
    CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
    CP_Font_DrawText("What is A?", centerX, titleY);

    for (i = 0; i < OPTION_COUNT; i++)
    {
        CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));
        CP_Settings_Fill(CP_Color_Create(255, 255, 255, 255));
        CP_Graphics_DrawRect(optionX[i], optionY, OPTION_BTN_W, OPTION_BTN_H);

        CP_Settings_Fill(CP_Color_Create(0, 0, 0, 255));
        CP_Font_DrawText(labels[i], optionX[i], optionY);
    }

    if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
    {
        float mx = CP_Input_GetMouseX();
        float my = CP_Input_GetMouseY();

        for (i = 0; i < OPTION_COUNT; i++)
        {
            if (IsAreaClicked(optionX[i], optionY, OPTION_BTN_W, OPTION_BTN_H, mx, my))
            {
                if (i == correctOption)
                {
                    if (CP_Random_GetBool())
                        ammo = (ammo + 2 >= AMMO_CAP) ? AMMO_CAP : ammo + 2;
                    else
                        pendingPierce = 1;
                        ammo += 1;
                }
                /* wrong answer: nothing happens, matches "bullet already deleted" */

                awaitingQuestion = 0;
                return;
            }
        }
    }
}

static int AllTargetsHit(void)
{
    int i;
    for (i = 0; i < enemyCount; i++)
        if (!targets[i].hit)
            return 0;
    return 1;
}

static float VelocityToDegrees(CP_Vector vel) // For flipping the projectiles
{
    float radians = atan2f(vel.y, vel.x);
    return radians * (180.0f / 3.14159265f);
}

/* ---------------------------------------------------------- */
/* level layouts */
static void SetupLevel(int level)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    int   i;

    for (i = 0; i < MAX_ENEMIES; i++) targets[i].hit = 0;
    for (i = 0; i < MAX_DOORS; i++) { doors[i].open = 0; buttons[i].pressed = 0; }
    for (i = 0; i < MAX_POWERUPS; i++) powerups[i].active = 1;

    doorCount = 0;
    boxCount = 0;
    powerupCount = 0;

    switch (level)
    {
    case 1:
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 2:
        enemyCount = 1;
        wallCount = 3;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.25f);

        walls[0].pos = CP_Vector_Set(w * 0.75f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 220.0f;

        walls[1].pos = CP_Vector_Set(w * 0.45f, h * 0.65f);
        walls[1].w = 220.0f;
        walls[1].h = 30.0f;

        walls[2].pos = CP_Vector_Set(w * 0.65f, h * 0.75f);
        walls[2].w = 30.0f;
        walls[2].h = 220.0f;
        break;

    case 3:
        enemyCount = 2;
        wallCount = 4;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.2f);
        targets[1].pos = CP_Vector_Set(w * 0.85f, h * 0.6f);

        walls[0].pos = CP_Vector_Set(w * 0.4f, h * 0.3f);
        walls[0].w = 30.0f;
        walls[0].h = 200.0f;

        walls[1].pos = CP_Vector_Set(w * 0.6f, h * 0.55f);
        walls[1].w = 200.0f;
        walls[1].h = 30.0f;

        walls[2].pos = CP_Vector_Set(w * 0.4f, h * 0.75f);
        walls[2].w = 30.0f;
        walls[2].h = 200.0f;

        walls[3].pos = CP_Vector_Set(w * 0.75f, h * 0.25f);
        walls[3].w = 30.0f;
        walls[3].h = 200.0f;
        break;

    case 4:   /* introduces boxes and the ammo powerup */
        enemyCount = 2;
        wallCount = 2;
        boxCount = 2;
        powerupCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.8f, h * 0.2f);
        targets[1].pos = CP_Vector_Set(w * 0.8f, h * 0.7f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.45f);
        walls[0].w = 30.0f;
        walls[0].h = 250.0f;

        walls[1].pos = CP_Vector_Set(w * 0.75f, h * 0.35f);
        walls[1].w = 30.0f;
        walls[1].h = 220.0f;

        boxes[0].pos = CP_Vector_Set(w * 0.3f, h * 0.6f);
        boxes[0].w = 80.0f;
        boxes[0].h = 80.0f;

        boxes[1].pos = CP_Vector_Set(w * 0.6f, h * 0.75f);
        boxes[1].w = 80.0f;
        boxes[1].h = 80.0f;

        powerups[0].pos = CP_Vector_Set(w * 0.3f, h * 0.3f);
        powerups[0].pos = CP_Vector_Set(w * 0.3f, h * 0.3f);
        break;

    case 5:   /* introduces the door/button and the piercing powerup */
        enemyCount = 2;
        wallCount = 1;
        doorCount = 1;
        powerupCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);
        targets[1].pos = CP_Vector_Set(w * 0.5f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.4f, h * 0.6f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;

        doors[0].pos = CP_Vector_Set(w * 0.65f, h * 0.3f);
        doors[0].w = 30.0f;
        doors[0].h = 250.0f;

        buttons[0].pos = CP_Vector_Set(w * 0.3f, h * 0.75f);

        powerups[0].pos = CP_Vector_Set(w * 0.5f, h * 0.8f);
        powerups[0].pos = CP_Vector_Set(w * 0.3f, h * 0.3f);
        break;

    case 6:
    default:  /* combines everything */ 
        enemyCount = 3;
        wallCount = 3;
        boxCount = 1;
        doorCount = 1;
        powerupCount = 2;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.2f);
        targets[1].pos = CP_Vector_Set(w * 0.85f, h * 0.65f);
        targets[2].pos = CP_Vector_Set(w * 0.60f, h * 0.7f);

        walls[0].pos = CP_Vector_Set(w * 0.7f, h * 0.3f);
        walls[0].w = 30.0f;
        walls[0].h = 250.0f;

        walls[1].pos = CP_Vector_Set(w * 0.7f, h * 0.65f);
        walls[1].w = 30.0f;
        walls[1].h = 250.0f;

        walls[2].pos = CP_Vector_Set(w * 0.5f, h * 0.3f);
        walls[2].w = 30.0f;
        walls[2].h = 300.0f;

        boxes[0].pos = CP_Vector_Set(w * 0.3f, h * 0.7f);
        boxes[0].w = 80.0f;
        boxes[0].h = 80.0f;

        doors[0].pos = CP_Vector_Set(w * 0.5f, h * 0.65f);
        doors[0].w = 30.0f;
        doors[0].h = 300.0f;

        buttons[0].pos = CP_Vector_Set(w * 0.35f, h * 0.2f);

        powerups[0].pos = CP_Vector_Set(w * 0.3f, h * 0.5f);
        powerups[0].pos = CP_Vector_Set(w * 0.3f, h * 0.3f);

        powerups[1].pos = CP_Vector_Set(w * 0.55f, h * 0.75f);
        powerups[1].pos = CP_Vector_Set(w * 0.3f, h * 0.3f);
        break;
    }
}

/* ---------------------------------------------------------- */
/* state functions */

void Game_Init(void)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    int   i;

    playerImage = CP_Image_Load("Assets/player.png");
    enemyImage = CP_Image_Load("Assets/stage1_enemy.png");
    powerupBoxImage = CP_Image_Load("Assets/powerup_box.png");
    bulletImage = CP_Image_Load("Assets/pencil_projectile.png");
    bulletPierceImage = CP_Image_Load("Assets/pen_pierce_projectile.png");

    Progress_LoadBackground();

    playerPos = CP_Vector_Set(w * 0.1f, h * 0.85f);

    SetupLevel(Progress_GetCurrentLevel());
    
    bullet.active = 0;
    bullet.piercing = 0;
    aiming = 0;
    ammo = MAX_AMMO;
    awaitingQuestion = 0;
    pendingPierce = 0;

    {
        float totalWidth = OPTION_COUNT * OPTION_BTN_W + (OPTION_COUNT - 1) * OPTION_GAP;
        float startX = (w - totalWidth) / 2.0f + OPTION_BTN_W / 2.0f;

        for (i = 0; i < OPTION_COUNT; i++)
            optionX[i] = startX + i * (OPTION_BTN_W + OPTION_GAP);
        optionY = h * 0.55f;
    }
}

void Game_Update(void)
{
    float dt = CP_System_GetDt();

    if (CP_Input_KeyTriggered(KEY_Q))
    {
        CP_Engine_SetNextGameState(Level_Select_Init, Level_Select_Update, Level_Select_Exit);
        return;
    }

    if (!awaitingQuestion)
    {
        if (!bullet.active)
        {
            if (CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
            {
                aiming = 1;
            }
            else if (aiming && !CP_Input_MouseDown(MOUSE_BUTTON_LEFT))
            {
                CP_Vector mouse = CP_Vector_Set(CP_Input_GetMouseX(), CP_Input_GetMouseY());
                CP_Vector dir = AimDirection(playerPos, mouse);

                bullet.pos = playerPos;
                bullet.vel = CP_Vector_Set(dir.x * BULLET_SPEED, dir.y * BULLET_SPEED);
                bullet.active = 1;
                bullet.piercing = pendingPierce;
                pendingPierce = 0;
                aiming = 0;
                ammo--;
            }
        }

        UpdateBullet(dt);

        if (AllTargetsHit())
        {
            Progress_UnlockLevel(Progress_GetCurrentLevel() + 1);
            CP_Engine_SetNextGameState(Level_Complete_Init, Level_Complete_Update, Level_Complete_Exit);
            return;
        }

        if (ammo <= 0 && !bullet.active && !aiming && !awaitingQuestion)
        {
            CP_Engine_SetNextGameState(Level_Failed_Init, Level_Failed_Update, Level_Failed_Exit);
            return;
        }
    }
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();

    CP_Image_Draw(Progress_GetBackground(), w / 2.0f, h / 2.0f, w, h, 255);

    if (aiming)
        DrawAimLine();

    DrawWalls();
    DrawBoxes();
    DrawDoorsAndButtons();
    DrawPowerups();
    DrawTargets();

    CP_Image_Draw(playerImage, playerPos.x, playerPos.y, PLAYER_DIAMETER, PLAYER_DIAMETER, 255);

    if (bullet.active)
    {
        float degrees = VelocityToDegrees(bullet.vel);   /* + 90.0f if your pencil points up by default */

        CP_Image_DrawAdvanced(bullet.piercing ? bulletPierceImage : bulletImage, bullet.pos.x, bullet.pos.y,
            BULLET_DRAW_SIZE, BULLET_DRAW_SIZE, 255, degrees);

        if (bullet.piercing)
        {
            CP_Settings_Fill(CP_Color_Create(255, 0, 0, 255));
            CP_Graphics_DrawCircle(bullet.pos.x, bullet.pos.y, 6.0f);
        }
    }

    DrawAmmo();

    if (awaitingQuestion)
        DrawQuestionPopup();
}

void Game_Exit(void)
{
}