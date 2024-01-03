
//#include <stdio.h>
//#include "platform.h"
#include "xil_printf.h"

//各种初始化
//设置MIO引脚地址
#define MIO_PIN_07		(*(volatile unsigned int *)0xF800071C)
#define MIO_PIN_50		(*(volatile unsigned int *)0xF80007C8)
#define MIO_PIN_51		(*(volatile unsigned int *)0xF80007CC)

//设置GPIO端口方向寄存器地址
#define DIRM_0			(*(volatile unsigned int *)0xE000A204)
#define DIRM_1			(*(volatile unsigned int *)0xE000A244)
#define DIRM_2			(*(volatile unsigned int *)0xE000A284)
#define DIRM_3			(*(volatile unsigned int *)0xE000A2C4)
//设置GPIO端口输出使能寄存器地址
#define OEN_0			(*(volatile unsigned int *)0xE000A208)
#define OEN_1			(*(volatile unsigned int *)0xE000A248)
#define OEN_2			(*(volatile unsigned int *)0xE000A288)
#define OEN_3			(*(volatile unsigned int *)0xE000A2C8)
//设置GPIO端口输出寄存器地址
#define DATA_0			(*(volatile unsigned int *)0xE000A040)
#define DATA_1			(*(volatile unsigned int *)0xE000A044)
#define DATA_2			(*(volatile unsigned int *)0xE000A048)
#define DATA_3			(*(volatile unsigned int *)0xE000A04C)
//设置GPIO端口输入寄存器地址
#define DATA_0_RO		(*(volatile unsigned int *)0xE000A060)
#define DATA_1_RO		(*(volatile unsigned int *)0xE000A064)
#define DATA_2_RO		(*(volatile unsigned int *)0xE000A068)
#define DATA_3_RO		(*(volatile unsigned int *)0xE000A06C)

//设置UART1引脚地址的宏定义
#define rMIO_PIN_48		(*(volatile unsigned long*)0xF80007C0)
#define rMIO_PIN_49 	(*(volatile unsigned long*)0xF80007C4)
#define rUART_CLK_CTRL 	(*(volatile unsigned long*)0xF8000154)
#define rControl_reg0 	(*(volatile unsigned long*)0xE0001000)
#define rMode_reg0 		(*(volatile unsigned long*)0xE0001004)
//设置 UART1端口波特率等参数地址寄存器的宏定义
#define rBaud_rate_gen_reg0     (*(volatile unsigned long*)0xE0001018)
#define rBaud_rate_divider_reg0 (*(volatile unsigned long*)0xE0001034)
#define rTx_Rx_FIFO0            (*(volatile unsigned long*)0xE0001030)
#define rChannel_sts_reg0       (*(volatile unsigned long*)0xE000102C)

//各种功能函数
void arm(int Arm_id,int Arm_dir);                       //机械臂
void box(void);                                         //箱子
void plate(int Plate_ID,int Plate_dir);                 //吸盘
void trans(int Trans_dir);                              //传送带
void auto_ctl(void);                                    //自动模式
void displayOnLED(int num);						//展示在LED上

void send_Char_9(unsigned char modbus[]);				//9字节串口发送函数
void send_Char(unsigned char data);						//单字节串口发送函数
void RS232_Init();										//串口初始化函数

void delay(int i,int n,int m);							//延时函数

int step_count = 0;
unsigned char reverse_opt[200][9];
int grabCount = 5;

