// Lab10.c
// Runs on TM4C123
// Elijah Pustilinik and Siddharth Sunder
// This is a starter project for the EE319K Lab 10

// Last Modified: 5/4/2021
// http://www.spaceinvaders.de/
// sounds at http://www.classicgaming.cc/classics/spaceinvaders/sounds.php
// http://www.classicgaming.cc/classics/spaceinvaders/playguide.php
/* 
 Copyright 2021 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
// ******* Possible Hardware I/O connections*******************
// Slide pot pin 1 connected to ground
// Slide pot pin 2 connected to PD2/AIN5
// Slide pot pin 3 connected to +3.3V 
// fire button connected to PE0
// special weapon fire button connected to PE1
// 8*R resistor DAC bit 0 on PB0 (least significant bit)
// 4*R resistor DAC bit 1 on PB1
// 2*R resistor DAC bit 2 on PB2
// 1*R resistor DAC bit 3 on PB3 (most significant bit)
// LED on PB4
// LED on PB5

// VCC   3.3V power to OLED
// GND   ground
// SCL   PD0 I2C clock (add 1.5k resistor from SCL to 3.3V)
// SDA   PD1 I2C data

//************WARNING***********
// The LaunchPad has PB7 connected to PD1, PB6 connected to PD0
// Option 1) do not use PB7 and PB6
// Option 2) remove 0-ohm resistors R9 R10 on LaunchPad
//******************************

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "SSD1306.h"
#include "Print.h"
#include "Random.h"
#include "ADC.h"
#include "Images.h"
#include "Sound.h"
#include "Timer0.h"
#include "Timer1.h"
#include "TExaS.h"
#include "Switch.h"
//********************************************************************************
// debuging profile, pick up to 7 unused bits and send to Logic Analyzer
#define PB54                  (*((volatile uint32_t *)0x400050C0)) // bits 5-4
#define PF321                 (*((volatile uint32_t *)0x40025038)) // bits 3-1
// use for debugging profile
#define PF1       (*((volatile uint32_t *)0x40025008))
#define PF2       (*((volatile uint32_t *)0x40025010))
#define PF3       (*((volatile uint32_t *)0x40025020))
#define PB5       (*((volatile uint32_t *)0x40005080)) 
#define PB4       (*((volatile uint32_t *)0x40005040)) 
	
//defining struct for sprite
struct sprite{
	int32_t x; // x coor 0 to 127
	int32_t y; // y coor 0 to 63
	int32_t vx; // pixels per 50ms
	int32_t vy;
	int32_t iv; //initial velocity
	int32_t center1X;
	int32_t center1Y;
	int32_t center2X;
	int32_t center2Y;
	int32_t center3X;
	int32_t center3Y;
	int32_t minDist1;
	int32_t minDist2;
	int32_t minDist3;
	const uint8_t *image;
	
}; typedef struct sprite sprite_t;


// **************SysTick_Init*********************
// Initialize Systick periodic interrupts
// Input: interrupt period
//        Units of period are 12.5ns
//        Maximum is 2^24-1
//        Minimum is determined by length of ISR
// Output: none
void SysTick_Init(unsigned long period){
  // write this
	NVIC_ST_CTRL_R = 0;
	NVIC_ST_CURRENT_R = 0;
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x20000000;
	NVIC_ST_RELOAD_R = period - 1;
	NVIC_ST_CTRL_R = 7;
}


// TExaSdisplay logic analyzer shows 7 bits 0,PB5,PB4,PF3,PF2,PF1,0 
// edit this to output which pins you use for profiling
// you can output up to 7 pins
void LogicAnalyzerTask(void){
  UART0_DR_R = 0x80|PF321|PB54; // sends at 10kHz
}
void ScopeTask(void){  // called 10k/sec
  UART0_DR_R = (ADC1_SSFIFO3_R>>4); // send ADC to TExaSdisplay
}
// edit this to initialize which pins you use for profiling
void Profile_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x22;      // activate port B,F
  while((SYSCTL_PRGPIO_R&0x20) != 0x20){};
  GPIO_PORTF_DIR_R |=  0x0E;   // output on PF3,2,1 
  GPIO_PORTF_DEN_R |=  0x0E;   // enable digital I/O on PF3,2,1
  GPIO_PORTB_DIR_R |=  0x30;   // output on PB4 PB5
  GPIO_PORTB_DEN_R |=  0x30;   // enable on PB4 PB5  
}
//********************************************************************************
 
void Delay100ms(uint32_t count); // time delay in 0.1 seconds
void Draw1(void);
void Draw2(void);
void Draw3(void);
void BallMove(void);
void AngleMove(void);
void PowerMove(void);
void LanguageSelect(void);
void Player1Turn(void);
void Player2Turn(void);
int32_t DistSquare(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void CollisionComp(void);
uint8_t language = 2;
void pauseGame(void);
void endGame(void);
uint32_t Random10(void); // for choosing wind speed magnitude between 0-10
int32_t Random1(void); //for choosing negative or positive wind speed
uint8_t pauseFlag = 1;
uint8_t player1score = 0;
uint8_t player2score = 0;
uint8_t windArr[11] = {0, 50, 45, 40, 35, 30, 25, 20, 15, 10, 5};

uint32_t Random10(void){
	
	return Random()%11;
}

int32_t Random1(void){
	// 0 or 1
	return Random()%2;
}

void TitleScreen(void){
	SSD1306_DrawBMP(0, 63, battleship_title, 0, SSD1306_WHITE);
	SSD1306_OutBuffer();
	Delay100ms(10);
	SSD1306_DrawString(5, 25, "Press any", SSD1306_WHITE);
	SSD1306_DrawString(5, 34, "button to", SSD1306_WHITE);
	SSD1306_DrawString(5, 43, "start", SSD1306_WHITE);
	SSD1306_OutBuffer();
	while((FireIn() != 1) && (PlayIn() != 1)){}
	Delay100ms(1);
}

void LanguageSelect(void){
	SSD1306_ClearBuffer();
	SSD1306_DrawString(30, 10, "Language: ", SSD1306_WHITE);
	SSD1306_DrawString(30, 20, "English(1)", SSD1306_WHITE);
	SSD1306_DrawString(30, 30, "Spanish(2)", SSD1306_WHITE);
	SSD1306_OutBuffer();
	while(language > 1){
		if(FireIn()){
				language = 1;
		}	
		if(PlayIn()){
			language = 0;
			}	
		}
	Delay100ms(1);
	}

	void pauseGame(void){
		DisableInterrupts();
		while(pauseFlag == 1){
			SSD1306_ClearBuffer();
			if(language == 1){
			SSD1306_DrawString(30, 20, "Game Paused", SSD1306_WHITE);
			}
			else if(language == 0){
				SSD1306_DrawString(42, 20, "Pausada", SSD1306_WHITE);
			}
			SSD1306_OutBuffer();
			if(PlayIn()){
				pauseFlag = 0;
				Delay100ms(1);
			}
		}
		EnableInterrupts();
	}
	void endGame(void){
		DisableInterrupts();
		while(pauseFlag == 1){
			SSD1306_ClearBuffer();
			if(player1score > 4){
				if(language == 1){
					SSD1306_DrawString(20, 20, "Player 1 Wins!", SSD1306_WHITE);
					SSD1306_DrawString(20, 29, "Press any button", SSD1306_WHITE);
					SSD1306_DrawString(20, 38, "to play again.", SSD1306_WHITE);
					SSD1306_OutBuffer();
				}
				else if(language == 0){
					SSD1306_DrawString(20, 20, "Jugador 1 Gana!", SSD1306_WHITE);
					SSD1306_DrawString(2, 29, "Pulsa cualqier boton", SSD1306_WHITE);
					SSD1306_DrawString(8, 38, "para jugar de nuevo.", SSD1306_WHITE);
					SSD1306_OutBuffer();
				}
			}
			else if(player2score > 4){
				if(language == 1){
					SSD1306_DrawString(20, 20, "Player 2 Wins!", SSD1306_WHITE);
					SSD1306_DrawString(20, 29, "Press any button", SSD1306_WHITE);
					SSD1306_DrawString(20, 38, "to play again.", SSD1306_WHITE);
					SSD1306_OutBuffer();
				}
				else if(language == 0){
					SSD1306_DrawString(20, 20, "Jugador 2 Gana!", SSD1306_WHITE);
					SSD1306_DrawString(2, 29, "Pulsa cualqier boton", SSD1306_WHITE);
					SSD1306_DrawString(8, 38, "para jugar de nuevo.", SSD1306_WHITE);
					SSD1306_OutBuffer();
				}
			}
			SSD1306_OutBuffer();
			if(PlayIn() || FireIn()){
				pauseFlag = 0;
				player1score = 0;
				player2score = 0;
				Delay100ms(1);
			}
		}
		EnableInterrupts();
		
		/*if (player1score > 4){
			SSD1306_ClearBuffer();
			if(language == 1){
				SSD1306_DrawString(20, 30, "Player 1", SSD1306_WHITE);
				
			}
		} */
	}

