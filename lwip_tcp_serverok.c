/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>

#include "iot_gpio_ex.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_uart.h"
#include "hi_uart.h"
#include "iot_watchdog.h"
#include "iot_errno.h"

#include "lwip/sockets.h"

//定义蜂鸣器和激光GPIO引脚
#define BEEP_GPIO    2
#define LASER_GPIO   9

static uint32_t status = 0;
static uint32_t U_SLEEP_TIME = 50000;  

#define UART_BUFF_SIZE 100
//#define U_SLEEP_TIME   100000//开门狗的时间

static uint8_t s_data[39] = {0};//定义数组储存通信协议
int x=0,y=0;
char distance[4]={0},a1[4]={0}, b1[4]={0}, c1[4]={0}, d1[4]={0},a2[4]={0}, b2[4]={0}, c2[4]={0},d2[4]={0}; // 存储分割后的数字
int distance_val=0,a1_val=0,a2_val=0,b1_val=0,b2_val=0,c1_val=0,c2_val=0,d1_val=0,d2_val=0;
char uartReadBuff[UART_BUFF_SIZE] = {0};//2种转向方式判断
char uartReadBuff_zfc[]={0};
char v[] = {0}; 
// int retval; // 声明全局变量retval
int connfd; // 声明全局变量connfd
double retval;
char prevUartReadBuff[66]={0};
char tempBuff[UART_BUFF_SIZE]; // 定义临时缓冲区



//*************************************任务一：网络端口接收********************************************

void TcpServerTask(unsigned short port)//定义了一个名为TcpServerTask的函数，该函数接收一个无符号短整型参数port，表示服务器要监听的端口号。
{
    
    int backlog = 1;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //定义了两个变量backlog和sockfd，分别表示允许的最大未处理连接数和套接字描述符。创建一个IPv4的TCP套接字，并将其描述符赋值给sockfd。

    struct sockaddr_in clientAddr = {0};
    socklen_t clientAddrLen = sizeof(clientAddr);
    struct sockaddr_in serverAddr = {0};//初始化服务器地址结构体serverAddr，
    serverAddr.sin_family = AF_INET;//设置其协议族
    serverAddr.sin_port = htons(port);  //端口号
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); //地址

    ssize_t retval = bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)); //将服务器地址结构体绑定到套接字上
    
    if (retval < 0)//如果绑定失败，关闭套接字
    {
        lwip_close(sockfd);
    }
    retval = listen(sockfd, backlog); //开始监听套接字，等待客户端连接
    if (retval < 0) 
    {
        lwip_close(sockfd);
    }

    while(1)//使用无限循环来处理客户端连接。
    {
        char g_request[10]={0};//定义一个字符数组g_request，用于存储客户端发送的指令。
        connfd = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen);//接受客户端连接，
        
        if (connfd < 0) {//如果连接失败，关闭套接字并退出循环。
            printf("accept failed, %d, %d\r\n", connfd, errno);
            printf("do_cleanup...\r\n");
            lwip_close (sockfd);
        }
        else//如果连接成功，进入另一个无限循环，用于处理客户端发送的指令。
        {
                                            
            while(1)
            {
                // char g_request[10]={0};//定义一个字符数组g_request，用于存储客户端发送的指令。
                retval = recv(connfd, g_request, sizeof(g_request), 0);
                printf("connfd: %d\n", connfd);
                if (retval < 0) //从客户端接收数据，如果接收失败，关闭连接并退出循环。
                {
                    sleep(1);
                    lwip_close(connfd);
                    break;
                }
                //客户端输入指令命令小车是否开始动
                if(strcmp(g_request, "TCPO") == 0)//小车开始动
                {
                    status=1;//status=1是串口控制
                    U_SLEEP_TIME = 35000;
                    //printf("2\n");

                }
                if(strcmp(g_request, "TCPC") == 0)//小车停
                {
                    status=2;
                    U_SLEEP_TIME = 50000;    
                }

         }
        
                }
               
    }
   
     lwip_close(sockfd);    //当客户端断开连接时，关闭连接并继续等待下一个客户端连接。当服务器任务结束时，关闭套接字。
}
//线程一：网络连接
void NetDemoTest(unsigned short port, const char* host)//定义了一个名为NetDemoTest的函数，该函数接收一个无符号短整型参数port和一个字符串指针参数host，调用TcpServerTask函数启动服务器任务。
{
    (void) host;
    TcpServerTask(port);
}