int main(){
	u32 flag;		//变量flag记录SW0~SW7按键的信息
	//注：下面MIO引脚和EMIO引脚的序号是统一编号的，MIO序号为0~31及32~53，EMIO序号为54~85及86~117
	//配置及初始化MIO07引脚的相关寄存器，MIO07作为LED灯控制的输出引脚
	MIO_PIN_07 = 0x00003600;
	DIRM_0 = DIRM_0|0x00000080;
	OEN_0 = OEN_0|0x00000080;
	//配置及初始化MIO50、MIO51引脚的相关寄存器，MIO50、MIO51作为按键输入引脚
	MIO_PIN_50 = 0x00003600;
	MIO_PIN_51 = 0x00003600;
	DIRM_1 = DIRM_1 & 0xFFF3FFFF;
	//初始化EMIO54~EMIO58的引脚，它们对应BTNU、BTND、BTNL、BTNR、BTNC按键，输入
	DIRM_2 = DIRM_2 & 0xFFFFFFE0;
	//初始化EMIO59~EMIO66的引脚，它们对应SW7~SW0拨动开关，输入
	DIRM_2 = DIRM_2 & 0xFFFFE01F;
	//初始化EMIO67~EMIO74的引脚，它们对应LED7~LED0，输出
	DIRM_2 = DIRM_2|0x001FE000;
	OEN_2 = OEN_2|0x001FE000;

	//初始化UART1
	RS232_Init();

	//记录操作信息的反操作

	int i = 0;
	int j = 0;
	for(i = 0;i<200;i++) reverse_opt[i][0] = '#';
	for(i = 0;i<200;i++) reverse_opt[i][1] = '1';
	for(i = 0;i<200;i++){
		for(j = 2;j<9;j++){
			reverse_opt[i][j] = '0';
		}
	}
	step_count = 0;						//仅记录机械臂相关（包括轨道）的操作数



    while(1) {
        //读取SW7、SW6的输入信息
        flag = DATA_2_RO & 0x00000060;
        //根据SW[7:6]选择处理的状态，其中：00复位，10手动，01自动，11示教
        switch (flag) {
            case 0x00:                            //复位模式
                DATA_2 = DATA_2 & 0xFFE01FFF;        //指示灯LED7~LED0全灭
                u32 flag_button = DATA_2_RO & 0x0000001F;       //取BTNU,BTND,BTNL,BTNR,BTNC按键
                if (flag_button == 0x00000010) {                //判断BTNC按键是否按下
                    DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                    delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                    flag_button = DATA_2_RO & 0x0000001F;
                    while (flag_button == 0x00000010) {            //判断BTNC按键是否抬起
                        flag_button = DATA_2_RO & 0x0000001F;
                    }
                    DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                    //放箱子
                    if (step_count) {
                        step_count--;
                        int k, m;
                        unsigned char test[9];
                        for (k = step_count; k >= 0; k--) {
                            for (m = 0; m < 9; m++) test[m] = reverse_opt[k][m];
                            send_Char_9(test);
                        }
                        step_count = 0;
                        for (i = 0; i < 200; i++) reverse_opt[i][0] = '#';
                        for (i = 0; i < 200; i++) reverse_opt[i][1] = '1';
                        for (i = 0; i < 200; i++) {
                            for (j = 2; j < 9; j++) {
                                reverse_opt[i][j] = '0';
                            }
                        }
                    }
                }
                break;

            case 0x20:                                            //手动控制模式
                DATA_2 = (DATA_2 | 0x00002000) & 0xFFFFBFFF;        //控制模式指示灯LED7亮、LED6灭
                // SW[2:5]= 0-5时，对应1-6号机械臂，0110导轨，0111吸盘，1000箱子，1001传送带
                // DATA_2_RO的8-11位，低8对应2，9对应3...
                flag = DATA_2_RO & 0x00000780;                  //取SW2,3,4,5来决定是哪一种模式，机械臂/轨道/吸盘/箱子/传送带
                // 低5位分别对应按键BTNU、BTND、BTNL、BTNR、BTNC
                flag_button = DATA_2_RO & 0x0000001F;       //取BTNU,BTND,BTNL,BTNR,BTNC按键
                switch (flag) {
                    case 0x000:       //第一个轴
                        //DATA_2[14]=1，仅LED7亮
                        DATA_2 = (DATA_2 | 0x00002000) & 0x00002FFF;
                        if (flag_button == 0x00000004) {            //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {    //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //BTNL抬起，LED9指示灯灭
                            //机械臂顺时针，转速角度为3
                            arm(1, 0);
                            reverse_opt[step_count][2] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {    //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {    //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //机械臂逆时针，转速角度为3
                            arm(1, 1);
                            reverse_opt[step_count][2] = '3';
                            step_count++;
                        }
                        break;
                        //
                    case 0x080:
                        //第二个轴
                        //LED7、5亮
                        DATA_2 = (DATA_2 | 0x0000A000) & 0x0000AFFF;
                        if (flag_button == 0x00000004) {            //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {    //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            arm(2, 0);
                            reverse_opt[step_count][3] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {    //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {    //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            arm(2, 1);
                            reverse_opt[step_count][3] = '3';
                            step_count++;
                        }
                        break;

                    case 0x100:
                        //第三个轴
                        //点灯
                        DATA_2 = (DATA_2 | 0x00012000) & 0x00012FFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            arm(3, 0);
                            reverse_opt[step_count][4] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            arm(3, 1);
                            reverse_opt[step_count][4] = '3';
                            step_count++;
                        }
                        break;
                        //
                    case 0x180:
                        //第四个轴
                        //点灯
                        DATA_2 = (DATA_2 | 0x0001A000) & 0x0001AFFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            arm(4, 0);
                            reverse_opt[step_count][5] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = (DATA_0 | 0x00000080) & 0x00008FFF;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            arm(4, 1);
                            reverse_opt[step_count][5] = '3';
                            step_count++;
                        }
                        break;
                        //
                    case 0x200:
                        //第五个轴
                        //点灯
                        DATA_2 = (DATA_2 | 0x00022000) & 0x00022FFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            arm(5, 0);
                            reverse_opt[step_count][6] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            arm(5, 1);
                            reverse_opt[step_count][6] = '3';
                            step_count++;
                        }
                        break;
                        //
                    case 0x280:
                        //第六个轴
                        //点灯
                        DATA_2 = (DATA_2 | 0x0002A000) & 0x0002AFFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            arm(6, 0);
                            reverse_opt[step_count][7] = '7';
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            arm(6, 1);
                            reverse_opt[step_count][7] = '3';
                            step_count++;
                        }
                        break;
                        //
                    case 0x300:
                        //轨道移动
                        //点灯
                        DATA_2 = (DATA_2 | 0x00032000) & 0x00032FFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //左移，2档
                            arm(7, 0);
                            reverse_opt[step_count][8] = '5';
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //右移，2档
                            arm(7, 1);
                            reverse_opt[step_count][8] = '2';
                            step_count++;
                        }
                        break;
                        //
                    case 0x380:
                        //吸盘
                        //点灯
                        DATA_2 = (DATA_2 | 0x0003A000) & 0x0003AFFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            plate(1, 0);
                            step_count++;
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            plate(1, 1);
                            step_count++;
                        }
                        break;
                    case 0x400:
                        //箱子
                        //点灯
                        DATA_2 = (DATA_2 | 0x00042000) & 0x00042FFF;
                        if (flag_button == 0x00000010) {                //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000010) {            //判断BTNC按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //放箱子
                            box();
                        }
                        break;
                        //
                    case 0x480:
                        //传送带
                        //点灯
                        DATA_2 = (DATA_2 | 0x0004A000) & 0x0004AFFF;
                        if (flag_button == 0x00000004) {                //判断BTNL按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴顺时针，转速角度为3
                            trans(0);
                        } else if (flag_button == 0x00000008) {            //判断BTNR按键是否按下
                            DATA_0 = DATA_0 | 0x00000080;        //LED9指示灯亮
                            delay(1000, 500, 50);                    //延时约1秒，进行消抖动处理
                            flag_button = DATA_2_RO & 0x0000001F;
                            while (flag_button == 0x00000008) {            //判断BTNR按键是否抬起
                                flag_button = DATA_2_RO & 0x0000001F;
                            }
                            DATA_0 = DATA_0 & 0xFFFFFF7F;        //LED9指示灯灭
                            //轴逆时针，转速角度为3
                            trans(1);
                        }
                        break;
                        //
                }
                break;
            case 0x40:                    //自动模式
            	//先亮灯
                DATA_2 = (DATA_2 | 0x00004000) & 0xFFFF7FFF;    //LED7灭、LED6亮
                //取操作：1111-设置模式，其他：自动
                // DATA_2_RO的8-11位，低8对应2，9对应3...
                flag = DATA_2_RO & 0x00000780;                  //取SW2,3,4,5来决定模式
                if(flag == 0x780){								//sw[2:5]=1111
                	// 低5位分别对应按键BTNU、BTND、BTNL、BTNR、BTNC
                	//全亮
                	DATA_2 = (DATA_2 | 0x0007A000) & 0x0007AFFF;
                	flag_button = DATA_2_RO & 0x0000001F;   //依次取：BTNU、BTND、BTNL、BTNR、BTNC
                	if (flag_button == 0x00000004) {           // BTNL按下，则自动模式
                		DATA_0 = DATA_0 | 0x00000080;        //LED9灯亮
                	    delay(1000, 500, 50);                    //延时1秒消抖动处理
                	    flag_button = DATA_2_RO & 0x0000001F;
                	    while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                	    	flag_button = DATA_2_RO & 0x0000001F;
                	    }
                	    DATA_0 = DATA_0 & 0xFFFFFF7F;        //按键松，LED9灭
                	    //自动模式
                	    auto_ctl();
                	 } else if (flag_button == 0x00000008) {
                		 //BTNC按下，实现扩充功能：手动设定抓物次数，
                	     //设置次数时，次数需要在LED上显示，然后根据
                	     //设定次数，自动完成相应次数的物体搬运。搬运的
                	     //次数需显示在LED上。
                	     DATA_0 = DATA_0 | 0x00000080;        //LED9灯亮
                	     delay(1000, 500, 50);                    //延时1秒消抖动处理
                	     flag_button = DATA_2_RO & 0x0000001F;
                	     while (flag_button == 0x00000008) {            //判断BTNL按键是否抬起
                	    	 flag_button = DATA_2_RO & 0x0000001F;
                	     }
                	     DATA_0 = DATA_0 & 0xFFFFFF7F;        //按键松，LED9灭
                	     // 在LED上显示设定的抓物次数
                	     displayOnLED(grabCount);
                	     // 执行搬运动作
                	     for (int i = grabCount; i > 0; --i) {
                	    	 delay(2952, 500, 50);                    //延时1秒消抖动处理
                	     // 自动搬运
                	         auto_ctl();
                	     // 在LED上显示搬运次数
                	         displayOnLED(i - 1);
                	      }
                	 }//num增减操作
                	 else if (flag_button == 0x00000001){			//BTNU ++
                		DATA_0 = DATA_0 | 0x00000080;		//LED9指示灯亮
                	    delay(1000,500,50);					//延时约1秒，进行消抖动处理
                	    flag_button = DATA_2_RO & 0x0000001F;
                	    while(flag_button == 0x00000001){		//判断BTN	u按键是否抬起
                	    	flag_button = DATA_2_RO & 0x0000001F;
                	    }
                	    DATA_0 = DATA_0 & 0xFFFFFF7F;
                	    grabCount++;
                	    displayOnLED(grabCount);
                	}else if (flag_button == 0x00000002){	//BTND --
                	    DATA_0 = DATA_0 | 0x00000080;		//LED9指示灯亮
                	    delay(1000,500,50);					//延时约1秒，进行消抖动处理
                	    flag_button = DATA_2_RO & 0x0000001F;
                	    while(flag_button == 0x00000002){			//判断BTNd按键是否抬起
                	    	flag_button = DATA_2_RO & 0x0000001F;
                	    }
                	    DATA_0 = DATA_0 & 0xFFFFFF7F;
                	    grabCount--;
                	    displayOnLED(grabCount);
                	}
                }else{
                	// 低5位分别对应按键BTNU、BTND、BTNL、BTNR、BTNC
                	flag_button = DATA_2_RO & 0x0000001F;   //依次取：BTNU、BTND、BTNL、BTNR、BTNC
                	if (flag_button == 0x00000004) {           // BTNL按下，则自动模式
                		DATA_0 = DATA_0 | 0x00000080;        //LED9灯亮
                	    delay(1000, 500, 50);                    //延时1秒消抖动处理
                	    flag_button = DATA_2_RO & 0x0000001F;
                	    while (flag_button == 0x00000004) {            //判断BTNL按键是否抬起
                	    	flag_button = DATA_2_RO & 0x0000001F;
                	    }
                	    DATA_0 = DATA_0 & 0xFFFFFF7F;        //按键松，LED9灭
                	    //自动模式
                	    auto_ctl();
                	} else if (flag_button == 0x00000008) {
                	  //BTNC按下，实现扩充功能：手动设定抓物次数，
                	  //设置次数时，次数需要在LED上显示，然后根据
                	  //设定次数，自动完成相应次数的物体搬运。搬运的
                	  //次数需显示在LED上。
                		DATA_0 = DATA_0 | 0x00000080;        //LED9灯亮
                	    delay(1000, 500, 50);                    //延时1秒消抖动处理
                	    flag_button = DATA_2_RO & 0x0000001F;
                	    while (flag_button == 0x00000008) {            //判断BTNL按键是否抬起
                	    	flag_button = DATA_2_RO & 0x0000001F;
                	    }
                	    DATA_0 = DATA_0 & 0xFFFFFF7F;        //按键松，LED9灭
                	    // 手动设定抓物次数，例如设定为5次
                	    grabCount = 5;
                	    // 在LED上显示设定的抓物次数
                	    displayOnLED(grabCount);
                	    // 执行搬运动作
                	    for (int i = grabCount; i > 0; --i) {
                	    	delay(2952, 500, 50);                    //延时1秒消抖动处理
                	        // 自动搬运
                	        auto_ctl();
                	        // 在LED上显示搬运次数
                	        displayOnLED(i - 1);
                	    }
                	}
                    break;
                    case 0x60:                    //机械臂示教模式（该模式暂不实现）
                        DATA_2 = DATA_2 | 0x00006000;                    //LED7亮、LED6亮
                    break;
                }
        }
    }
    return 0;
}