uint32_t ConvertAngle(uint32_t data){
	
	if(data < 50){
		return 0;
	}
	if(data > 3900){
		return 85;
	}
	
	return ((data-45)/44);
}

uint32_t ConvertVelocity(uint32_t data){
	if(data < 50){
		return 0;
	}
	if(data > 3900){
		return 20;
	}
	
	return ((data-50)/200);	//converts ADC input to value between 0 and 20
}

uint32_t tangent(float data){
		//returns a 3-digit integer (e.g. reuturn of 121 == slope of 1.21)
		data = data*0.0174533;			//convert data (angle) to radians
		return 100*((data) + ((data*data*data)/(3)) + ((2*data*data*data*data*data)/(15)) + ((17*data*data*data*data*data*data*data)/(315)) + ((62*data*data*data*data*data*data*data*data*data)/2835));
}


void drawArrowLeft (uint32_t data){
	//starting position is 17,55
	uint32_t slope = tangent(data);
	int32_t xvalues[20];
	for(int i = 0; i < 20; i++){
		xvalues[i] = i+18;
	}
	int32_t yvalues[20];
	for(int i = 0; i < 20; i++){
		yvalues[i] = 51-(((slope*((i-1)+1))+50)/100);
	}
	
	for(int i = 0; i < 20; i++){
		SSD1306_DrawBMP(xvalues[i], yvalues[i], cannonball, 0, SSD1306_WHITE);
		if((yvalues[i] < 25)){
			i = 20;
		} 
	}
	
	
	/* for(int i = 0; i < 20; i++){			//starting position is 17,55
		SSD1306_DrawBMP(xcursor, ycursor, cannonball, 0, SSD1306_WHITE);
		counter += yrate;
		if((counter/20)>0){
			ycursor-= ((counter+10)/20);
			counter = 0;
		}
		xcursor++;
		if(ycursor < 20){
			i = 20;
		}
	}*/
	
}
	void drawArrowRight (uint32_t data){
	uint32_t slope = tangent(data);
	int32_t xvalues[20];
	for(int i = 0; i < 20; i++){
		xvalues[i] = 107-i;
	}
	int32_t yvalues[20];
	for(int i = 0; i < 20; i++){
		yvalues[i] = 51-(((slope*((i-1)+1))+50)/100);
	}
	
	for(int i = 0; i < 20; i++){
		SSD1306_DrawBMP(xvalues[i], yvalues[i], cannonball, 0, SSD1306_WHITE);
		if((yvalues[i] < 25)){
			i = 20;
		} 
	}
}

