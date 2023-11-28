#include "stm32f10x.h"
#include "IERG3810_KEY.h"
#include "IERG3810_LED.h"
#include "IERG3810_USART.h"
#include "IERG3810_clock.h"
#include "IERG3810_Interrupt.h"
#include "IERG3810_TFTLCD.h"
#include "Global.h"
#include "character.h"
#include "Bullet.h"
#include <stdlib.h>

u16 count = 0;
u16 clear = 0;

int character_bullet_exist[10] = {0};
u16 character_bullet[10] = {0};        // record x location of the bullet
int b = 0;
int draw_check = 0;

u32 sheep = 0;
u32 timeout = 10000;
u32 ps2count = 0;
u32 ps2key = 0;
u8 ps2dataReady = 0;
u8 i = 0;
u8 buffer[2] = {0};
u8 ps2index = 0;  //index of buffer
int key = -1;
int fight_end = 0; //detect the end of fight
int win_lose = -1; //win or lose
int atk = 0;
int def = 0;
u16 enemy_origin = 0xAF;
u16 me_origin_y = 0x85;
int key8Counter = 0;
int rockCounter = 0;
int rockRand = -1;

void Delay(u32 t);
struct Character{ // 16*16
	u16 x;
	u16 y;        // the location of the character
	int hpvalue;  // the hp value of the character
    int fire;     // fire status: 1 = fire, 0 = default
	int rush;     // rush status: 1 = rush, 0 = default
};
struct Bullet{
	u16 x;
	u16 y;
	int exist;
};
// This is ps2key
void IERG3810_PS2key_ExtiInit(void) {
	/*setting: PC11*/ /*EXTI-11*/
	RCC->APB2ENR |= 1 << 4;                  // enable port - A clock

	GPIOC->CRH &= 0xFFFF0FFF;                // modify PC10 PC11
	GPIOC->CRH |= 0x00008000;                // pull high / low mode '10', input '00'
	GPIOC->ODR |= 1 << 11;                    // pull high
	RCC->APB2ENR |= 0x01;                    // Enable AFIO clock

	AFIO->EXTICR[2] &= 0xFFFF0FFF;          //£¨RM0008, AFIO EXTICR1, page - 192)
	AFIO->EXTICR[2] |= 0x00002000;          // [15:12] 0010
	EXTI->IMR |= 1 << 11;                    //£¨RM0008, page - 211) edge trigger
	EXTI->FTSR |= 1 << 11;                   //£¨RM0008, page - 212) falling edge

	NVIC->IP[40] = 0x35;                     // set priority of this interrupt; EXTI0->IRQ6; 11->IRQ40
	//NVIC->IP[EXTI15_10_IRQn] = 0x10;
	NVIC->ISER[1] &= ~(1 << 8);             //set NVIC 'SET ENABLE REGISTER'
											//DDI0337E page - 8 - 3
	NVIC->ISER[1] |= (1 << 8);              // IRQ40

}

void IERG3810_PS2key_DataInit() {

	/*setting: PC10*/
	RCC->APB2ENR |= 1 << 4;

	GPIOC->CRH &= 0xFFFFF0FF;
	GPIOC->CRH |= 0x00000800;
	GPIOC->BRR = 1;
}
void EXTI15_10_IRQHandler(void) {

	if (ps2count > 0 && ps2count < 9) { //1-8
		ps2key |= (u32)((GPIOC->IDR & GPIO_Pin_10) >> 10) << (ps2count - 1);  // use a mask to get data in C10;
															 // then shift it to the correct bit of ps2key;
	}
	ps2count++;
	Delay(10);  //We found that the processor is too fast and get error.
	// A short delay can eliminate the error.
	EXTI->PR = 1 << 11;  //Clear this exception pending bit
}
// End of ps2key part
void IERG3810_SYSTICK_Init10ms(void)
{

	SysTick->CTRL = 0;         // clear
	SysTick->LOAD = 719999; // Refer to DDI0337E page 8-10. 

	SysTick->CTRL |= 0x00000004;           //SysTick->CTRL |= SysTick_CTRL_CLKSOURCE; set bit 2
	SysTick->CTRL |= SysTick_CTRL_TICKINT; // Enables SysTick interrupt, 1 = Enable, 0 = Disable
	SysTick->CTRL |= SysTick_CTRL_ENABLE;  // Enable SysTick

}

