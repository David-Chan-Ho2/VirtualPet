#include "stm32f0xx_hal.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>

#include "gfx01m2.h"

// ===================== RTOS Objects =====================
static osThreadId_t renderTaskHandle;
static osThreadId_t inputTaskHandle;
static osThreadId_t logicTaskHandle;
static osMutexId_t  lcdMutex;
static osMessageQueueId_t inputQueue;

// ===================== Game State =====================
typedef enum {
    APP_IDLE = 0,
    APP_MENU,
    APP_PLAYING,
    APP_FEEDING,
	APP_SLEEPING,
    APP_RAN_AWAY,
    APP_DEAD
} AppState;

typedef enum {
    BTN_NONE = 0,
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_CENTER
} BtnEvent;

typedef struct {
    int happiness;
    int hunger;
    int sleep;
    AppState state;
    int menuIndex;
} Pet;

static Pet pet = { .happiness = 80, .hunger = 80, .sleep = 80, .state = APP_IDLE, .menuIndex = 0 };

static uint8_t clear_screen_flag = 0;

#define SPR_W 32
#define SPR_H 32 - 8

#define DRAW_X 50
#define DRAW_Y (DRAW_X + SPR_H)

// ===================== Palette + Sprite “XPM-lite” =====================
static const char *FACE[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYLRRLYYYYRRYL.......",
"......LYYYYYYLRRLYYYYYYL........",
".......LLYYYYYLLYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *IDLE_0[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYLRRLYYYYRRYL.......",
"......LYYYYYYLRRLYYYYYYL........",
".......LLYYYYYLLYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *IDLE_1[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYL.LLYYYYYL.LLYYL.......",
"......LYYLLLLYYYYYLLLLYYL.......",
"......LYYYLLYYLLYYYLLYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYLRRLYYYYRRYL.......",
"......LYYYYYYLRRLYYYYYYL........",
".......LLYYYYYLLYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *EAT_0[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYL.LLYYYYYL.LLYYL.......",
"......LYYLLLLYYYYYLLLLYYL.......",
"......LYYYLLYYLLYYYLLYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYLRRLYYYYRRYBBB.....",
"......LYYYYYYLRRLYYYYYYLBBB.....",
".......LLYYYYYLLYYYYYLL.BBB.....",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *EAT_1[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYL.LLYYYYYL.LLYYL.......",
"......LYYLLLLYYYYYLLLLYYL.......",
"......LYYYLLYYLLYYYLLYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYYYYYBBBYRRYL.......",
"......LYYYYYYYYYYBBBYYYL........",
".......LLYYYYYYYYBBBYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *PLAY_0[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYLRRLYYYYRRYL.......",
"......LYYYYYYLRRLYYYYYYL...AAA..",
".......LLYYYYYLLYYYYYLL....AAA..",
".........LLLYYYYYYLLL......AAA..",
"............LLLLLL.............."
};

static const char *PLAY_1[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL.AAA..",
".....LLYRRYYYLRRLYYYYRRYL..AAA..",
"......LYYYYYYLRRLYYYYYYL...AAA..",
".......LLYYYYYLLYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *SLEEP_0[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYYYYYYYYYRRYL.......",
"......LYYYYYYYYYYYYYYYYL........",
".......LLYYYYYYYYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static const char *SLEEP_1[SPR_H] = {
"..LL.......................LL...",
"..LLLL....................LLL...",
"..LLLYL..................LLLL...",
"..LLLYYLL............LLLLYLLL...",
"...LLYYYYL..LLLLL..LLYYYYYLL....",
"...LLYYYYYLLYYYYYLLYYYYYYYLL....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
"....LYYYYYYYYYYYYYYYYYYYYYL.....",
".....LYYYYYYYYYYYYYYYYYYYL......",
"......LYLYYYYYYYYYYYYYLYL.......",
".......LYYYYYYYYYYYYYYYL........",
".......LYYYYYYYYYYYYYYYL........",
"......LYYYYYYYYYYYYYYYYYL.......",
"......LYYLYYLYYYYYLYYLYYL.......",
"......LYYYLLYYYYYYYLLYYYL.......",
"......LYYYYYYYLLYYYYYYYYL.......",
".....LYYRRYYYYYYYYYYYRRYYL......",
".....LYRRRRYLYLLYLYYRRRRYL......",
".....LYRRRRYYLLLLYYYRRRRYL......",
".....LLYRRYYYYYYYYYYYRRYL.......",
"......LYYYYYYYYYYYYYYYYL........",
".......LLYYYYYYYYYYYYLL.........",
".........LLLYYYYYYLLL...........",
"............LLLLLL.............."
};

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b){
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static uint16_t SpriteColor(char c){
	switch(c){
		case 'Y': return rgb565(0xFF,0xD7,0x00); // Yellow
		case 'R': return rgb565(0xFF,0x00,0x00); // Red
		case 'B': return rgb565(0xA0,0x52,0x2D); // Brown
		case 'A': return rgb565(0x20,0xF8,0xE0); // Aqua
		case 'L': return rgb565(0x00,0x00,0x00); // Black
		case '.': default: return rgb565(0xFF,0xFF,0xFF); // White
	}
}

#define SPR_SCALE 4

static void DrawSpriteXPM(const char **sprite, uint16_t x, uint16_t y)
{
    for (int r = 0; r < SPR_H; r++) {
        const char *row = sprite[r];

        for (int c = 0; c < SPR_W; c++) {
            uint16_t color = SpriteColor(row[c]);

            uint16_t px = x + c * SPR_SCALE;
            uint16_t py = y + r * SPR_SCALE;

            for (int dy = 0; dy < SPR_SCALE; dy++) {
                for (int dx = 0; dx < SPR_SCALE; dx++) {
                    LCD_DrawImage(&color, px + dx, py + dy, 1, 1);
                }
            }
        }
    }
}

static void DrawSpriteXPM2(const char **sprite, uint16_t x, uint16_t y)
{
    for (int r = 11; r < SPR_H-2; r++) {
        const char *row = sprite[r];

        for (int c = 2; c < SPR_W-3; c++) {
            uint16_t color = SpriteColor(row[c]);

            uint16_t px = x + c * SPR_SCALE;
            uint16_t py = y + r * SPR_SCALE;

            for (int dy = 0; dy < SPR_SCALE; dy++) {
                for (int dx = 0; dx < SPR_SCALE; dx++) {
                    LCD_DrawImage(&color, px + dx, py + dy, 1, 1);
                }
            }
        }
    }
}

// ===================== UI Helpers =====================
static inline int clamp(int v, int lo, int hi){ return (v<lo)?lo : (v>hi)?hi : v; }

static void DrawMenu(void) {
    osMutexAcquire(lcdMutex, osWaitForever);
    LCD_DrawString("== POKEDEX ==", 24, 52, 0xFFFF, 0x0000);
    const char* items[3] = {"Name: Pikachu #0025", "Type: Electric", "Weakness: Ground"};
    for (int i=0;i<3;i++){
        LCD_DrawString(items[i], 24, 72 + 14*i, 0x0000, 0xFFFF);
    }

    LCD_DrawString("Press Center to Exit", 24,  72 + 14 * 4, 0xFFFF, 0x0000);
    osMutexRelease(lcdMutex);
}

static void drawMenu() {

	LCD_DrawString("^", 50 + 54, 214, 0x0000, 0xFFFF);
	LCD_DrawString("v", 50 + 54, 226, 0x0000, 0xFFFF);
	LCD_DrawString("<", 50 + 43, 220, 0x0000, 0xFFFF);
	LCD_DrawString(">", 50 + 65, 220, 0x0000, 0xFFFF);
	LCD_DrawString("Feed", 50 + 44, 200, 0x0000, 0xFFFF);
	LCD_DrawString("Play", 50 + 0, 220, 0x0000, 0xFFFF);
	LCD_DrawString("Sleep", 50 + 85, 220, 0x0000, 0xFFFF);
	LCD_DrawString("Pokedex", 84, 240, 0x0000, 0xFFFF);

}

static void DrawIdle(int frame) {
    osMutexAcquire(lcdMutex, osWaitForever);
    DrawSpriteXPM2(frame ? IDLE_1 : IDLE_0, DRAW_X, DRAW_Y);

    drawMenu();
    osMutexRelease(lcdMutex);
}

static void DrawAnimFeed(int frame){
    osMutexAcquire(lcdMutex, osWaitForever);
    DrawSpriteXPM2(frame ? EAT_1 : EAT_0, DRAW_X, DRAW_Y);
    osMutexRelease(lcdMutex);
}

static void DrawAnimPlay(int frame){
    osMutexAcquire(lcdMutex, osWaitForever);
    DrawSpriteXPM2(frame ? PLAY_1 : PLAY_0, DRAW_X, DRAW_Y);
    osMutexRelease(lcdMutex);
}

static void DrawAnimSleep(int frame){
    osMutexAcquire(lcdMutex, osWaitForever);
    DrawSpriteXPM2(frame ? SLEEP_1 : SLEEP_0, DRAW_X, DRAW_Y);

    LCD_DrawString(frame ? "zzzz" : "ZZZZ", 100, 94, 0x0000, 0xFFFF);

    osMutexRelease(lcdMutex);
}

static void DrawEndScreen(const char* title, uint16_t color) {
    osMutexAcquire(lcdMutex, osWaitForever);
    LCD_DrawString(title, 10, 60, color, 0x0000);
    LCD_DrawString("Press CENTER to reset", 10, 80, 0xFFFF, 0x0000);
    osMutexRelease(lcdMutex);
}

static void ThreadSafeClearScreen(){
	osMutexAcquire(lcdMutex, osWaitForever);
	LCD_Clear(0xFFFF);
	osMutexRelease(lcdMutex);
}

// ===================== Input Task =====================
static uint8_t readDebounced(void){
    uint8_t s1 = Joystick_Read();
    osDelay(20);
    uint8_t s2 = Joystick_Read();
    return (s1==s2) ? s1 : 0;
}

static BtnEvent mapButtons(uint8_t state){
    if (state & JOY_UP)     return BTN_UP;
    if (state & JOY_DOWN)   return BTN_DOWN;
    if (state & JOY_LEFT)   return BTN_LEFT;
    if (state & JOY_RIGHT)  return BTN_RIGHT;
    if (state & JOY_CENTER) return BTN_CENTER;
    return BTN_NONE;
}

static void InputTask(void *arg){
    while(1){
        uint8_t s = readDebounced();
        BtnEvent e = mapButtons(s);
        if (e != BTN_NONE) {
            osMessageQueuePut(inputQueue, &e, 0, 0);
            while (Joystick_Read()) { osDelay(15); }
        }
        osDelay(15);
    }
}

// ===================== Logic Task =====================
#define MAX_HAPPINESS	100
#define MAX_HUNGER		100
#define MAX_SLEEP       100

static void StartAction(AppState action){
    if (action == APP_FEEDING)  pet.hunger    = clamp(pet.hunger + 20, 0, MAX_HUNGER);
    if (action == APP_PLAYING)  pet.happiness = clamp(pet.happiness + 20, 0, MAX_HAPPINESS);
    if (action == APP_SLEEPING) pet.sleep     = clamp(pet.sleep + 20, 0, MAX_SLEEP);
    pet.state = action;
}

static void LogicTask(void *arg){
    uint32_t nextTick = osKernelGetTickCount() + 2000U;
    const uint32_t decayPeriodMs = 100U;

    while(1){
    	osDelayUntil(nextTick);
    	nextTick += decayPeriodMs;

        if (pet.state == APP_IDLE || pet.state == APP_MENU) {
            pet.happiness = clamp(pet.happiness - 1, 0, MAX_HAPPINESS);
            pet.hunger    = clamp(pet.hunger    - 1, 0, MAX_HUNGER);
            pet.sleep     = clamp(pet.sleep     - 1, 0, MAX_SLEEP);
        }

        if (pet.state != APP_DEAD && pet.state != APP_RAN_AWAY) {
            if (pet.hunger == 0){
            	clear_screen_flag = 1;
            	pet.state = APP_DEAD;
            }
            if (pet.happiness == 0) {
            	clear_screen_flag = 1;
            	pet.state = APP_RAN_AWAY;
            }
            if (pet.sleep == 0) {
                clear_screen_flag = 1;
                pet.state = APP_DEAD;
            }
        }

        BtnEvent e;
        while (osMessageQueueGet(inputQueue, &e, NULL, 0) == osOK) {
            if (pet.state == APP_DEAD || pet.state == APP_RAN_AWAY) {
                if (e == BTN_CENTER) {
                	clear_screen_flag = 1;
                    pet.happiness = MAX_HAPPINESS;
                    pet.hunger = MAX_HUNGER;
                    pet.sleep = MAX_SLEEP;
                    pet.menuIndex = 0;
                    pet.state = APP_IDLE;
                }
                continue;
            }

            switch (pet.state) {
				case APP_IDLE:

					if (e == BTN_UP){
						StartAction(APP_FEEDING);
					} else if (e == BTN_LEFT) {
						StartAction(APP_PLAYING);
					} else if (e == BTN_RIGHT) {
						StartAction(APP_SLEEPING);
					} else if (e == BTN_DOWN) {
						clear_screen_flag = 1;
						StartAction(APP_MENU);
					}

					break;
				case APP_MENU:
					if (e == BTN_CENTER) {
						clear_screen_flag = 1;

						pet.state = APP_IDLE;
					}
					break;
				default:
					break;
            }
        }
    }
}

// ===================== Render Task =====================
#define ANIMATION_TICKS		4000

static void RenderTask(void *arg){
    int frame = 0;

    while(1){
    	if(clear_screen_flag){
    		ThreadSafeClearScreen();
    		clear_screen_flag = 0;
    	}

        switch (pet.state) {
        case APP_IDLE:
        	osMutexAcquire(lcdMutex, osWaitForever);
			DrawSpriteXPM(FACE, DRAW_X, DRAW_Y);
			drawMenu();
			osMutexRelease(lcdMutex);
            DrawIdle(frame ^= 1);
            osDelay(320);
            break;

        case APP_MENU:
            DrawMenu();
            osDelay(60);
            break;

        case APP_FEEDING: {
            uint32_t start = osKernelGetTickCount();
            while ((osKernelGetTickCount() - start) < ANIMATION_TICKS) {
                DrawAnimFeed(frame ^= 1);
                osDelay(180);
            }
            pet.state = APP_IDLE;
            break;
        }

        case APP_PLAYING: {
            uint32_t start = osKernelGetTickCount();
            while ((osKernelGetTickCount() - start) < ANIMATION_TICKS) {
                DrawAnimPlay(frame ^= 1);
                osDelay(180);
            }
            pet.state = APP_IDLE;
            break;
        }

        case APP_SLEEPING: {
            uint32_t start = osKernelGetTickCount();
            while ((osKernelGetTickCount() - start) < ANIMATION_TICKS) {
                DrawAnimSleep(frame ^= 1);
                osDelay(180);
            }
            pet.state = APP_IDLE;
            break;
        }

        case APP_RAN_AWAY:
            DrawEndScreen("Your pet ran away :(", 0xF800);
            osDelay(140);
            break;

        case APP_DEAD:
            DrawEndScreen("Your pet died :(", 0xF800);
            osDelay(140);
            break;
        }
    }
}

// ===================== HAL + System =====================

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    while(1);
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
	  while(1);
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
	  while(1);
  }
}


// ===================== CMSIS-RTOS2 Bootstrap =====================
int main(void) {
    HAL_Init();
    SystemClock_Config();

    LCD_Init();
    LCD_Clear(0xFFFF);

    osKernelInitialize();

    lcdMutex   = osMutexNew(NULL);
    inputQueue = osMessageQueueNew(8, sizeof(BtnEvent), NULL);

    const osThreadAttr_t render_attr = { .name="Render", .stack_size=1024, .priority=osPriorityNormal };
    const osThreadAttr_t input_attr  = { .name="Input",  .stack_size=512,  .priority=osPriorityAboveNormal };
    const osThreadAttr_t logic_attr  = { .name="Logic",  .stack_size=768,  .priority=osPriorityNormal };

    renderTaskHandle = osThreadNew(RenderTask, NULL, &render_attr);
    inputTaskHandle  = osThreadNew(InputTask,  NULL, &input_attr);
    logicTaskHandle  = osThreadNew(LogicTask,  NULL, &logic_attr);

    osKernelStart();
    while(1);
}
