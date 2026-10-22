#include <REGX52.H>
#include <intrins.h>

//====================== 引脚定义 ======================
// LCD1602 引脚
sbit LCD_RS = P2^6;
sbit LCD_RW = P2^5;
sbit LCD_E  = P2^7;
#define LCD_DataPort P0

// DS18B20 温度传感器引脚
sbit DS18B20_DQ = P1^0;

// ADC0832 模数转换引脚 (用于读取MQ-2烟雾)
sbit ADC_CS  = P1^1;
sbit ADC_CLK = P1^5;
sbit ADC_DAT = P1^6;

// 报警模块与继电器引脚
sbit BUZZER    = P1^7;
sbit RELAY     = P1^4;
sbit LED_RED   = P2^0;
sbit LED_GREEN = P2^1;

// 按键引脚 
sbit KEY_MODE = P3^1;
sbit KEY_ADD  = P3^2;
sbit KEY_SUB  = P3^3;

//====================== 全局变量 ======================
unsigned char temp_value = 0;  
unsigned char smoke_value = 0; 
unsigned char max_temp = 40;   
unsigned char max_smoke = 50;  
unsigned char set_mode = 0;    

//====================== 延时函数 ======================
void Delay1ms(unsigned int xms)	//@12.000MHz
{
	unsigned char i, j;
	while(xms--)
	{
		i = 2; j = 239;
		do { while (--j); } while (--i);
	}
}

// 【新增】标准10微秒延时
void Delay10us() {
	unsigned char i = 2;
	while (--i);
}

// 【新增】x个10微秒延时
void DelayX10us(unsigned char x) {
    while(x--) {
        Delay10us();
    }
}

//====================== LCD1602 驱动 ======================
void LCD_Delay() { Delay1ms(1); }

void LCD_WriteCommand(unsigned char Command) {
	LCD_RS=0; LCD_RW=0; LCD_DataPort=Command;
	LCD_E=1; LCD_Delay(); LCD_E=0; LCD_Delay();
}
void LCD_WriteData(unsigned char Data) {
	LCD_RS=1; LCD_RW=0; LCD_DataPort=Data;
	LCD_E=1; LCD_Delay(); LCD_E=0; LCD_Delay();
}
void LCD_Init(void) {
	LCD_WriteCommand(0x38); 
	LCD_WriteCommand(0x0C); 
	LCD_WriteCommand(0x06); 
	LCD_WriteCommand(0x01); 
}
void LCD_SetCursor(unsigned char Line,unsigned char Column) {
	if(Line==1) LCD_WriteCommand(0x80|(Column-1));
	else        LCD_WriteCommand(0x80|(Column-1)+0x40);
}
void LCD_ShowChar(unsigned char Line,unsigned char Column,unsigned char Char) {
	LCD_SetCursor(Line,Column); LCD_WriteData(Char);
}
void LCD_ShowString(unsigned char Line,unsigned char Column,unsigned char *String) {
	unsigned char i;
	LCD_SetCursor(Line,Column);
	for(i=0;String[i]!='\0';i++) { LCD_WriteData(String[i]); }
}
void LCD_ShowNum(unsigned char Line,unsigned char Column,unsigned int Number,unsigned char Length) {
	unsigned char i; unsigned int Pow=1;
	LCD_SetCursor(Line,Column);
	for(i=0;i<Length-1;i++) Pow*=10;
	for(i=Length;i>0;i--) {
		LCD_WriteData('0'+Number/Pow%10);
		Pow/=10;
	}
}

//====================== DS18B20 驱动  ======================
unsigned char DS18B20_Init() {
    unsigned char ack;
    DS18B20_DQ = 1;
    DS18B20_DQ = 0;
    DelayX10us(50); // 拉低500us复位
    DS18B20_DQ = 1; 
    DelayX10us(6);  // 等待60us
    ack = DS18B20_DQ; // 读响应
    DelayX10us(40); // 等待400us让响应脉冲结束
    return ack;
}

void DS18B20_WriteByte(unsigned char dat) {
    unsigned char i;
    for(i=0; i<8; i++) {
        DS18B20_DQ = 0;
        _nop_(); _nop_(); // 严格控制拉低时间
        DS18B20_DQ = dat & 0x01;
        DelayX10us(6); // 保持60us让传感器采样
        DS18B20_DQ = 1;
        _nop_(); _nop_();
        dat >>= 1;
    }
}

unsigned char DS18B20_ReadByte() {
    unsigned char i, dat=0;
    for(i=0; i<8; i++) {
        dat >>= 1;
        DS18B20_DQ = 0;
        _nop_(); _nop_(); // 拉低2us启动读槽
        DS18B20_DQ = 1;   // 主机释放总线
        _nop_(); _nop_(); _nop_(); _nop_(); // 等待约4us
        if(DS18B20_DQ) dat |= 0x80; // 读取数据
        DelayX10us(5); // 延时50us结束读槽
    }
    return dat;
}