struct Character me = { 0x0F, 0x85, 100, 0, 1 };
struct Character enemy = { 0xAF, 0x85, 100,1,1 };


int main(void)
{
	IERG3810_clock_tree_init();
	IERG3810_LED_Init();
	IERG3810_SYSTICK_Init10ms();
	IERG3810_TFILCH_Init();
	IERG3810_NVIC_SetPriorityGroup(5); //set PRIGROUP
	IERG3810_PS2key_ExtiInit();//Init PS2 keyboard as an interrupt input
	IERG3810_PS2key_DataInit();
	IERG3810_TFILCH_Init();
	DS0_OFF();
	DS1_OFF();
	IERG3810_TFTLCD_FillRectangle(0x0000, 0,0x200, 0, 0x200);
	//fight_end = 1; win_lose = 0;
	while (1)
	{
		if (fight_end == 1) {
			if(draw_check == 0){
				IERG3810_TFTLCD_FillRectangle(0x0046, 0, 0xF0, 0, 0xFFF);
				draw_check =1;
				DS0_OFF();
			}
			Show_WinLose(win_lose);
			Delay(10000);
		}
		else {
			if (task1HeartBeat == 1) {
				Draw_HPBar(0x8A, 0xF0, enemy.hpvalue);
				Draw_HPBar(0x0A, 0xF0, me.hpvalue);
			}
			if(task1HeartBeat ==0){
				Draw_Instruction();
				Draw_Character(me.x, me.y);
				Draw_Monster(enemy.x, enemy.y);
			}
			if (task1HeartBeat >= 2) { // a cycle = 10ms
				task1HeartBeat = 0;
				count++;
				clear++;

				// move
				switch (key) {
				case 4:
					if (me.x - 0x02 > 0x0) {
						me.x = me.x - 0x02;
					}
					key = -1;
					break;
				case 6:
					if (me.x + 0x02 < 0xF0) {
						me.x = me.x + 0x02;
					}
					key = -1;
					break;
				case 0:         // fire
					me.fire = 1;
					break;
				case 8:
					if (me.y + 0x11 < 0x9F) {
						me.y = me.y + 0x11;
					}
					key8Counter = 1;
					key = -1;
					break;
				}
				if (key8Counter != 0) {      // keep the current y position for a time period
					key8Counter++;
					if (key8Counter == 8) {
						me.y = me_origin_y;
						key8Counter = 0;
						IERG3810_TFTLCD_FillRectangle(0x0000, 0, 0xF0, me.y + 0x11, 0xFF);
					}
				}

				if (me.fire == 1 && me.y == me_origin_y) {
					Create_Character_Bullet(character_bullet_exist, character_bullet, me.x);
					me.fire = 0;
					key = -1;
				}
				for (b = 0; b < 10; b++) {               // update and draw the bullet
					if (character_bullet_exist[b] == 1) {
						if (character_bullet[b] + 0x1 >= 0xF0) // reach the boundary
							Destroy_Character_Bullet(character_bullet_exist, character_bullet, b); // destroy
						else if (me.y == enemy.y && character_bullet[b] + 0x02 >= enemy.x && character_bullet[b] <= enemy.x) {
							Destroy_Character_Bullet(character_bullet_exist, character_bullet, b); // destroy
							enemy.hpvalue = enemy.hpvalue - 1 - atk;
							enemy.x = enemy.x + 0x01;
							IERG3810_TFTLCD_FillRectangle(0x0000, 0, 0xF0, 0xEF, 0x1F);
						}
						else {
							character_bullet[b] = character_bullet[b] + 0x02;
							//Draw_Bullet(character_bullet[b], me.y + 0x05);
						}
					}
				}
				for (b = 0; b < 10; b++) {
					if (character_bullet_exist[b] == 1)
						Draw_Bullet(character_bullet[b], me_origin_y + 0x05);
				}

				// enemy rush
				if (enemy.rush == 1) {
					if (enemy.x - 0x02 > 0x0 && enemy.x > me.x) {
						enemy.x = enemy.x - 0x02;
					}
					else {
						enemy.x = enemy_origin;
						enemy.rush = 0;
					}
				}

				// detect rush collision
				if ((me.x + 0x02 >= enemy.x && me.x <= enemy.x && enemy.y == me.y) || (me.x - 0x02 <= enemy.x && me.x >= enemy.x && enemy.y == me.y)) {
					DS0_ON();
					me.hpvalue = me.hpvalue - 3 + def;
					IERG3810_TFTLCD_FillRectangle(0x0000, 0, 0xF0, 0xEF, 0x1F);
				}
				// rock drop
				if (enemy.hpvalue < 40) {
					//Draw_Rock(me.x, me_origin_y + 0x2F);
					rockCounter = 1;
				}
				if(rockCounter == 2){
					if(rockRand !=-1){
						SGE(rockRand , 12, 0x08, 0x8800, 0x0000);  // generate rock according to the random number
						if(me.x >= rockRand*16 && me.x <= (rockRand+1)*16)
							me.hpvalue = me.hpvalue-10;
					}
				}

				if (clear > 0x02) {
					IERG3810_TFTLCD_FillRectangle(0x0000, 0, 0xF0, 0x85, 0x15);
					clear = 0;
					DS0_OFF();
				}
				// time slot between monster rush
				if (count > 0x1F) {
					count = 0;
					enemy.rush = 1;
				}
				if (me.hpvalue <= 0 || enemy.hpvalue <= 0) {
					fight_end = 1;
					if(me.hpvalue >= enemy.hpvalue) win_lose = 1;
					else win_lose = 0;
				}
			}
			if (task2HeartBeat >= 15) {
				task2HeartBeat = 0;
				if(rockCounter == 2){
					rockCounter = 0;
				}
				if(rockCounter == 1){
					rockRand = rand()%2;
					rockCounter=2;     // 2 means pending
				}
			}


			sheep++; // count sheep :D
			 //if PS2 keybaord received data correctly

			if (ps2count >= 11)
			{
				//EXTI->IMR &= ~(1 << 11); optional, suapend interrupt

					//IERG3810_TFTLCD_FillRectangle(0x0000, 0x0,0x1000, 0x0, 0x2000);
				buffer[ps2index++] = ps2key;

				switch (ps2key) {
				case 0x70: // "0" on PS/2 keyboard
					key = 0;
					break;
				case 0x69: // "1" on PS/2 keyboard
					key = 1;
					break;
				case 0x72: // "2" on PS/2 keyboard
					key = 2;
					break;
				case 0x7A: // "3" on PS/2 keyboard
					key = 3;
					break;
				case 0x6B: // "4" on PS/2 keyboard
					key = 4;
					break;
				case 0x73: // "5" on PS/2 keyboard
					key = 5;
					break;
				case 0x74: // "6" on PS/2 keyboard
					key = 6;
					break;
				case 0x6C: // "7" on PS/2 keyboard
					key = 7;
					break;
				case 0x75: // "8" on PS/2 keyboard
					key = 8;
					break;
				case 0x7D: // "9" on PS/2 keyboard
					key = 9;
					break;
				case 0x7C: // "*" on PS/2 keyboard
					key = 99;
					break;
				case 0xF0: //break
					key = -1;
					break;
				default:
					// Handle other keys if needed
					key = -1;
					break;
				}

				EXTI->PR = 1 << 11;//Clear this exception pending bit
				ps2key = 0;
				ps2count = 0;
			}   //end of "if PS2 keybaord received data correctly"

			timeout--;
			if (timeout == 0)//clear PS2 keyboard data when timeout
			{
				timeout = 20000;
				ps2key = 0;
				ps2count = 0;
			}  // end of "clear PS2 keyboard data when timeout"
		}

	}
}
/* collision detection & HP
if(bullet.exist == 1){
	bullet.x = count;
	Draw_Bullet(bullet.x,bullet.y);

	if (bullet.x == me.x && bullet.y == me.y){  // detect collision
		DS0_ON();
		bullet.exist=0;
		me.hpvalue = me.hpvalue - 2;
	}
}
*/
/* move1: rush
if (me.rush == 1) {
	if (me.x + 0x08 < 0xF0) {
		me.x = me.x + 0x08;
		Draw_Character(me.x, me.y);
	}
	else me.rush = 0;
}
*/
//else if ((character_bullet[b] + 0x01 >= enemy.x && character_bullet[b] <= enemy.x) || (character_bullet[b] - 0x01 <= enemy.x && character_bullet[b] >= enemy.x)) {