//*************************************任务二：UART串口接收********************************************

// void Uart1GpioInit(void)//11口为TX，12口为RX。即初始化端口
// {
//     IoTGpioInit(IOT_IO_NAME_GPIO_11);// 设置GPIO11的管脚复用关系为UART1_TX 
//     IoSetFunc(IOT_IO_NAME_GPIO_11, IOT_IO_FUNC_GPIO_11_UART2_TXD);
//     IoTGpioInit(IOT_IO_NAME_GPIO_12);// 设置GPIO12的管脚复用关系为UART1_RX 
//     IoSetFunc(IOT_IO_NAME_GPIO_12, IOT_IO_FUNC_GPIO_12_UART2_RXD);
// }



// void Uart1Config(void)//即配置端口
// {
//     uint32_t ret;// 初始化UART配置，波特率 115200，数据bit为8,停止位1，奇偶校验为NONE 
//     IotUartAttribute uart_attr = {
//         .baudRate = 115200,
//         .dataBits = 8,
//         .stopBits = 1,
//         .parity = 0,
//     };
//     ret = IoTUartInit(HI_UART_IDX_2, &uart_attr);
//     if (ret != IOT_SUCCESS) {
//         printf("Init Uart1 Falied Error No : %d\n", ret);
//         return;
//     }
// }


// void Uart2GpioInit(void)
// {
//     IoTGpioInit(IOT_IO_NAME_GPIO_0);
//     // 设置GPIO0的管脚复用关系为UART1_TX Set the pin reuse relationship of GPIO0 to UART1_ TX
//     IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_UART1_TXD);
//     IoTGpioInit(IOT_IO_NAME_GPIO_1);
//     // 设置GPIO1的管脚复用关系为UART1_RX Set the pin reuse relationship of GPIO1 to UART1_ RX
//     IoSetFunc(IOT_IO_NAME_GPIO_1, IOT_IO_FUNC_GPIO_1_UART1_RXD);
// }

// void Uart2Config(void)
// {
//     uint32_t ret;
//     /* 初始化UART配置，波特率 9600，数据bit为8,停止位1，奇偶校验为NONE */
//     /* Initialize UART configuration, baud rate is 9600, data bit is 8, stop bit is 1, parity is NONE */
//     IotUartAttribute uart_attr = {
//         .baudRate = 115200,
//         .dataBits = 8,
//         .stopBits = 1,
//         .parity = 0,
//     };
//     ret = IoTUartInit(HI_UART_IDX_1, &uart_attr);
//     if (ret != IOT_SUCCESS) {
//         printf("Init Uart2 Falied Error No : %d\n", ret);
//         return;
//     }
// }