//机械臂相关各部件动作函数
void arm(int Arm_ID,int Arm_dir)
{
	unsigned char modbus_com[9];
	modbus_com[0]='#';				//起始符，固定为#
	modbus_com[1]='1';				//1号机械臂
	modbus_com[2]='0';
	modbus_com[3]='0';
    modbus_com[4]='0';
    modbus_com[5]='0';
    modbus_com[6]='0';
    modbus_com[7]='0';
    modbus_com[8]='0';

	switch(Arm_ID){
	case 1:						//第一个轴
	     if (Arm_dir==0){
		    modbus_com[2]='3';
	     }
	     else if(Arm_dir==1){
	    	modbus_com[2]='7';
	     }
	     break;
	case 2:						//第二个轴
		 if (Arm_dir==0){
		    modbus_com[3]='3';
		 }
		 else if(Arm_dir==1){
		    modbus_com[3]='7';
		 }
	   	 break;
	case 3:						//第三个轴
		 if (Arm_dir==0){
		    modbus_com[4]='3';
		 }
		 else if(Arm_dir==1){
		    modbus_com[4]='7';
		 }
		 break;
	case 4:						//第四个轴
		 if (Arm_dir==0){
		    modbus_com[5]='3';
		 }
		 else if(Arm_dir==1){
		   	modbus_com[5]='7';
		 }
	   	 break;
	case 5:						//第五个轴
		 if (Arm_dir==0){
		    modbus_com[6]='3';
		 }
		 else if(Arm_dir==1){
		    modbus_com[6]='7';
		 }
         break;
	case 6:						//第六个轴
		 if (Arm_dir==0){
		    modbus_com[7]='3';
		 }
		 else if(Arm_dir==1){
		    modbus_com[7]='7';
		 }
		 break;
	case 7:						//轨道上的移动
		 if (Arm_dir==0){
		    modbus_com[8]='2';
		 }
		 else if(Arm_dir==1){
		    modbus_com[8]='5';
		 }
		 break;
	}
	send_Char_9(modbus_com);
}

