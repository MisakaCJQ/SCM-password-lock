#include <STC15F2K60S2.h>
#include <intrins.h>
#define uint unsigned int
#define uchar unsigned char

#define ADC_POWER 0x80      /*ADC电源开关*/
#define ADC_FLAG 0x10       /*当A/D转换完成后，ADC_FLAG要软件清零*/
#define ADC_START 0x08      /*当A/D转换完成后，cstAdcStart会自动清零，所以要开始下一次转换，则需要置位*/
#define ADC_SPEED_90 0x60   /*ADC转换速度 90个时钟周期转换一次*/
#define ADC_CHS1_7 0x07     /*选择P1.7作为A/D输入*/


uchar i=0,blink=0;//i用于定时器中断当中数码管扫描的变量，blink用于250ms分频
uchar barCounter=0;//用于LED进度条显示时的分频
uchar barLedCounter=0;//用于LED进度条显示时切换数组元素
uchar flagblink=0;//用于记录闪烁位的翻转状态
uchar flagState=0;//状态标志，0代表锁定状态，1代表解锁状态，2代表设定密码状态
uchar flagBar=0;//用于进度条LED显示的标志位
uchar flagWrong=0;//标志输错密码
uchar flagBeep=0;//用于标志蜂鸣器工作
uchar wrongCounter=0;//用于错误时LED显示分频
uchar wrongLedCounter=0;//用于错误时选择LED显示
uchar segSelect[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f}; //段选信号，显示0-9
uchar digSelect[]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07};//位选信号，数码管0-7位
uchar progressBarLed[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};//LED进度条
uchar wrongLed[]={0x00,0xaa,0x55};//密码错误时的三种LED显示
uchar segLine=0x40;//横杠段选信号
//uchar writePw[8]={0};//待写入的6位密码，仅有后6个数据有效
uchar readPw[8]={0};//读取出的6位密码
uchar pwshow[8]={0x0};//锁定状态数码管上显示的6位密码
uchar editBit=2;//当前编辑位，即右下角有点的位。

sbit sbtLedSel = P2 ^ 3;//数码管和LED切换引脚
sbit sbtBeep = P3 ^ 4;//蜂鸣器引脚
sbit sbtKey1 = P3 ^ 2;//key1引脚
sbit sbtKey2 = P3 ^ 3;//key2引脚
sbit sbtKey3 = P1 ^ 7;//key3引脚
sbit SDA = P4 ^ 0;//I2C总线的数据线
sbit SCL = P5 ^ 5;//I2C总线的时钟线
sbit sbtIn =P1 ^ 0;//P1.0引脚，连接继电器输入。