void Uart1GpioInit(void)
{
    // Pegasus & Taurus UART1串口
    IoTGpioInit(IOT_IO_NAME_GPIO_0);
    IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_UART1_TXD);

    IoTGpioInit(IOT_IO_NAME_GPIO_1);
    IoSetFunc(IOT_IO_NAME_GPIO_1, IOT_IO_FUNC_GPIO_1_UART1_RXD);
}
void Uart2GpioInit(void)
{
    // Pegasus & Stm32 UART2串口
    IoTGpioInit(IOT_IO_NAME_GPIO_11);
    IoSetFunc(IOT_IO_NAME_GPIO_11, IOT_IO_FUNC_GPIO_11_UART2_TXD);

    IoTGpioInit(IOT_IO_NAME_GPIO_12);
    IoSetFunc(IOT_IO_NAME_GPIO_12, IOT_IO_FUNC_GPIO_12_UART2_RXD);
}
// 配置UART串口
void Uart1Config(void)
{
    uint32_t ret;
    IotUartAttribute uart_attr = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    ret = IoTUartInit(HI_UART_IDX_1, &uart_attr);
    if (ret != IOT_SUCCESS)
    {
        printf("Init Uart1 Falied Error No : %d\n", ret);
        return;
    }
}
void Uart2Config(void)
{
    uint32_t ret;
    IotUartAttribute uart_attr = {
        .baudRate = 115200,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    ret = IoTUartInit(HI_UART_IDX_2, &uart_attr);
    if (ret != IOT_SUCCESS)
    {
        printf("Init Uart2 Falied Error No : %d\n", ret);
        return;
    }
}





// 串口数据赋值函数，
void Angle_Block_Assignment(void)//发送给32信号的初始化
{
    s_data[0] = 0xAA;
    s_data[1] = 0x55;
    s_data[2]=0x03;
    s_data[3]=0x02;
    s_data[4]=0x03;
    s_data[5]=0x0F;
    s_data[6]=0xD3;
    s_data[7]=0x00;
    s_data[8]=0;
    s_data[9]=0;
    s_data[10]=0; 
    s_data[11]=0;
    s_data[12]=0;
    s_data[13]=0;
    s_data[14]=0;
    s_data[15]=0;
    s_data[16]=0;
    s_data[17]=0;
    s_data[18]=0;
    s_data[19]=0;
    s_data[20]=0;
    s_data[21]=0;
    s_data[22]=0;
    s_data[23]=0;
    s_data[24]=0;
    s_data[25]=0;
    s_data[26]=0;
    s_data[27]=0;
    s_data[28]=0;
    s_data[29]=0;
    s_data[30]=0;
    s_data[31]=0;
    s_data[32]=0;
    s_data[33]=0;
    s_data[34]=0;
    s_data[35]=0;
    s_data[36]=0;
    s_data[37]=0;
    s_data[38]=0;



}


// Taurus & Pegasus串口通信，判断车往左还是右转
void UartTask(void)
{

    IoTGpioInit(BEEP_GPIO);
    IoTGpioSetDir(BEEP_GPIO, IOT_GPIO_DIR_OUT);

    uint32_t count = 0;
   
    Uart1GpioInit();// 对UART1的一些初始化 
    Uart1Config();// 对UART1参数的一些配置 
    Uart2GpioInit();// 对UART1的一些初始化 
    Uart2Config();// 对UART1参数的一些配置 

    Angle_Block_Assignment();
    // char prevUartReadBuff[66]={0};
    char uartReadBuff[UART_BUFF_SIZE]={0};
    while(1){
    
    //char p[20][10]={""};//定义20个长度为10的字符串，用于存放得到的测距等信息，后续需要去分别提取使用

    //char tempBuff[UART_BUFF_SIZE]={0};//置空
    usleep(300000);
    char uartReadBuff[UART_BUFF_SIZE]={};
   // printf("strlen(tempBuff0): %d\n",strlen(tempBuff0));

    //printf("111uartReadBuff: %s\n",uartReadBuff);
    IoTUartRead(HI_UART_IDX_2, uartReadBuff, sizeof(uartReadBuff));//读串口消息，并存6个字节的数据UART_BUFF_SIZE
       //  printf("222uartReadBuff: %s\n",uartReadBuff);
//    printf("tempBuff: %s\n",tempBuff);
//      if (strlen(tempBuff)==0){//如果读到的长度不变就还是原来的,没发送的话
//         strcpy(uartReadBuff,tempBuff0);
// //         printf("222\n");
//      }
//      else{
//          strcpy(uartReadBuff, tempBuff);//把tempBuff赋值给uartReadBuff
// //         printf("333\n");
//      }
   
//     printf("uartReadBuff: %s\n",uartReadBuff);
    
    
   // 假设 uartReadBuff 中读到的串口消息格式为 "a,b,c,d"
    // 使用 strtok 函数将字符串按逗号分割成四个部分
   
    strcpy(uartReadBuff_zfc, uartReadBuff); //将uartReadBuff转换为字符串
    //printf("123uartReadBuff_zfc: %s\n",uartReadBuff);
    char *token = strtok(uartReadBuff_zfc, "a");

    if (token != NULL) {
        strcpy(distance, token); // 将第一个数字存入变量 distance 中
        token = strtok(NULL, ","); // 继续分割下一个数字
        distance_val=atoi(distance);//val是数字，distance是字符串
        
    if (token != NULL) {
        strcpy(a1, token); // 将第一个数字存入变量 a1 中
        token = strtok(NULL, "b"); // 继续分割下一个数字
        a1_val=atoi(a1);
    

        if (token != NULL) {
            strcpy(a2, token); // 将第二个数字存入变量 a2 中
            a2_val=atoi(a2);
            token = strtok(NULL, ",");
          
          

            if (token != NULL) {
                strcpy(b1, token); // 将第三个数字存入变量 b1 中
                b1_val=atoi(b1);
                token = strtok(NULL, "c");
           
            

                if (token != NULL) {
                    strcpy(b2, token); // 将第四个数字存入变量 b2 中
                    b2_val=atoi(b2);
                    token = strtok(NULL, ","); // 继续分割下一个数字
                

                     if (token != NULL) {
                          strcpy(c1, token); // 将第一个数字存入变量 c1 中
                          c1_val=atoi(c1);
                          token = strtok(NULL, "d"); // 继续分割下一个数字
                   
                          if (token != NULL) {
                             strcpy(c2, token); // 将第二个数字存入变量 c2 中
                             c2_val=atoi(c2);
                             token = strtok(NULL, ",");
                         

                             if (token != NULL) {
                                strcpy(d1, token); // 将第三个数字存入变量 d1 中
                                d1_val=atoi(d1);
                                token = strtok(NULL, "e");
                               

                                if (token != NULL) {
                                    strcpy(d2, token); // 将第四个数字存入变量 d2中
                                    d2_val=atoi(d2);
                                    

                                
                }
            }
        }
    }
                }
            }
        }
    }
        
    }
      
       
   
        //printf("uartReadBuff:%s\n,distance_val:%d\n,a1_val: %d\n,a2_val: %d\n,b1_val: %d\n,b2_val: %d\n,c1_val: %d\n,c2_val: %d\n,d1_val: %d\n,d2_val: %d\n,x: %d, y: %d\n",uartReadBuff,distance_val,a1_val,a2_val,b1_val,b2_val,c1_val,c2_val,d1_val,d2_val,x,y);
    
    
    



    if (status==1)//上位机输入开始后
    {
            //判断每个字符串长度是否小于4，
    //printf("111x:%d,y:%d\n",x,y);//会显示错误的x和y值
    char uartReadBuff222[]={0};
    IoTUartRead(HI_UART_IDX_1, uartReadBuff222, sizeof(uartReadBuff222));
    IoTUartWrite(HI_UART_IDX_1, uartReadBuff222, sizeof(uartReadBuff222));
    printf("uartReadBuff222:%s\n",uartReadBuff222);

// int i=1;
// char uartReadBuff[UART_BUFF_SIZE]={0};
// char v[9][1]={"a",",","b",",","c",",","d",",","e"};

// for (i=0;i<9;i++){//目前输入9个数，后续要改
//     if (strlen(p[i])<4){
//         char zero []="0";
//         char zero2 []="00";
//         char zero3 []="000";
//         char zero4 []="0000";
//         if (strlen(p[i])==3){
//              strcat(zero,p[i]);//实现0+distance
//              strcpy(p[i], zero);// 使v为v
//         }
//          if (strlen(p[i])==2){
//              strcat(zero2,p[i]);//实现v+uartReadBuff
//              strcpy(p[i], zero2);// 使v为v
//         }
//       if (strlen(p[i])==1){
//              strcat(zero3,p[i]);//实现v+uartReadBuff
//              strcpy(p[i], zero3);// 使v为v
//         }
//         if (strlen(p[i])==0){
//              strcat(zero4,p[i]);//实现v+uartReadBuff
//              strcpy(p[i], zero4);// 使v为v
//         }
//     }

// strcat(uartReadBuff,p[i]);
// //printf("p[i]: %s\n",p[i]);
// strcat(uartReadBuff,v[i][0]);
// //printf("v[j]: %s\n",v[j]);


// //printf("p[1]: %s\n",p[1]);
// //printf("v[1]: %c\n",v[0][0]);
// printf("uartReadBuff: %s\n",uartReadBuff);

// }






           //printf("prevUartReadBuff: %s\n",prevUartReadBuff);
          //  printf("uartReadBuff: %s\n",uartReadBuff);
        
        if (distance_val<500){//距离小于500就响
    Angle_Block_Assignment();
     s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x02;//功能码
    s_data[3]=0x08;//数据长度
    s_data[4]=0x78;//子命令
    s_data[5]=0x05;//电机数量
    s_data[6]=0x64;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x64;
    s_data[9]=0x00;
    s_data[10]=0x05; // 速度
    s_data[11]=0x00;//2号电机ID号
    s_data[12]=0xF0;
    s_data[13]=0;
    s_data[14]=0;
    s_data[15]=0;
    s_data[16]=0;
    s_data[17]=0;
    s_data[18]=0;
    s_data[19]=0;
    s_data[20]=0;
    s_data[21]=0;
    s_data[22]=0;
    s_data[23]=0;
    s_data[24]=0;
    s_data[25]=0;
    s_data[26]=0;
    s_data[27]=0;
    s_data[28]=0;
    s_data[29]=0;
    s_data[30]=0;
    s_data[31]=0;
    s_data[32]=0;
    s_data[33]=0;
    s_data[34]=0;
    s_data[35]=0;
    s_data[36]=0;
    s_data[37]=0;
    s_data[38]=0;
    IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
    usleep(U_SLEEP_TIME);

    distance_val=501;//初始化距离 
               }
      






        if (strlen(uartReadBuff)!=0) 
           {
                   char vorignal[]="v";
          // printf("111v+uartreadbuff: %s\n",v);
           //printf("111prevUartReadBuff: %s\n",prevUartReadBuff);
            
            strcpy(prevUartReadBuff, uartReadBuff);//把uartreadbuff赋值给prev
           
            strcpy(v, vorignal);// 使v为v
            strcat(v,uartReadBuff);//实现v+uartReadBuff

          //  printf("222v+uartreadbuff: %s\n",v);
          //  printf("222prevUartReadBuff: %s\n",prevUartReadBuff);
        
            char send_data[100];
            // //memset(send_data, 0, sizeof(send_data));// 清空send_data的内容
            strcpy(send_data,v);//把v赋值给send_data
            printf("send_data: %s\n",send_data);
            
            
            retval = send(connfd, send_data, strlen(send_data), 0); // 发送字符串给客户端
            //printf("g_connfd: %d\n",connfd);
                if (retval < 0) // 如果发送失败，关闭连接并退出循环。
                {
                    sleep(1);
                    lwip_close(connfd);
                    //break;
                    
                }
               // printf("222x:%d,y:%d\n",x,y);
}


        if (strlen(uartReadBuff)==0) //如果没检测到就让小车停/或者让小车向左前走
    {
      //  printf("strlen(uartReadBuff): %d\n",strlen(uartReadBuff));
    
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    // s_data[0]=0xAA;
    // s_data[1]=0x55;//帧头
    // s_data[2]=0x03;//功能码
    // s_data[3]=0x22;//数据长度
    // s_data[4]=0x01;//子命令
    // s_data[5]=0x04;//电机数量
    // s_data[6]=0x00;//1号电机ID号
    // s_data[7]=0x00;
    // s_data[8]=0x00;
    // s_data[9]=0x00;
    // s_data[10]=0xC0; // 速度
    // s_data[11]=0x01;//2号电机ID号
    // s_data[12]=0x00;
    // s_data[13]=0x00;
    // s_data[14]=0x80;
    // s_data[15]=0x3F; // 速度
    // s_data[16]=0x02;//3号电机ID号
    // s_data[17]=0x00;
    // s_data[18]=0x00;
    // s_data[19]=0x00;
    // s_data[20]=0xC0; // 速度
    // s_data[21]=0x03;//4号电机ID号
    // s_data[22]=0x00;
    // s_data[23]=0x00;
    // s_data[24]=0x80;
    // s_data[25]=0x3F;// 速度
    // s_data[26]=0x9E;
    // s_data[27]=0x2F;
    // s_data[28]=0xDD; 
    // s_data[29]=0xBD;
    // s_data[30]=0x17;
    // s_data[31]=0x5C;
    // s_data[32]=0xD8;
    // s_data[33]=0xA2; 
    // s_data[34]=0x19;
    // s_data[35]=0x72;
    // s_data[36]=0xC9;
    // s_data[37]=0xF3;
    // s_data[38]=0xB4;


       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
      // printf("333x:%d,y:%d\n",x,y);
    }


     else if (strlen(uartReadBuff)!=0){
         
        x=a1_val + (c1_val-a1_val) /2;
        y=a2_val + (b2_val-a2_val) /2;
        // printf("444x:%d,y:%d\n",x,y);
    
    if (x>810 && x < 1110 && y>460 && y < 620) //如果到达位置时就让小车停，占总面积的15%会停
    {
       Angle_Block_Assignment(); // 重新设置 s_data 数组内容
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
    }



  
    if (x>0 && x < 810 && y>0 && y < 540) //4个轮子转，但是00和02方向一致，01和03方向一致.向左前走
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x00;
    s_data[10]=0xC0; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x80;
    s_data[15]=0x3F; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x00;
    s_data[20]=0xC0; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x80;
    s_data[25]=0x3F;// 速度
    s_data[26]=0x9E;
    s_data[27]=0x2F;
    s_data[28]=0xDD; 
    s_data[29]=0xBD;
    s_data[30]=0x17;
    s_data[31]=0x5C;
    s_data[32]=0xD8;
    s_data[33]=0xA2; 
    s_data[34]=0x19;
    s_data[35]=0x72;
    s_data[36]=0xC9;
    s_data[37]=0xF3;
    s_data[38]=0xB4;
      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
      // printf("s_data:%s\n",s_data);
    }
    if (x>0 && x < 810 && y<1080 && y > 540) //向左后走
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x00;
    s_data[10]=0x40; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x80;
    s_data[15]=0xBF; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x00;
    s_data[20]=0x40; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x80;
    s_data[25]=0xBF;// 速度
    s_data[26]=0xAA;
    s_data[27]=0x62;
    s_data[28]=0xF7; 
    s_data[29]=0xAC;
    s_data[30]=0x18;
    s_data[31]=0xA9;
    s_data[32]=0xA0;
    s_data[33]=0xE3; 
    s_data[34]=0x7C;
    s_data[35]=0x06;
    s_data[36]=0xCC;
    s_data[37]=0x09;
    s_data[38]=0x44;
      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
      // printf("s_data:%s\n",s_data);
       
    }
    if (x<1920&& x > 1110 && y>0&& y < 540) //向右前
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容

    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x80;
    s_data[10]=0xBF; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x00;
    s_data[15]=0x40; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x80;
    s_data[20]=0xBF; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x00;
    s_data[25]=0x40;// 速度
    s_data[26]=0xC7;
    s_data[27]=0x8A;
    s_data[28]=0x7C; 
    s_data[29]=0x94;
    s_data[30]=0xAA;
    s_data[31]=0xF3;
    s_data[32]=0x4D;
    s_data[33]=0x6D; 
    s_data[34]=0x9B;
    s_data[35]=0x2E;
    s_data[36]=0x55;
    s_data[37]=0xE1;
    s_data[38]=0x87;
      

      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
      // printf("s_data:%s\n",s_data);
    }
     if (x > 1110 && x<1920 && y > 540&& y<1080) //向右后
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x80;
    s_data[10]=0x3F; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x00;
    s_data[15]=0xC0; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x80;
    s_data[20]=0x3F; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x00;
    s_data[25]=0xC0;// 速度
    s_data[26]=0xF3;
    s_data[27]=0xC7;
    s_data[28]=0x56; 
    s_data[29]=0x85;
    s_data[30]=0xA5;
    s_data[31]=0x06;
    s_data[32]=0x35;
    s_data[33]=0x2C; 
    s_data[34]=0xFE;
    s_data[35]=0x5A;
    s_data[36]=0x50;
    s_data[37]=0x1B;
    s_data[38]=0x77;
      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
       //printf("s_data:%s\n",s_data);
    }
    if (x > 810 && x<1110 && y > 0 && y<460) //前进
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x00;
    s_data[10]=0xC0; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x00;
    s_data[15]=0x40; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x00;
    s_data[20]=0xC0; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x00;
    s_data[25]=0x40;// 速度
    s_data[26]=0x4A;
    s_data[27]=0x01;
    s_data[28]=0x12; 
    s_data[29]=0x77;
    s_data[30]=0xA6;
    s_data[31]=0xC0;
    s_data[32]=0x2D;
    s_data[33]=0xCE; 
    s_data[34]=0x45;
    s_data[35]=0xE4;
    s_data[36]=0xA6;
    s_data[37]=0xDE;
    s_data[38]=0x47;
      

      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
      // printf("s_data:%s\n",s_data);
    }


     if (x > 810 && x<1110 && y > 620&& y<1080) //后退
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容
    s_data[0]=0xAA;
    s_data[1]=0x55;//帧头
    s_data[2]=0x03;//功能码
    s_data[3]=0x22;//数据长度
    s_data[4]=0x01;//子命令
    s_data[5]=0x04;//电机数量
    s_data[6]=0x00;//1号电机ID号
    s_data[7]=0x00;
    s_data[8]=0x00;
    s_data[9]=0x00;
    s_data[10]=0x40; // 速度
    s_data[11]=0x01;//2号电机ID号
    s_data[12]=0x00;
    s_data[13]=0x00;
    s_data[14]=0x00;
    s_data[15]=0xC0; // 速度
    s_data[16]=0x02;//3号电机ID号
    s_data[17]=0x00;
    s_data[18]=0x00;
    s_data[19]=0x00;
    s_data[20]=0x40; // 速度
    s_data[21]=0x03;//4号电机ID号
    s_data[22]=0x00;
    s_data[23]=0x00;
    s_data[24]=0x00;
    s_data[25]=0xC0;// 速度
    s_data[26]=0x7E;
    s_data[27]=0x4C;
    s_data[28]=0x38; 
    s_data[29]=0x66;
    s_data[30]=0xA9;
    s_data[31]=0x35;
    s_data[32]=0x55;
    s_data[33]=0x8F; 
    s_data[34]=0x20;
    s_data[35]=0x90;
    s_data[36]=0xA3;
    s_data[37]=0x24;
    s_data[38]=0xB7;
      
      

      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       usleep(U_SLEEP_TIME);
       //printf("s_data:%s\n",s_data);
    }
}

    }
    else if (status==2)//上位机控制小车停
    {
    Angle_Block_Assignment(); // 重新设置 s_data 数组内容

      
       IoTUartWrite(HI_UART_IDX_2, s_data, sizeof(s_data)); // 将数组通过UART发送出去
       U_SLEEP_TIME = 3500;
      // printf("s_data:%s\n",s_data);
    }
    usleep(U_SLEEP_TIME);//开门狗OK

     //char uartReadBuff[UART_BUFF_SIZE]={};
     
    }



}
//线程二：串口控制 

















// void UartExampleEntry(void)
// {
//     osThreadAttr_t attr;
//     IoTWatchDogDisable();

//     attr.name = "Uart1_Direction_Control";
//     attr.attr_bits = 0U;
//     attr.cb_mem = NULL;
//     attr.cb_size = 0U;
//     attr.stack_mem = NULL;
//     attr.stack_size = 5 * 1024; // 任务栈大小*1024 
//     attr.priority = osPriorityNormal;

//     if (osThreadNew((osThreadFunc_t)Uart1_Direction_Control, NULL, &attr) == NULL) {
//         printf("[Uart1_Direction_Control] Failed to create Uart1_Direction_Control!\n");
//     }
// }

//APP_FEATURE_INIT(UartExampleEntry);