//箱子相关函数
void box(void)
{
	unsigned char modbus_com[9];
	modbus_com[0]='#';				//起始符，固定为#
	modbus_com[1]='4';				//箱子
	modbus_com[2]='0';
	modbus_com[3]='0';
    modbus_com[4]='0';
    modbus_com[5]='0';
    modbus_com[6]='0';
    modbus_com[7]='0';
    modbus_com[8]='0';
    //1号站
    modbus_com[2] = '1';
	send_Char_9(modbus_com);
}

//吸盘相关函数
void plate(int Plate_ID,int Plate_dir)
{
	unsigned char modbus_com[9];
	modbus_com[0]='#';				//起始符，固定为#
	modbus_com[1]='5';				//吸盘
	modbus_com[2]='0';
	modbus_com[3]='0';
    modbus_com[4]='0';
    modbus_com[5]='0';
    modbus_com[6]='0';
    modbus_com[7]='0';
    modbus_com[8]='0';

	switch(Plate_ID){
	case 1:						//1号站
         if(Plate_dir == 0){
             modbus_com[2]='1';
         }else if(Plate_dir == 1){
             modbus_com[2]='2';
         }
	     break;
	case 2:						//2号站
         if(Plate_dir == 0){
             modbus_com[3]='1';
         }else if(Plate_dir == 1){
             modbus_com[3]='2';
         }
	   	 break;
	case 3:						//3号站
          if(Plate_dir == 0){
             modbus_com[4]='1';
         }else if(Plate_dir == 1){
             modbus_com[4]='2';
         }
	}
	send_Char_9(modbus_com);
}