float sine(float angle){
    //input angle in degrees
    //comvert to radians, then convert back to degrees
    angle *= 0.0174533;
    angle = angle - ((angle*angle*angle)/6)
    + ((angle*angle*angle*angle*angle)/120) - ((angle*angle*angle*angle*angle*angle*angle)/5040) + ((angle*angle*angle*angle*angle*angle*angle*angle*angle)/362880);
    return angle;
}

float cosine(float angle){
	//input angle in degrees
	//comvert to radians, then convert back to degrees
		angle *= 0.0174533;
	angle = 1 - ((angle*angle)/2) + ((angle*angle*angle*angle)/24) - ((angle*angle*angle*angle*angle*angle)/720) + ((angle*angle*angle*angle*angle*angle*angle*angle)/40320);
	return angle;
}

int32_t DistSquare(int32_t x, int32_t y, int32_t x_2, int32_t y_2){
	return ((x-x_2)*(x-x_2) + (y-y_2)*(y-y_2));
}

const int g = 1; //gravity
int addWind = 0; //wind to be added when needed
sprite_t ball;
sprite_t shipLeft;
sprite_t shipRight;
sprite_t mountain;
uint8_t NeedToDraw = 0;
uint32_t angle;
uint32_t power;
int32_t wind; 
uint8_t angleSet; //0 for not set, 1 for set
uint8_t setBall = 0;
uint8_t powerSet; //0 for not set, 1 for set
uint8_t collision; //0 for no collision, 1 for yes collision
uint8_t playerturn; //player 1 turn = 1, player 2 turn = 2
int button;
uint32_t random;
int32_t random2 = 0;
uint32_t random3;
int32_t random4;
uint8_t timer = 0;
int8_t addThing = -1;
uint8_t counter1 = 0; //used for wind
uint8_t counter2 = 0; //used for wind
uint8_t windFlag; //used for when to add wind
int main(void){
	//initialize sprites
	//cannonball
		angle = 45;
		power = 0;
		ball.x = 18;
		ball.y = 51;
		//ball.iv = 10;
		//ball.vy = -1 * (ball.iv * sine(angle));
		//ball.vx = ball.iv * cosine(angle);
		ball.image = cannonball;
	//ships
	shipLeft.x = 0;
	shipLeft.y = 59;
	shipLeft.minDist1 = 9;
	shipLeft.center1X = shipLeft.x + shipLeft.minDist1;
	shipLeft.center1Y = shipLeft.y;
	shipLeft.image = ship_left;
	shipRight.x = 109;
	shipRight.y = 59;
	shipRight.minDist1 = 9;
	shipRight.center1X = shipRight.x + shipRight.minDist1;
	shipRight.center1Y = shipRight.y;
	shipRight.image = ship_right;
	//mountain
	mountain.x = 5;
	mountain.y = 63;
	//left center
	mountain.minDist1 = 7;
	mountain.center1X = mountain.x + 47;
	mountain.center1Y = 57;
	//middle center
	mountain.minDist2 = 8;
	mountain.center2X = mountain.x + 58;
	mountain.center2Y = 48;
	//right center
	mountain.minDist3 = 7;
	mountain.center3X = mountain.x + 68;
	mountain.center3Y = 58;
	mountain.image = mountain3;
	
  DisableInterrupts();
  // pick one of the following three lines, all three set to 80 MHz
  //PLL_Init();                   // 1) call to have no TExaS debugging
  TExaS_Init(&LogicAnalyzerTask); // 2) call to activate logic analyzer
  //TExaS_Init(&ScopeTask);       // or 3) call to activate analog scope PD2
  SSD1306_Init(SSD1306_SWITCHCAPVCC);
  SSD1306_OutClear();   
  //Random_Init(1);
  Profile_Init(); // PB5,PB4,PF3,PF2,PF1 
	Switch_Init();
  SSD1306_ClearBuffer();
	
	//initialize semaphores, set the layout
	angleSet = 0;
	powerSet = 0;
	collision = 0;
	playerturn = 1;
	windFlag = 0;
	SSD1306_DrawBMP(mountain.x, mountain.y, mountain.image, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipLeft.x, shipLeft.y, shipLeft.image, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipRight.x, shipRight.y, shipRight.image, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(0, 63, water, 0, SSD1306_WHITE);
	//SSD1306_DrawBMP(ball.x, ball.y, ball.image, 0, SSD1306_WHITE);
	SSD1306_OutBuffer();
	
  EnableInterrupts();
	TitleScreen();
	LanguageSelect();
	SysTick_Init(8000000); //replacing delay100ms with systick, 80000 = 1ms
	ADC_Init();
	Random_Init(ADC_In());
	Sound_Init();
	random = Random10(); //unsigned
	random2 += random; //signed
	random3 = Random1(); //unsigned
	if(random3 == 0){
		random4 = 1;
	}else if(random3 == 1){
		random4 = -1;
	}
	wind = random2 * random4;
	
	
  while(1){
		//testing projectile motion
		if(NeedToDraw){ 
			if(playerturn == 1){
				if((angleSet != 1) || (powerSet != 1)){
					Draw1();
				}else{
					Draw2();
				}
				if((player1score > 4) || (player2score > 4)){
				pauseFlag = 1;
				endGame();
				}
			}
			if(playerturn == 2){
				if((angleSet != 1) || (powerSet != 1)){
					Draw3();
				}else{
					Draw2();
				}
				if((player1score > 4) || (player2score > 4)){
				pauseFlag = 1;
				endGame();
				}
			}
			
		}
		
		//switching turns
		if((collision == 1)&&(playerturn == 1)){ 
			Player2Turn();
			playerturn = 2;
			
		}
		if((collision == 1)&&(playerturn == 2)){ 
			Player1Turn();
			playerturn = 1;
			
			//changing wind speed
			random = Random10(); //unsigned
			random2 = 0;
			random2 += random; //signed
			random3 = Random1(); //unsigned
			if(random3 == 0){
				random4 = 1;
			}else if(random3 == 1){
				random4 = -1;
			}
			wind = random2 * random4;
			
			//setting wind counter
			if(wind<0){
				uint8_t w = wind*-1;
				counter1 = windArr[w];
				counter2 = counter1;
				addWind = -2;
			}else if(wind == 0){
				uint8_t w = wind;
				counter1 = windArr[w];
				counter2 = counter1;
				addWind = 0;
			}else if(wind>0){
				uint8_t w = wind;
				counter1 = windArr[w];
				counter2 = counter1;
				addWind = 2;
			}
			
		}
		

	if(angleSet != 1){
		angle = ConvertAngle(ADC_In());
		/*
		SSD1306_SetCursor(9,2);
		SSD1306_OutUDec2(angle);
		*/
	}else if(powerSet != 1){
		power = ConvertVelocity(ADC_In());
		/*
		SSD1306_SetCursor(9,2);
		SSD1306_OutUDec2(power);
		*/
	}
		if(PlayIn()){
			pauseFlag = 1;
			pauseGame();
		}
  }
}

void Player1Turn(void){
	//resets semaphores, ball position
	//allows angle, power to be ready to set again for P1 turn again
	angleSet = 0;
	powerSet = 0;
	collision = 0;
	ball.x = 18;
	ball.y = 51;
	
}

void Player2Turn(void){
	//resets semaphores, ball position
	//allows angle, power to be ready to set again for P1 turn again
	angleSet = 0;
	powerSet = 0;
	collision = 0;
	ball.x = 107;
	ball.y = 51;
}

void Draw1(void){
	SSD1306_ClearBuffer();
	SSD1306_DrawBMP(5, 63, mountain3, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipLeft.x, shipLeft.y, ship_left, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipRight.x, shipRight.y, ship_right, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(0, 63, water, 0, SSD1306_WHITE);
	//if(angleSet != 1){
	 drawArrowLeft(angle);
		if(language == 1){
			SSD1306_DrawString(0, 8, "P1: ", SSD1306_WHITE);
			SSD1306_DrawChar(18, 8, player1score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(102, 8, "P2: ", SSD1306_WHITE);
			SSD1306_DrawChar(120, 8, player2score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(35, 8, "Angle: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 8, (angle/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 8, (angle%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(35, 16, "Power: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 16, (power/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 16, (power%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(50, 24, "Wind: ", SSD1306_WHITE);
			if(wind<0){ //west
				uint32_t display = wind*-1;
				SSD1306_DrawChar(80, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind==0){ //no wind
				//uint32_t display = 0;
				SSD1306_DrawChar(80, 24, 0x30, SSD1306_WHITE);			//first digit
				//SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				//SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind>0){ //east
				uint32_t display = wind;
				SSD1306_DrawChar(80, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(92, 24, "E", SSD1306_WHITE);
			}
		}
		else if (language == 0){
			SSD1306_DrawString(0, 8, "J1: ", SSD1306_WHITE);
			SSD1306_DrawChar(18, 8, player1score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(102, 8, "J2: ", SSD1306_WHITE);
			SSD1306_DrawChar(120, 8, player2score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(29, 8, "Angulo: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 8, (angle/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 8, (angle%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(29, 16, "Fuerza: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 16, (power/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 16, (power%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(50, 24, "Viento: ", SSD1306_WHITE);
			if(wind<0){ //west
				uint32_t display = wind*-1;
				SSD1306_DrawChar(90, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(96, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(102, 24, "O", SSD1306_WHITE);
			}
			if(wind==0){ //no wind
				//uint32_t display = 0;
				SSD1306_DrawChar(80, 24, 0x30, SSD1306_WHITE);			//first digit
				//SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				//SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind>0){ //east
				uint32_t display = wind;
				SSD1306_DrawChar(90, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(96, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(102, 24, "E", SSD1306_WHITE);
			}
		}
	//}
	//drawArrowRight(angle);
	//SSD1306_DrawBMP(ball.x, ball.y, ball.image, 0, SSD1306_WHITE);
	SSD1306_OutBuffer();
	
	NeedToDraw = 0;
}

void Draw2(void){
	SSD1306_ClearBuffer();
	SSD1306_DrawBMP(5, 63, mountain3, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipLeft.x, shipLeft.y, ship_left, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipRight.x, shipRight.y, ship_right, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(0, 63, water, 0, SSD1306_WHITE);
	
	//drawArrowLeft(angle);
	
	//drawArrowRight(angle);
	SSD1306_DrawBMP(ball.x, ball.y, ball.image, 0, SSD1306_WHITE);
	SSD1306_OutBuffer();
	
	NeedToDraw = 0;
}

void Draw3(void){
	SSD1306_ClearBuffer();
	SSD1306_DrawBMP(5, 63, mountain3, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipLeft.x, shipLeft.y, ship_left, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(shipRight.x, shipRight.y, ship_right, 0, SSD1306_WHITE);
	SSD1306_DrawBMP(0, 63, water, 0, SSD1306_WHITE);
	
	//drawArrowLeft(angle);
	
	drawArrowRight(angle);
		if(language == 1){
			SSD1306_DrawString(0, 8, "P1: ", SSD1306_WHITE);
			SSD1306_DrawChar(18, 8, player1score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(102, 8, "P2: ", SSD1306_WHITE);
			SSD1306_DrawChar(120, 8, player2score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(35, 8, "Angle: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 8, (angle/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 8, (angle%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(35, 16, "Power: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 16, (power/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 16, (power%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(50, 24, "Wind: ", SSD1306_WHITE);
			if(wind<0){ //west
				uint32_t display = wind*-1;
				SSD1306_DrawChar(80, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind==0){ //no wind
				//uint32_t display = 0;
				SSD1306_DrawChar(80, 24, 0x30, SSD1306_WHITE);			//first digit
				//SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				//SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind>0){ //east
				uint32_t display = wind;
				SSD1306_DrawChar(80, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(92, 24, "E", SSD1306_WHITE);
			}
		}
		else if (language == 0){
			SSD1306_DrawString(0, 8, "J1: ", SSD1306_WHITE);
			SSD1306_DrawChar(18, 8, player1score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(102, 8, "J2: ", SSD1306_WHITE);
			SSD1306_DrawChar(120, 8, player2score + 0x30, SSD1306_WHITE);
			SSD1306_DrawString(29, 8, "Angulo: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 8, (angle/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 8, (angle%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(29, 16, "Fuerza: ", SSD1306_WHITE);
			SSD1306_DrawChar(71, 16, (power/10) + 0x30, SSD1306_WHITE);			//first digit
			SSD1306_DrawChar(77, 16, (power%10) + 0x30, SSD1306_WHITE);			//second digit
			SSD1306_DrawString(50, 24, "Viento: ", SSD1306_WHITE);
			if(wind<0){ //west
				uint32_t display = wind*-1;
				SSD1306_DrawChar(90, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(96, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(102, 24, "O", SSD1306_WHITE);
			}
			if(wind==0){ //no wind
				//uint32_t display = 0;
				SSD1306_DrawChar(80, 24, 0x30, SSD1306_WHITE);			//first digit
				//SSD1306_DrawChar(86, 24, (display%10) + 0x30, SSD1306_WHITE);
				//SSD1306_DrawString(92, 24, "W", SSD1306_WHITE);
			}
			if(wind>0){ //east
				uint32_t display = wind;
				SSD1306_DrawChar(90, 24, (display/10) + 0x30, SSD1306_WHITE);			//first digit
				SSD1306_DrawChar(96, 24, (display%10) + 0x30, SSD1306_WHITE);
				SSD1306_DrawString(102, 24, "E", SSD1306_WHITE);
			}
		}

	SSD1306_DrawBMP(ball.x, ball.y, ball.image, 0, SSD1306_WHITE);
	SSD1306_OutBuffer();
	
	NeedToDraw = 0;
}

void BallMove(void){
	
	//if(ball.y > 61){
	//		ball.y = 61;
	//}
	//if(ball.x > 125){
	//		ball.x = 125;
	//}else{
	if((collision != 1)){
		if((ball.y + ball.vy) > 59)
			ball.y = 60;
		else{
			ball.y += ball.vy;
		}
		if(windFlag == 1){
			ball.vx += addWind;
			windFlag = 0;
		}
			ball.x += ball.vx;
			ball.vy += g;
	}
	
	NeedToDraw = 1;
}

void CollisionComp(void){
	// this is for launching from shipLeft at shipRight
	//check for shipLeft collision
	//int32_t distance = DistSquare(3, 5, 7, 9);
	
	//player 1
	if(playerturn == 1){
		if((DistSquare(ball.x, ball.y, shipRight.center1X, shipRight.center1Y)) < (shipRight.minDist1*shipRight.minDist1)){
			player1score++;
			Sound_Explosion();
			collision = 1;
			//check mountain collision
		}else if((DistSquare(ball.x, ball.y, mountain.center1X, mountain.center1Y)) < (mountain.minDist1*mountain.minDist1)){
			collision = 1;
		}else if((DistSquare(ball.x, ball.y, mountain.center2X, mountain.center2Y)) < (mountain.minDist2*mountain.minDist2)){
			collision = 1;
		}else if((DistSquare(ball.x, ball.y, mountain.center3X, mountain.center3Y)) < (mountain.minDist3*mountain.minDist3)){
			collision = 1;		
			//check if goes out of bounds or hits ground collision
		}else if((ball.x>124) || (ball.y>59)){
			collision = 1;
		}
	}
	
	//player 2
	if(playerturn == 2){
		if((DistSquare(ball.x, ball.y, shipLeft.center1X, shipLeft.center1Y)) < (shipLeft.minDist1*shipLeft.minDist1)){
			player2score++;
			Sound_Explosion();
			collision = 1;
			//check mountain collision
		}else if((DistSquare(ball.x, ball.y, mountain.center1X, mountain.center1Y)) < (mountain.minDist1*mountain.minDist1)){
			collision = 1;
		}else if((DistSquare(ball.x, ball.y, mountain.center2X, mountain.center2Y)) < (mountain.minDist2*mountain.minDist2)){
			collision = 1;
		}else if((DistSquare(ball.x, ball.y, mountain.center3X, mountain.center3Y)) < (mountain.minDist3*mountain.minDist3)){
			collision = 1;		
			//check if goes out of bounds or hits ground collision
		}else if((ball.x<3) || (ball.y>59)){
			collision = 1;
		}
	}
}

void AngleMove(void){
	angle = ConvertAngle(ADC_In());
	
	NeedToDraw = 1;
}

void PowerMove(void){
	power = ConvertVelocity(ADC_In());
	
	NeedToDraw = 1;
}


void SysTick_Handler(void){ // every 100 ms
/*
	//for moving ships side to side every 2 seconds
	if(timer == 20){
		addThing *= -1;
		shipLeft.x += addThing;
		shipRight.x -= addThing;
		timer = 0;
	}else{
		timer++;
	}
	*/
	//for moving ships up and down every 2 seconds
	
	//for wind counter
	if(counter1 == 0){
		windFlag = 1;
		counter1 = counter2;
	}else{
		counter1--;
	}
	
	if(timer == 20){
		addThing *= -1;
		shipLeft.y += addThing;
		shipRight.y += addThing;
		timer = 0;
	}else{
		timer++;
	}

	button = FireIn();
	if((button > 0) && (angleSet != 1)){
		angleSet = 1;
		
	}else if((button > 0) && (angleSet == 1)){
		powerSet = 1;
		setBall = 1;
		Sound_Shoot();
	}
	
	if((setBall == 1) && (playerturn == 1)){
		ball.iv = power;
		ball.vy = -1 * (ball.iv * sine(angle));
		ball.vx = ball.iv * cosine(angle);
		setBall = 0;
	}
	
	if((setBall == 1) &&(playerturn == 2)){
		ball.iv = power;
		ball.vy = -1 * (ball.iv * sine(angle));
		ball.vx = -1*(ball.iv * cosine(angle));
		setBall = 0;
	}
	
	if(angleSet != 1){
		AngleMove();
	}else if(powerSet != 1){
		PowerMove();
	}else if(collision != 1){
		CollisionComp();
		BallMove();
	}else{
		BallMove();
	}
	
  //BallMove();
	PF1 ^= 0x02;
}

// You can't use this timer, it is here for starter code only 
// you must use interrupts to perform delays
void Delay100ms(uint32_t count){uint32_t volatile time;
  while(count>0){
    time = 727240;  // 0.1sec at 80 MHz
    while(time){
	  	time--;
    }
    count--;
  }
}
