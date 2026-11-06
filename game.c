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
#include <time.h>

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

/* boss (level 6) */
#define BOSS_MAX_LIVES        3
#define BOSS_DIAMETER         200.0f   /* collision */
#define BOSS_DRAW_SIZE        270.0f   /* visual, bigger than the hitbox like TARGET_DRAW_SIZE */
#define BOSS_LEVEL_AMMO       5        /* the boss takes 3 hits, so 5 bullets leaves 2 spare for misses, no powerups needed */

/* where the boss is allowed to land when he teleports */
#define BOSS_MARGIN_SIDE      40.0f    /* gap between his edge and the left/right of the window */
#define BOSS_MARGIN_TOP       90.0f    /* leaves room for the life pips above him */
#define BOSS_MARGIN_BOTTOM    160.0f   /* keeps him clear of the ammo icons */
#define BOSS_CLEARANCE        30.0f    /* breathing room kept around him when he lands */
#define BOSS_PLAYER_CLEARANCE 120.0f   /* extra distance kept away from the player */
#define BOSS_MIN_JUMP         350.0f   /* a teleport must move him at least this far */
#define BOSS_START_MIN_X      0.6f     /* his FIRST spot is on the right side: at least this fraction of the window width across */
#define BOSS_TELEPORT_TRIES   500       /* random spots tried per pass */

#define BOSS_PIP_SIZE         26.0f
#define BOSS_PIP_STEP         40.0f

/* area covered by the 1x/2x speed button (same numbers Game_Update uses for it) */
#define UI_SPEED_X 140.0f
#define UI_SPEED_Y 60.0f
#define UI_SPEED_W 440.0f
#define UI_SPEED_H 160.0f

#define OPTION_COUNT 4
#define OPTION_BTN_W 140.0f
#define OPTION_BTN_H 80.0f
#define OPTION_GAP   30.0f

static CP_Image playerImage;
static CP_Image enemyImage;
static CP_Image bossImage;
static CP_Image powerupBoxImage;
static CP_Image bulletImage;
static CP_Image bulletPierceImage;
static CP_Image speedBtn1xImage;
static CP_Image speedBtn2xImage;

static int   awaitingQuestion;   /* 1 while the MCQ popup is showing, pauses the game */
static int   correctOption;      /* index 0-3 of the correct answer this question */
static int   pendingPierce;      /* 1: the next fired bullet starts piercing */

static float optionX[OPTION_COUNT];
static float optionY;

static CP_Sound shootSound;
static CP_Sound enemyDeathSound;
static CP_Sound enemyDamageSound;
static CP_Sound powerupSound;

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

typedef struct
{
    CP_Vector pos;
    int       lives;    /* BOSS_MAX_LIVES counting down to 0; at 0 he is defeated */
    int       active;   /* 1 only on the boss level */
} Boss;

static Target  targets[MAX_ENEMIES];
static Rect    walls[MAX_WALLS];
static Rect    boxes[MAX_BOXES];
static Door    doors[MAX_DOORS];
static Button  buttons[MAX_DOORS];
static Powerup powerups[MAX_POWERUPS];
static Boss    boss;

static int enemyCount, wallCount, boxCount, doorCount, powerupCount;

static CP_Vector playerPos;
static Bullet    bullet;
static int       aiming;
static int       ammo;
static int       levelStartAmmo;   /* bullets this level starts with */
static int       seeded;           /* random generator seeded yet? */
static int speedBoosted;   /* 1 = current bullet is at 2x speed */

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

static void AddAmmo(int n)
{
    /* the cap is never lower than what the level started with, and picking up
       a reload never *reduces* the stock (level 6 starts above AMMO_CAP) */
    int cap = (levelStartAmmo > AMMO_CAP) ? levelStartAmmo : AMMO_CAP;

    if (ammo + n > cap)
    {
        if (ammo < cap)
            ammo = cap;
    }
    else
        ammo += n;
}