void Delay500ms()		//@11.0592MHz
{
	unsigned char i, j, k;

	_nop_();
	_nop_();
	i = 22;
	j = 3;
	k = 227;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay100ms()       //@11.0592MHz  延时100ms
{
    unsigned char i, j, k;
    _nop_();
    _nop_();
    i = 5;
    j = 52;
    k = 195;
    do
    {
        do
        {
            while ( --k );
        }
        while ( --j );
    }
    while ( --i );
}

void Delay80ms()		//@11.0592MHz
{
	unsigned char i, j, k;

	_nop_();
	_nop_();
	i = 4;
	j = 93;
	k = 155;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay10ms()		//@11.0592MHz
{
	unsigned char i, j;

	i = 108;
	j = 145;
	do
	{
		while (--j);
	} while (--i);
}

void Delay5ms()     //@11.0592MHz  延时5ms
{
    unsigned char i, j;
    i = 54;
    j = 199;
    do
    {
        while ( --j );
    }
    while ( --i );
}

void Delay1ms()		//@11.0592MHz
{
	unsigned char i, j;

	_nop_();
	_nop_();
	_nop_();
	i = 11;
	j = 190;
	do
	{
		while (--j);
	} while (--i);
}

void Delay4us()        //延时4us
{
    ;;
}

void Delayms( uint n )
{
    while( n )
    {
        uchar i, j;
        i = 11;
        j = 190;
        do
        {
            while ( --j );
        }
        while ( --i );
        n--;
    }
}
/******************ADC导航按键部分***************************/
/**************************************
 * 函数名：GetADC
 * 描述  ：获得AD转换的值,没有设置A/D转换中断（具体看IE、IP）
 * 输入  ：无
 * 输出  ：AD转换的结果
 **************************************/

uchar GetADC()
{
	unsigned char result;
	ADC_CONTR = ADC_POWER | ADC_START | ADC_SPEED_90 | ADC_CHS1_7;//没有将ADC_FLAG置1，用于判断A/D是否结束
	_nop_();
	_nop_();
	_nop_();
	_nop_();
	while(!(ADC_CONTR&ADC_FLAG));//等待直到A/D转换结束
	ADC_CONTR &= ~ADC_FLAG; //ADC_FLAGE软件清0
	result = ADC_RES; //获取AD的值
	return result;	  //返回ADC值
}

/********************************
 * 函数名：NavKeycheck()	   
 * 描述  ：检测功能键的上5、下2、左4、右1、确认键3、开关按键3（0），没按下返回0x07，返回相应的值  (包含消抖过程)
 * 输入  ：无
 * 输出  ：键对应的值
********************************/
uchar NavKeyCheck()
{
	uchar key;
	key=GetADC();		  //获得ADC值赋值给key
	if(key!=255)
	{
		Delay1ms();
		key=GetADC();
		if(key!=255)	  //按键消抖
		{
	     	key=key&0xE0;//获取高3位，其他位清零
        	key=_cror_(key,5);//循环右移5位
			return key;
		}
	}
	return 0x07;
}

void NavKey_Process()
{
    uchar NavKeyCurrent;  //导航按键当前的状态
    uchar NavKeyPast;     //导航按键前一个状态

    NavKeyCurrent = NavKeyCheck();    //获取当前ADC值
    if( NavKeyCurrent != 0x07 )       //导航按键是否被按下 不等于0x07表示有按下
    {
        NavKeyPast = NavKeyCurrent;
        while( NavKeyCurrent != 0x07 )        //等待导航按键松开
            NavKeyCurrent = NavKeyCheck();
				//松开后再对输入的信号处理
        switch( NavKeyPast )
        {
            case 0x05 ://向上
                if(pwshow[editBit]==9)
                    pwshow[editBit]=0;
                else
                    pwshow[editBit]++;
                break;
            case 0x02 ://向下
                if(pwshow[editBit]==0)
                    pwshow[editBit]=9;
                else
                    pwshow[editBit]--;
                break;
            case 0x04 ://向左
                if(editBit==2)
                    editBit=7;
                else
                    editBit--;
                break;
            case 0x01 ://向右
                if(editBit==7)
                    editBit=2;
                else
                    editBit++;
                break;
            case 0x03 ://中间
                pwshow[editBit]=0;
                break;
        }
    }

    //Delay100ms();
}



/**********************非易失存储器部分**************************/
void IIC_init()     //I2C总线初始化
{
    SCL = 1;
    Delay4us();
    SDA = 1;
    Delay4us();
}

void start()        //主机启动信号
{
    SDA = 1;
    Delay4us();
    SCL = 1;
    Delay4us();
    SDA = 0;
    Delay4us();
}

void stop()         //停止信号
{
    SDA = 0;
    Delay4us();
    SCL = 1;
    Delay4us();
    SDA = 1;
    Delay4us();
}

void respons()      //从机应答信号
{
    uchar i = 0;
    SCL = 1;
    Delay4us();
    while( SDA == 1 && ( i < 255 ) ) //表示若在一段时间内没有收到从器件的应答则
        i++;                //主器件默认从期间已经收到数据而不再等待应答信号。
    SCL = 0;
    Delay4us();
}

void writebyte( uchar data0 )//对24C02写一个字节数据
{
    uchar i, temp;
    temp = data0;
    for( i = 0; i < 8; i++ )
    {
        temp = temp << 1;
        SCL = 0;
        Delay4us();
        SDA = CY;
        Delay4us();
        SCL = 1;
        Delay4us();
    }
    SCL = 0;
    Delay4us();
    SDA = 1;
    Delay4us();
}
uchar readbyte()            //从24C02读一个字节数据
{
    uchar i, k;
    SCL = 0;
    Delay4us();
    SDA = 1;
    Delay4us();
    for( i = 0; i < 8; i++ )
    {
        SCL = 1;
        Delay4us();
        k = ( k << 1 ) | SDA;
        Delay4us();
        SCL = 0;
        Delay4us();
    }
    Delay4us();
    return k;
}

void write_add( uchar addr, uchar data0 ) //对24C02的地址addr，写入一个数据data0
{
    start();
    writebyte( 0xa0 );
    respons();
    writebyte( addr );
    respons();
    writebyte( data0 );
    respons();
    stop();
}
uchar read_add( uchar addr )      //从24C02的addr地址，读一个字节数据
{
    uchar data0;
    start();
    writebyte( 0xa0 );
    respons();
    writebyte( addr );
    respons();
    start();
    writebyte( 0xa1 );
    respons();
    data0 = readbyte();
    stop();
    return data0;
}

void init()//初始化
{
    uchar j=0,data0=0x00;;
    //设定P0,P1.0,P2.3,P3.4推挽输出
    P0M1=0x00;
    P0M0=0xff;
    P1M1=0x00;
    P1M0=0x01;
    P2M1=0x00;
    P2M0=0x08;
    P3M1 = 0x00;//蜂鸣器
    P3M0 = 0x10;
    
    //AD信号初始化部分
    P1ASF=0x80;//将P1ASF寄存器对应位置1
	ADC_RES = 0;//结果寄存器清零
    //ADC_RESL=0;
	ADC_CONTR = ADC_POWER | ADC_FLAG | ADC_START | ADC_SPEED_90 | ADC_CHS1_7;		//对应位赋值
	Delayms(2);
    
    /************定时器0设置******************/
    TMOD=0x01;				//定时器0，方式1。定时器1方式0，16位自动重装。
	EA=1;					//打开总的中断
	ET0=1;					//开启定时器0中断	
 	TH0=(65535-1000)/256;	//设置定时初值
	TL0=(65535-1000)%256;
	TR0=1;					//启动定时器
    /***********定时器1设置******************/
    AUXR |= 0x40;           //定时器1时钟1T模式
    ET1=1;					//开启定时器1中断
    TH1 = 0xF4;             //设置定时1初值
    TL1 = 0xCD;             //设置定时1初值
    TF1 = 0;                //清除TF1标志
    TR1 = 1;                //定时器1开始计时

    sbtLedSel=0;
    sbtBeep=0;
    sbtIn=0;
    P0=0x00;

    IIC_init();//IIC初始化
    for(j=0;j<8;j++)//从非易失存储器中读出密码
    {
        data0=read_add(j);
        readPw[j]=data0;
    }

    /*
    for(j=0;j<8;j++)
    {
        write_add(j,0x08);
        Delay10ms();
    }
    */
}

void timer0() interrupt 1
{
    TH0=(65535-1000)/256;	  //重新填充初值
	TL0=(65535-1000)%256;

    //NavKey_Process();//获取导航按键状态
    i++;
    if(i==8)
        i=0;
    
    if(flagState==0)//锁定状态
    {
        //错误处理部分
        if(flagWrong==1)
        {
            wrongCounter++;
            if(wrongCounter==50)
                wrongLedCounter=1;
            else if(wrongCounter==100)
                wrongLedCounter=2;
            else if(wrongCounter==150)
                wrongLedCounter=1;
            else if(wrongCounter==200)
                wrongLedCounter=2;
            else if(wrongCounter==250)
            {
                wrongLedCounter=0;//LED停止显示
                flagWrong=0;//关闭错误标志
                wrongCounter=0;//错误分频归0
            }
            
        }

        //显示部分
        P0=0;
        sbtLedSel=0;
        P2=digSelect[i];
        if(i<2)//前两位显示横杠
            P0=segLine;
        else
        {
            if(i==editBit)
                P0=segSelect[pwshow[i]] | 0x80;//编辑位加小数点
            else
                P0=segSelect[pwshow[i]];
            //P0=segSelect[i+1];
        }
        Delay1ms();

        if(i==7&&flagWrong==1)
        {
            P0=0;
            sbtLedSel=1;
            P0=wrongLed[wrongLedCounter];
            Delay1ms();
        }
        

    }
    else if (flagState==1)//解锁状态
    {
        if(flagBar==0)//初次进入解锁状态
        {
            barCounter++;
            if (barCounter==20)//分频
            {
                if (barLedCounter==8)//进度条已显示完毕
                    flagBar=1;//将标志设为1
                else
                    barLedCounter++;//否则就将显示的LED右移
                barCounter=0;
            }
        }
        
        P0=0;
        sbtLedSel=0;
        P2=digSelect[i];
        P0=segLine;
        Delay1ms();

        if(i==7)
        {
            P0=0;
            sbtLedSel=1;
            P0=progressBarLed[barLedCounter];
            Delay1ms();
        }
        
    }
    else if(flagState==2)//修改状态
    {
        blink++;
        if(blink==100)
        {
            blink=0;
            if(flagblink==0)//翻转闪烁状态
                flagblink=1;
            else
                flagblink=0;
        }

        barCounter++;
        if(barCounter==100)
        {
            barCounter=0;
            if(barLedCounter==0)
                barLedCounter=8;
            else
                barLedCounter=0;
        }

        P0=0;
        sbtLedSel=0;
        P2=digSelect[i];
        if(i<2)//前两位显示横杠
        {
            if(flagblink==1)
                P0=segLine;
            else
                P0=0;
        }
        else
        {
            if(i==editBit)
                P0=segSelect[pwshow[i]] | 0x80;
            else
                P0=segSelect[pwshow[i]];
        }
        Delay1ms();

        if(i==7)
        {
            P0=0;
            sbtLedSel=1;
            P0=progressBarLed[barLedCounter];
            Delay1ms();
        }
    }
    
    //Delay1ms();
}

void timer1() interrupt 3//定时器1
{
    if(flagBeep)
        sbtBeep = ~sbtBeep;//产生方波使得蜂鸣器发声
    else
        sbtBeep = 0;//如果开关关闭,则蜂鸣器断电以保护蜂鸣器
}

void main()
{
    uchar j=0;//用于写入或修改数据时在循环内的变量
    init();
	while(1)
    {
        if(flagState==0)//锁定状态
        {
            NavKey_Process();//获取并处理导航按键状态
            if(sbtKey1==0)//检测到key1按下
            {
                Delay5ms();//消抖
                if(sbtKey1==0)
                {
                    while( !sbtKey1 );//直到key1松开
                    if(pwshow[2]==readPw[2]&&pwshow[3]==readPw[3]&&pwshow[4]==readPw[4]&&
                    pwshow[5]==readPw[5]&&pwshow[6]==readPw[6]&&pwshow[7]==readPw[7])
                    {
                        barCounter=0;//将LED进度条分频变量归0
                        barLedCounter=0;//将LED进度条数组切换计数器归0
                        flagBar=0;//LED进度条初次显示标志归0
                        flagState=1;
                        /*********蜂鸣器提示部分*********/
                        TH1=0xef;
                        TR0=0;
                        P0=0;//清除数码管
                        flagBeep=1;
                        Delay80ms();
                        flagBeep=0;
                        Delay80ms();
                        flagBeep=1;
                        Delay80ms();
                        flagBeep=0;
                        TR0=1;

                        sbtIn=1;//继电器输入高电平
                    }
                    else
                    {
                        flagState=0;
                        wrongCounter=0;//错误LED分频归0
                        wrongLedCounter=0;//错误LED选择归0
                        flagWrong=1;//错误标志置1
                        /*********蜂鸣器提示部分*********/
                        TH1=0xe5;
                        TR0=0;
                        P0=0;
                        flagBeep=1;
                        Delay500ms();
                        flagBeep=0;
                        TR0=1;
                    }
                        
                }
            }
        }
        else if(flagState==1)//解锁状态
        {
            if(sbtKey1==0)//检测到key1按下，重新锁定
            {
                Delay5ms();//消抖
                if(sbtKey1==0)//重新锁定
                {
                    while( !sbtKey1 );//直到key1松开
                    for(j=2;j<8;j++)//将显示密码清空
                        pwshow[j]=0;
                    editBit=2;//编辑位小数点复位
                    flagState=0;//改回锁定状态
                    sbtIn=0;//继电器输入低电平
                    flagBar=0;//LED进度条初次显示标志归0
                    barLedCounter=0;//将LED进度条数组切换计数器归0
                    barCounter=0;//将LED进度条分频变量归0
                    //flagBeep=0;
                    
                }
            }
            if(sbtKey2==0)//检测到key2按下，进入修改密码状态
            {
                Delay5ms();//消抖
                if(sbtKey2==0)
                {
                    while( !sbtKey2 );//直到key2松开
                    editBit=2;//编辑位小数点复位
                    flagState=2;//进入修改密码状态
                    barLedCounter=0;//将LED进度条数组切换计数器归0
                    barCounter=0;//将LED进度条分频变量归0
                }
            }
        }
        else if(flagState==2)//修改状态
        {
            NavKey_Process();//获取并处理导航按键状态
            if(sbtKey1==0)//检测到key1按下，保存修改
            {
                Delay5ms();//消抖
                if(sbtKey1==0)
                {
                    while( !sbtKey1 );//直到key1松开
                    for(j=2;j<8;j++)//将密码逐位写入非易失存储器
                    {
                        write_add(j,pwshow[j]);
                        Delay10ms();
                        readPw[j]=pwshow[j];
                        //pwshow[j]=0x0;
                    }
                    flagState=1;//改回解锁状态
                    barLedCounter=8;//将LED进度条数组切换计数器归0
                    barCounter=0;//将LED进度条分频变量归0
                }
            }
            if (sbtKey2==0)//检测到key2按下，不保存修改
            {
                Delay5ms();//消抖
                if(sbtKey2==0)
                {
                    while( !sbtKey2 );//直到key1松开
                    flagState=1;//改回解锁状态
                    barLedCounter=8;//将LED进度条数组切换计数器归0
                    barCounter=0;//将LED进度条分频变量归0
                }
            }
        }
        else
        {
            flagState=0;
        }
    }
}