void DS18B20_ReadTemp() {
    unsigned char L, H;
    DS18B20_Init();
    DS18B20_WriteByte(0xCC);
    DS18B20_WriteByte(0xBE);
    L = DS18B20_ReadByte();
    H = DS18B20_ReadByte();
    temp_value = (H << 4) | (L >> 4); 
    
    DS18B20_Init();
    DS18B20_WriteByte(0xCC);
    DS18B20_WriteByte(0x44); 
}

//====================== ADC0832 驱动  ======================
unsigned char ADC0832_Read() {
    unsigned char i, dat = 0;
    ADC_CS = 0;
    ADC_CLK = 0;
    
    // 1. 发送配置位 (增加延时，让Proteus的芯片模型能反应过来)
    ADC_DAT = 1; Delay10us(); ADC_CLK = 1; Delay10us(); ADC_CLK = 0; Delay10us(); // Start
    ADC_DAT = 1; Delay10us(); ADC_CLK = 1; Delay10us(); ADC_CLK = 0; Delay10us(); // SGL (单端模式)
    ADC_DAT = 0; Delay10us(); ADC_CLK = 1; Delay10us(); ADC_CLK = 0; Delay10us(); // CH0 (选择通道0)

    // 2. 单片机主动拉高引脚，释放总线控制权，准备接收数据
    ADC_DAT = 1; 
    Delay10us(); 

    // 3. 读取8位数据
    for(i=0; i<8; i++) {
        ADC_CLK = 1; Delay10us();
        ADC_CLK = 0; Delay10us(); // ADC0832在时钟下降沿输出数据
        
        dat <<= 1;
        if(ADC_DAT) {
            dat |= 0x01;  // 读取引脚状态
        }
    }
    ADC_CS = 1;
    
    // 强制转换为 unsigned int (16位) 进行计算，防止8位机乘法溢出
    return (unsigned char)(((unsigned int)dat * 99) / 255);
}

//====================== 按键扫描与菜单逻辑 ======================
void Key_Scan() {
    if(KEY_MODE == 0) {
        Delay1ms(20); 
        if(KEY_MODE == 0) {
            set_mode++;
            if(set_mode > 2) set_mode = 0;
            LCD_WriteCommand(0x01); 
            while(KEY_MODE == 0);
        }
    }
    if(KEY_ADD == 0) {
        Delay1ms(20);
        if(KEY_ADD == 0) {
            if(set_mode == 1) { max_temp++; if(max_temp > 99) max_temp = 99; }
            if(set_mode == 2) { max_smoke++; if(max_smoke > 99) max_smoke = 99; }
            while(KEY_ADD == 0);
        }
    }
    if(KEY_SUB == 0) {
        Delay1ms(20);
        if(KEY_SUB == 0) {
            if(set_mode == 1) { max_temp--; if(max_temp < 10) max_temp = 10; }
            if(set_mode == 2) { max_smoke--; if(max_smoke < 10) max_smoke = 10; }
            while(KEY_SUB == 0);
        }
    }
}

//====================== 屏幕刷新 ======================
void Display_Update() {
    if(set_mode == 0) {
        LCD_ShowString(1, 1, "Temp :   C");
        LCD_ShowChar(1, 8, 0xDF); 
        LCD_ShowNum(1, 7, temp_value, 2);
        
        LCD_ShowString(2, 1, "Smoke:   %");
        LCD_ShowNum(2, 8, smoke_value, 2);
    } 
    else if(set_mode == 1) {
        LCD_ShowString(1, 1, "Set Max Temp:");
        LCD_ShowNum(2, 7, max_temp, 2);
        LCD_ShowString(2, 9, "C");
    } 
    else if(set_mode == 2) {
        LCD_ShowString(1, 1, "Set Max Smoke:");
        LCD_ShowNum(2, 7, max_smoke, 2);
        LCD_ShowString(2, 9, "%");
    }
}

//====================== 报警控制 ======================
void Check_Alarm() {
    if(set_mode != 0) { // 设置模式时关闭报警
        BUZZER = 0; RELAY = 0; 
        LED_RED = 1; LED_GREEN = 0; 
        return;
    }
    if(temp_value > max_temp || smoke_value > max_smoke) {
        LED_RED = 0;   
        LED_GREEN = 1; 
        BUZZER = 1;    // 报警响
        RELAY = 1;     // 风扇转
    } else {
        LED_RED = 1;   
        LED_GREEN = 0; 
        BUZZER = 0;    
        RELAY = 0;     
    }
}

//====================== 主函数 ======================
void main()
{
    LCD_Init(); 
    
    // 初始化时先启动一次温度转换，并等待它转换完毕，防止刚开机时跳 85度
    DS18B20_Init(); 
    DS18B20_WriteByte(0xCC); 
    DS18B20_WriteByte(0x44);
    Delay1ms(750); // 必须等750ms，因为DS18B20转换一次需要时间
    
    while(1)
    {
        if(set_mode == 0) {
            DS18B20_ReadTemp();         
            smoke_value = ADC0832_Read(); 
        }
        
        Key_Scan();       
        Display_Update(); 
        Check_Alarm();    
        
        Delay1ms(100);    
    }
}