//传送带相关函数
void trans(int Trans_dir)
{
	unsigned char modbus_com[9];
	modbus_com[0]='#';				//起始符，固定为#
	modbus_com[1]='6';				//传送带
	modbus_com[2]='0';
	modbus_com[3]='0';
    modbus_com[4]='0';
    modbus_com[5]='0';
    modbus_com[6]='0';
    modbus_com[7]='0';
    modbus_com[8]='0';

	//传送带开关
    if(Trans_dir == 0){
        modbus_com[2]='1';
    }else if(Trans_dir == 1){
        modbus_com[2]='2';
    }
	send_Char_9(modbus_com);
}

//自动模式：按启动键（BTNL），系统自动在1号传送带上出物体，然后机械臂抓取物体放到2号传送带上
void auto_ctl(void){

   delay(100,500,50);    //延时消抖
   box();                 //生成箱子
   delay(2952,500,50);    //延时消抖
   trans(0);              //传送带开
   delay(8964,500,50);    //延时消抖
   delay(1000,500,50);    //延时消抖
   //机械臂操作
   for(int i=0;i<30;i++){
     delay(200,500,50);
     arm(3,1);			//3号轴逆时针
//     reverse_opt[step_count][4] = '3';
//	  step_count++;
   }

	for(int i=0;i<10;i++){
      delay(200,500,50);
      arm(2,0);          //2号轴顺时针，转速3
//      reverse_opt[step_count][3] = '7';
//      step_count++;
    }

    for(int i=0;i<50;i++){
      delay(200,500,50);
      arm(5,0);			//5号轴顺时针，
//	  reverse_opt[step_count][6] = '7';
//      step_count++;
    }
    for(int i=0;i<17;i++){
      delay(200,500,50);
      arm(6,1);			//6号轴逆时针，
//      reverse_opt[step_count][7] = '3';
//      step_count++;
   }
    for(int i=0;i<18;i++){
       delay(200,500,50); //延时消抖
       arm(1,0);           //1号轴顺时针，转速为3
//       reverse_opt[step_count][2] = '7';
//       step_count++;
 	}
    for(int i=0;i<1;i++){
          delay(200,500,50);
          arm(2,0);          //2号轴顺时针，转速3
//          reverse_opt[step_count][3] = '7';
//          step_count++;
        }
   delay(500,500,50);            //延时消抖
   //吸进
   plate(1,0);                    //吸
   delay(1000,500,50);            //延时消抖

   //机械臂转
	for(int i=0;i<3;i++){
    delay(200,500,50);
    arm(2,1);          //2号轴逆时针，转速3
//    reverse_opt[step_count][3] = '3';
//    step_count++;
  }
   for(int i=0;i<4;i++){
     delay(200,500,50);
     arm(6,0);			//6号轴顺时针，
//     reverse_opt[step_count][7] = '7';
//     step_count++;
  }
   for(int i=0;i<33;i++){
        delay(200,500,50);
        arm(1,1);                //1号轴逆时针
//		reverse_opt[step_count][2] = '3';
//        step_count++;
   }




   delay(1000,500,50);
   plate(1,1);                //放
   delay(1000,500,50);


   //复位
   for(int i=0;i<13;i++){
          delay(200,500,50);
          arm(6,0);			//6号轴逆时针，
//          reverse_opt[step_count][7] = '3';
//          step_count++;
       }
   for(int i=0;i<50;i++){
      delay(200,500,50);
      arm(5,1);			//5号轴逆时针，
// 	  reverse_opt[step_count][6] = '7';
//      step_count++;
    }
   for(int i=0;i<8;i++){
          delay(200,500,50);
          arm(2,1);          //2号轴逆时针，转速3
//          reverse_opt[step_count][3] = '3';
//          step_count++;
      }
   for(int i=0;i<15;i++){
          delay(200,500,50); //延时消抖
          arm(1,0);           //1号轴顺时针，转速为3
//          reverse_opt[step_count][2] = '7';
//          step_count++;
   }
   for(int i=0;i<30;i++){
         delay(200,500,50);
         arm(3,0);			//3号轴顺时针
//         reverse_opt[step_count][4] = '3';
//    	  step_count++;
       }
   //复位
//   if(step_count){
//        step_count--;
//        int k,m;
//        unsigned char test[9];
//      for(k = step_count; k >= 0; k--){
//           for(m = 0;m<9;m++) test[m] = reverse_opt[k][m];
//           send_Char_9(test);
//       }
//       step_count = 0;
//       for(int i = 0; i<200; i++) reverse_opt[i][0] = '#';
//       for(int i = 0; i<200; i++) reverse_opt[i][1] = '1';
//       for(int i = 0; i<200; i++){
//           for(int j = 2; j<9; j++){
//               reverse_opt[i][j] = '0';
//           }
//       }
//   }
}