static void TriggerPowerupBox(void)
{
    unsigned int roll = CP_Random_RangeInt(1, 4);

    if (roll == 1)
    {
        AddAmmo(2);
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

/* ---------------------------------------------------------- */
/* boss: random teleport */

static int CircleOverlapsRect(CP_Vector c, float r, CP_Vector rectPos, float w, float h)
{
    /* closest point on the rectangle to the circle's center */
    float nearX = CP_Math_ClampFloat(c.x, rectPos.x - w / 2.0f, rectPos.x + w / 2.0f);
    float nearY = CP_Math_ClampFloat(c.y, rectPos.y - h / 2.0f, rectPos.y + h / 2.0f);
    float dx = c.x - nearX;
    float dy = c.y - nearY;

    return (dx * dx + dy * dy < r * r);
}

/* is the boss allowed to stand at p? (nothing else is there) */
static int IsBossSpotFree(CP_Vector p)
{
    float r = BOSS_DIAMETER / 2.0f + BOSS_CLEARANCE;
    int   i;

    for (i = 0; i < wallCount; i++)
        if (CircleOverlapsRect(p, r, walls[i].pos, walls[i].w, walls[i].h))
            return 0;

    for (i = 0; i < boxCount; i++)
        if (CircleOverlapsRect(p, r, boxes[i].pos, boxes[i].w, boxes[i].h))
            return 0;

    for (i = 0; i < doorCount; i++)
    {
        /* an open door no longer blocks anything, so only closed ones count */
        if (!doors[i].open &&
            CircleOverlapsRect(p, r, doors[i].pos, doors[i].w, doors[i].h))
            return 0;

        if (AreCirclesIntersecting(p.x, p.y, r,
            buttons[i].pos.x, buttons[i].pos.y, BUTTON_DIAMETER / 2.0f))
            return 0;
    }

    for (i = 0; i < enemyCount; i++)
        if (!targets[i].hit &&
            AreCirclesIntersecting(p.x, p.y, r,
                targets[i].pos.x, targets[i].pos.y, TARGET_DIAMETER / 2.0f))
            return 0;

    for (i = 0; i < powerupCount; i++)
        if (powerups[i].active &&
            AreCirclesIntersecting(p.x, p.y, r,
                powerups[i].pos.x, powerups[i].pos.y, POWERUP_BOX_DIAMETER / 2.0f))
            return 0;

    /* stay away from the player (his shots start there) */
    if (AreCirclesIntersecting(p.x, p.y, r,
        playerPos.x, playerPos.y, PLAYER_DIAMETER / 2.0f + BOSS_PLAYER_CLEARANCE))
        return 0;

    /* stay out from under the speed button */
    if (CircleOverlapsRect(p, r, CP_Vector_Set(UI_SPEED_X, UI_SPEED_Y), UI_SPEED_W, UI_SPEED_H))
        return 0;

    return 1;
}

/* pick a random free spot that is at least minX pixels across the window
   (0 = anywhere). Pass 1 also insists on a real jump (BOSS_MIN_JUMP); pass 2
   accepts any free spot on that side; pass 3 drops the side rule if that side
   has no room at all. */
static void TeleportBoss(float minX)
{
    float w = (float)CP_System_GetWindowWidth();
    float h = (float)CP_System_GetWindowHeight();
    float r = BOSS_DIAMETER / 2.0f;
    float right = w - BOSS_MARGIN_SIDE - r;
    int   pass, tries;

    for (pass = 0; pass < 3; pass++)
    {
        float left = BOSS_MARGIN_SIDE + r;

        if (pass < 2 && minX > left)
            left = minX;
        if (left >= right)
            continue;                       /* nothing fits on that side: next pass */

        for (tries = 0; tries < BOSS_TELEPORT_TRIES; tries++)
        {
            CP_Vector p = CP_Vector_Set(
                CP_Random_RangeFloat(left, right),
                CP_Random_RangeFloat(BOSS_MARGIN_TOP + r, h - BOSS_MARGIN_BOTTOM - r));

            if (pass == 0 && CP_Vector_Distance(p, boss.pos) < BOSS_MIN_JUMP)
                continue;

            if (IsBossSpotFree(p))
            {
                boss.pos = p;
                return;
            }
        }
    }
    /* no free spot at all (only happens in very small windows): he stays put */
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
			CP_Sound_Play(enemyDeathSound);
        }
    }

    /* boss: every hit costs him a life and uses up the bullet;
       if he survives, he teleports to a free spot */
    if (bullet.active && boss.active && boss.lives > 0 &&
        AreCirclesIntersecting(bullet.pos.x, bullet.pos.y, r,
            boss.pos.x, boss.pos.y, BOSS_DIAMETER / 2.0f))
    {
        boss.lives--;
        bullet.active = 0;
        CP_Sound_Play(enemyDamageSound);

        if (boss.lives > 0)
            TeleportBoss(0.0f);          /* anywhere free */
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

static void DrawBoss(void)
{
    int i;

    if (!boss.active || boss.lives <= 0)
        return;

    CP_Image_Draw(bossImage, boss.pos.x, boss.pos.y, BOSS_DRAW_SIZE, BOSS_DRAW_SIZE, 255);

    /* life pips above his head: red = life left, gray = life lost */
    CP_Settings_Stroke(CP_Color_Create(0, 0, 0, 255));
    for (i = 0; i < BOSS_MAX_LIVES; i++)
    {
        float px = boss.pos.x + (i - (BOSS_MAX_LIVES - 1) / 2.0f) * BOSS_PIP_STEP;
        float py = boss.pos.y - BOSS_DIAMETER / 2.0f - 20.0f;

        CP_Settings_Fill(i < boss.lives ? CP_Color_Create(255, 60, 60, 255)
            : CP_Color_Create(60, 60, 60, 255));
        CP_Graphics_DrawCircle(px, py, BOSS_PIP_SIZE);
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
					CP_Sound_Play(powerupSound);
                    if (CP_Random_GetBool())
                        AddAmmo(2);
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

    if (boss.active && boss.lives > 0)
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

    levelStartAmmo = MAX_AMMO;
    boss.active = 0;
    boss.lives = 0;
    boss.pos = CP_Vector_Set(0.0f, 0.0f);

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
      /* combines everything */
        enemyCount = 0;               /* no normal enemies: the boss is the only target */
        wallCount = 3;
        boxCount = 1;
        doorCount = 1;
        powerupCount = 2;

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

        /* boss: the only target. Needs 3 hits and teleports after each one.
           Placed last so the free-spot check sees everything else in the level. */
        boss.active = 1;
        boss.lives = BOSS_MAX_LIVES;
        TeleportBoss(w * BOSS_START_MIN_X);   /* his starting spot: right side, away from the player */
        levelStartAmmo = BOSS_LEVEL_AMMO;
        break;

	case 7: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 8: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 9: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 10: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 11: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 12: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 13: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 14: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 15: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 16: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 17: //placeholder levels for testing
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
        break;

    case 18: //placeholder levels for testing
        default: 
        enemyCount = 1;
        wallCount = 1;

        targets[0].pos = CP_Vector_Set(w * 0.85f, h * 0.3f);

        walls[0].pos = CP_Vector_Set(w * 0.5f, h * 0.35f);
        walls[0].w = 30.0f;
        walls[0].h = 500.0f;
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
    speedBtn1xImage = CP_Image_Load("Assets/1xspd.png");
    speedBtn2xImage = CP_Image_Load("Assets/2xspd.png");
    bossImage = CP_Image_Load("Assets/boss_enemy.png");

    shootSound = CP_Sound_Load("Assets/pencil_shoot.mp3");
	enemyDamageSound = CP_Sound_Load("Assets/enemy_damage.mp3");
	enemyDeathSound = CP_Sound_Load("Assets/enemy_death.mp3");
    powerupSound = CP_Sound_Load("Assets/powerup_get.mp3");


    Progress_LoadBackground();

    if (!seeded)
    {
        CP_Random_Seed((int)time(NULL));   /* different random rolls every launch */
        seeded = 1;
    }

    playerPos = CP_Vector_Set(w * 0.1f, h * 0.85f);

    SetupLevel(Progress_GetCurrentLevel());

    bullet.active = 0;
    bullet.piercing = 0;
    speedBoosted = 0;
    aiming = 0;
    ammo = levelStartAmmo;
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
                speedBoosted = 0;   /* new bullet always starts at 1x */
                bullet.piercing = pendingPierce;
                pendingPierce = 0;
                aiming = 0;
                ammo--;
                CP_Sound_Play(shootSound);
            }
        }

        if (bullet.active && CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT) &&
            IsAreaClicked(140.0f, 60.0f, 440.0f, 160.0f, CP_Input_GetMouseX(), CP_Input_GetMouseY()))
        {
            float factor = speedBoosted ? 0.5f : 2.0f;
            bullet.vel.x *= factor;
            bullet.vel.y *= factor;
            speedBoosted = !speedBoosted;
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
    DrawBoss();

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

    if (bullet.active)
    {
        CP_Image_Draw(speedBoosted ? speedBtn2xImage : speedBtn1xImage,
            140.0f, 60.0f, 440.0f, 160.0f, 255);
    }
    DrawAmmo();



    if (awaitingQuestion)
        DrawQuestionPopup();
}

void Game_Exit(void)
{
    CP_Sound_Free(shootSound);
    CP_Sound_Free(enemyDamageSound);
    CP_Sound_Free(enemyDeathSound);
    CP_Sound_Free(powerupSound);
}