// 在LED上显示数字
      void displayOnLED(int num) {
    // 根据实际硬件和显示方式，编写显示数字的代码
    	  unsigned char modbus_com[9];
    	  modbus_com[0]='#';				//起始符，固定为#
    	  modbus_com[1]='7';				//传送带
    	  modbus_com[2]='0';				//命令1
    	  modbus_com[3]='0';
    	  modbus_com[4]='0';
    	  modbus_com[5]='0';
    	  modbus_com[6]='0';
    	  modbus_com[7]='0';
    	  modbus_com[8]='0';
    	  if(num < 100 && num >= 0){
    		  int units = num % 10;
    		  int tens = num / 10;
    		  switch(units){
    		  case 0:
    	    	  modbus_com[2]='0';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='1';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 1:
    	    	  modbus_com[2]='0';				//g
    	    	  modbus_com[3]='0';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='0';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='0';				//a
    			  break;
    		  case 2:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='0';				//f
    	    	  modbus_com[4]='1';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='0';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 3:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='0';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 4:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='0';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='0';				//a
    			  break;
    		  case 5:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='0';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 6:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='1';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='0';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 7:
    	    	  modbus_com[2]='0';				//g
    	    	  modbus_com[3]='0';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='0';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 8:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='1';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  case 9:
    	    	  modbus_com[2]='1';				//g
    	    	  modbus_com[3]='1';				//f
    	    	  modbus_com[4]='0';				//e
    	    	  modbus_com[5]='1';				//d
    	    	  modbus_com[6]='1';				//c
    	    	  modbus_com[7]='1';				//b
    	    	  modbus_com[8]='1';				//a
    			  break;
    		  }
    		  //发送个位数
    		  send_Char_9(modbus_com);
    		  if(tens >= 0){
    			  modbus_com[1] = '8';
        		  switch(tens){
        		  case 0:
        		     modbus_com[2]='0';				//g
        		     modbus_com[3]='1';				//f
        		     modbus_com[4]='1';				//e
        		     modbus_com[5]='1';				//d
        		     modbus_com[6]='1';				//c
        		     modbus_com[7]='1';				//b
        		     modbus_com[8]='1';				//a
        		     break;
        		  case 1:
        	    	  modbus_com[2]='0';				//g
        	    	  modbus_com[3]='0';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='0';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='0';				//a
        			  break;
        		  case 2:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='0';				//f
        	    	  modbus_com[4]='1';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='0';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 3:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='0';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 4:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='1';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='0';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='0';				//a
        			  break;
        		  case 5:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='1';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='0';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 6:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='1';				//f
        	    	  modbus_com[4]='1';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='0';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 7:
        	    	  modbus_com[2]='0';				//g
        	    	  modbus_com[3]='0';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='0';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 8:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='1';				//f
        	    	  modbus_com[4]='1';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  case 9:
        	    	  modbus_com[2]='1';				//g
        	    	  modbus_com[3]='1';				//f
        	    	  modbus_com[4]='0';				//e
        	    	  modbus_com[5]='1';				//d
        	    	  modbus_com[6]='1';				//c
        	    	  modbus_com[7]='1';				//b
        	    	  modbus_com[8]='1';				//a
        			  break;
        		  }
        		  //发送十位数
        		  send_Char_9(modbus_com);
    		  }
    	  }


}
//
//         // 手动设定抓物次数
//         void manualSetGrabCount() {
//             // 根据实际硬件和输入方式，编写手动设定抓物次数的代码
//    // 例如，假设有一个名为 setGrabCountManually 的函数
//grabCount = setGrabCountManually();
//        }

//9个字节数据的发送函数
void send_Char_9(unsigned char modbus[])
{
	int i;
	char data;
	for(i=0;i<9;i++){
		data=modbus[i];
		send_Char(data);
		delay(100,10,10);		//延时
	}
}

//单个字节数据的发送函数
void send_Char(unsigned char data)
{
     while((rChannel_sts_reg0&0x10)==0x10);
     rTx_Rx_FIFO0=data;
}

//UART1的初始化函数
void RS232_Init()
{
     rMIO_PIN_48=0x000026E0;
     rMIO_PIN_49=0x000026E0;
     rUART_CLK_CTRL=0x00001402;
     rControl_reg0=0x00000017;
     rMode_reg0=0x00000020;
     rBaud_rate_gen_reg0=62;
     rBaud_rate_divider_reg0=6;
}

//延时函数
void delay(int n,int m,int p)
{
	 int i,j,k;
	 for(i=1;i<=n;i++){
		 for(j=1;j<=m;j++){
			 for(k=1;k<=p;k++){

			 }
		 }
	 }
}
