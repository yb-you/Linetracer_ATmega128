
#include <mega128.h>
#include <delay.h>
#include <lcd.h>		          //lcd 사용하기위해
#include <stdio.h>		          //sprint 사용하기위해

#asm
    .equ __lcd_port = 0x1B;               //LCD포트 A 설정
#endasm
#define ADC_VREF_TYPE 0x40		  //외부의 AVCC단자로 입력된 전압을 사용

unsigned char adc[16];                    //lcd출력을 위한 문자열
unsigned char led = 0xff;                 //led항상 출력을 위해 0xff초기화
int motor_state = 0;                      //모터스위치상태
int sampling_state = 0;                   //샘플링 스위치 상태
int ad_max[8]={0,0,0,0,0,0,0,0}, ad_min[8]={1000,1000,1000,1000,1000,1000,1000,1000};      //최대,최소값 배열 선언 및 초기화
int i;                                    //for문에 사용되는 변수 (최대, 최소 결정)
int n;                                    //for문에 사용되는 변수 (temp값 결정)
const int weight[8]={-100,-60,-45,-20,20,45,60,100};      //가중치
int temp[8]={0};                          //가중치 공식에 사용되는 변수
int sum_position=0;                       //
int weight_position=0;                    //
int position=0;                           // 포지션값
unsigned char motor_R[4]={0x99,0xaa,0x66,0x55};   //R모터 배열
unsigned char motor_L[4]={0x99,0xaa,0x66,0x55};   //L모터 배열
unsigned char speed_R[]={255,200, 125};           //R모터에 사용되는 ctc 주기 배열                                                                                         
unsigned char speed_LH[]={0xFF,0xFF, 0xFF};       //L모터에 사용되는 overflow high주기 배열 
unsigned char speed_LL[]={0x01,0x38, 0x83};       //L모터에 사용되는 overflow low주기 배열 
int index_speedR;                                 //speed_R배열에 사용되는 인덱스 변수
int index_speedLH;                                //speed_LH배열에 사용되는 인덱스 변수
int index_speedLL;                                //speed_LL배열에 사용되는 인덱스 변수
int r=0;                                      //motor_R배열에 사용되는 인덱스 변수
int l=0;                                      //motor_L배열에 사용되는 인덱스 변수
void lcd_adc(int sampling_state);                 //현재값 표시 사용자 정의 함수
void straight();                                  //2차주행을 위한 사용자 정의 함수
int straight_time=0;                              //직진하는시간 변수
int straight_ctcT=0;                              //4초 직진 이후 ctc반복횟수
int straight_ctc=0;                               //직진동안 ctc반복횟수
int straight_distance=0;                          //속도 증가 후 직진 진행거리
int drive=1;                                      //주행횟수
int straight_next=0;                              //1차 주행 이후 직진시작 변수  
interrupt [EXT_INT7] void ext_int7_isr(void){          //7번스위치 사용   (모터 상태 제어)
    delay_ms(300);
    motor_state++;                                         //motor_state변수 증가
}

interrupt [EXT_INT6] void ext_int6_isr(void){          //6번스위치 사용   (샘플링 스위치 제어)
   delay_ms(300);
   sampling_state++;                                      //sampling_state변수 증가
   lcd_init(16);                                          //lcd 초기화
   lcd_clear();
}
unsigned int read_adc(unsigned char adc_input){
    ADMUX = adc_input | (ADC_VREF_TYPE);               //7포트 입력을 설정
    
    delay_us(5);                                     //lcd변경 속도
    
    ADCSRA|= 0x40;                                    //아날로그를 디지털로 변환(0x40 = 변환시작)
    while((ADCSRA & 0x10)==0);                        //변환 완료(0x10 = 변환 완료)
    ADCSRA|=0x10;                                     //변환 완료 인터럽트 요청(생략해도 동작가능했음)
    return ADCW;                                      //아날로그가 디지털로 변환한 값(=단극성값)  
}
interrupt [TIM1_COMPA] void timer_compa_isr()     //ctc (R_motor)
{ 
   TCNT1H=0x00;                                     //TCNT1초기값 0x00설정                                    
   TCNT1L=0x00; 
            
   if((motor_state%2)==1){                         //모터제어(작동)
      while(1){                                        //while문을 통해 주기가 바뀐 후에 break로 탈출
           if(position==-1){break;}                       //position==-1일때 이전값 유지                                   
           if(position<25){                               //position<25일때(직진,우회전)
              index_speedR=0;                               //OCR1A=255(0.004초)
              if(straight_next==1){                           //다음주행시 직진일때    
                        index_speedR=2;                             //속도 증가 
                        straight_distance++;                        //직진진행거리 증가                                           
                        if(straight_distance>(straight_ctc+1000)/2){ index_speedR=0; break;}  //직진진행거리가 직진거리의 반이상 지나면 원래 속도로
                        break; }
              straight_time++;                              //직진하는 시간++
              break;}         
           else if(position<50){                          //25<position<50일때(약좌회전)
             index_speedR=1;  break;}                        //OCR1A=200(0.003초)         
           else{                                          //50<=position일때(강좌회전)
             index_speedR=2;                                  //OCR1A=125(0.002초)
             straight_time=0;                             //직진하는 시간 초기화
             break;}                                                   
           }
     OCR1A=speed_R[index_speedR];                     //OCR1A 주기 설정                                                                                                                                             
     PORTB=motor_R[(r++)%4];                          //R모터 회전
   }                                      
   else {}                                         //모터제어(정지)
   
}   

interrupt [TIM3_OVF] void timer3_ovf_isr()     //오버플로우   (L_motor)   
{               
   if((motor_state%2)==1){                         //모터제어(작동)
      while(1){                                      //while문을 통해 주기가 바뀐 후에 break로 탈출
           if(position==-1){ break;}                 //position==-1일때 이전값 유지
           if(-25<position){                         //position<25일때(직진,우회전)
               index_speedLH=0;                       //TCNT1H=0xFF (0.004초)
               index_speedLL=0;                       //TCNT1L=0x01
               if(straight_next==1){                    //다음주행 시 직진일때      
                         index_speedLH=2;                    //속도증가
                         index_speedLL=2; 
                         if(straight_distance>(straight_ctc+1000)/2){ index_speedLH=0; index_speedLL=0; break;}//직진거리의 반이상 지나면 원래 속도로
                         break;}                  
                     break;}        
           else if(-50<position){               //-50<position<-25일때(약우회전)            
              index_speedLH=1;                     //TCNT1H=0xFF (0.003초)
              index_speedLL=1; break;}             //TCNT1L=0x38 
           else{                                //position<=-50일때(강우회전)
              index_speedLH=2;                       //TCNT1H=0xFF (0.002초)
              index_speedLL=2;                       //TCNT1L=0x83
              straight_time=0; break;                //직진하는 시간 초기화
              }    
           }
     TCNT3H=speed_LH[index_speedLH];       //TCNT3H 주기 설정
     TCNT3L=speed_LL[index_speedLL];       //TCNT3L 주기 설정                                                                                                                                
     PORTC=motor_L[(l++)%4];               //L모터 회전
   }
   else {}                               //모터제어(정지)
}
void main(){
    DDRA=0xff;                        //(lcd를 연결한)A포트 출력으로 설정
    PORTA=0xff;                       
    
    DDRB=0xff;                       //(R모터를 연결한) B포트 출력으로 설정
    DDRC=0xff;                       //(L모터를 연결한) C포트 출력으로 설정
    
    DDRD =0xff;                      //(led를 연결한)D포트 출력으로 설정
    PORTD =led;                      //D포트 led값 설정(0xff): 항상켬

    DDRE = 0x00;                     //(스위치를 연결한)E포트 입력으로 설정
    PORTE = 0x00;
       
    EIMSK=0xc0;                      //7번,6번 스위치 사용
    EICRB=0x00;                      // low트리거 사용
          
    DDRF = 0x00;                     //(센서를 연결한)F포트 입력으로 설정
    PORTF =0x00;                                                 
    
    TIMSK=0x10;                      //timer/counter intuerrup mask register
    TCCR1A=0x00;
    TCCR1B=0x0c;                     //ctc모드 분주비 256(ctc모드 0x08+ 256분주비 0x04) 
    TCNT1=0x00;
    
    TCNT3H=0x00;
    TCNT3L=0x00;   
    ETIMSK=0x04;                     // 3번타이머          
    TCCR3A=0x00;                   
    TCCR3B=0x04;                     // 256분주

    ADMUX = ADC_VREF_TYPE;
    ADCSRA=0x86;                                     
    lcd_init(16);                    //lcd 초기화
    lcd_clear();                     //lcd 글자 지우기

    #asm("sei")
        
    while(1){
        switch((sampling_state%6)) {
         case 0:                                     //case 0:현재 센서값 표시
          lcd_adc(sampling_state);                       //lcd_adc(int sampling_state): 현재값 표시 사용자 정의 함수
          break;
          
         case 1:                                     //case 1:"sampling 표시"
          lcd_init(16);
          lcd_clear();
                     
          lcd_gotoxy(0,0);                                //(0,0)좌표설정
          lcd_puts("sampling...");                        //"sampling..."문자출력
          sprintf(adc,"SW%d",sampling_state%6);          
          lcd_gotoxy(12,1);                               //(12,1)좌표설정
          lcd_puts(adc);                                  //"SW1"문자출력
  
          break;
          
         case 2:                                     //case 2:현재 센서값 표시 & 최대 최소값 계산
          lcd_adc(sampling_state%6);                      //lcd_adc(int sampling_state): 현재값 표시 사용자 정의 함수
          
          for(i=0;i<8;i++){                               //센서값 최대, 최소 계산
             if(ad_max[i]<read_adc(i)) ad_max[i]=read_adc(i);
             if(ad_min[i]>read_adc(i)) ad_min[i]=read_adc(i);
          }                           
          break;
                                                     
         case 3:                                     //case3: 최댓값 출력
          
          sprintf(adc,"%3d %3d %3d %3d", ad_max[0], ad_max[1], ad_max[2], ad_max[3] );   //adc문자열 설정 (센서0~3 최댓값)
          lcd_gotoxy(0,0);                               //(0,0)좌표설정
          lcd_puts(adc);                                 //lcd에 adc문자열 출력
          
          sprintf(adc,"%3d %3d %3d SW%d", ad_max[4], ad_max[5], ad_max[6], sampling_state%6 );   //adc문자열 설정 (센서4~7 최댓값)
          lcd_gotoxy(0,1);                               //(0,1)좌표설정
          lcd_puts(adc);                                 //lcd에 adc문자열 출력
          break;
          
         case 4:                                     //case 4:최솟값 표시

          sprintf(adc,"%3d %3d %3d %3d", ad_min[0], ad_min[1], ad_min[2], ad_min[3] );   //adc문자열 설정 (센서0~3 최솟값)
          lcd_gotoxy(0,0);                               //(0,0)좌표설정
          lcd_puts(adc);                                 //lcd에 adc문자열 출력

          sprintf(adc,"%3d %3d %3d SW%d", ad_min[4], ad_min[5], ad_min[6], sampling_state%6 );   //adc문자열 설정 (센서4~6 최솟값,SW4)
          lcd_gotoxy(0,1);                               //(0,1)좌표설정
          lcd_puts(adc);                                 //lcd에 adc문자열 출력
          break;                  
         case 5:                                    //case 5:가중치 계산 & 표시
          lcd_init(16);
          lcd_clear();
          for(n=1;n<7;n++){                              //가중치 계산
            temp[n]=((read_adc(n)/(ad_max[n]-ad_min[n]))*100);}
          sum_position=temp[1]+temp[2]+temp[3]+temp[4]+temp[5]+temp[6];
            temp[1]*=weight[1]; temp[2]*=weight[2];
            temp[3]*=weight[3]; temp[4]*=weight[4];
            temp[5]*=weight[5]; temp[6]*=weight[6];
                        
          weight_position=temp[1]+temp[2]+temp[3]+temp[4]+temp[5]+temp[6];
                                             
         position=(weight_position/sum_position);

          lcd_gotoxy(0,0);                               //(0,0)좌표설정
          sprintf(adc,"%3d",position);                                  
          lcd_puts(adc);                                 //lcd에 position값 출력  
          sprintf(adc,"SW%d",sampling_state%6);          
          lcd_gotoxy(12,1);                              //(12,1)좌표설정
          lcd_puts(adc);                                 //lcd에"SW5"출력  
          sprintf(adc,"Drive%d",drive);          
          lcd_gotoxy(9,0);
          lcd_puts(adc); 
          if((motor_state%2)==1){                        //모터구동시
          straight();}                                      //2차주행을 위한 straight()사용자정의함수 실행
         break;
        }  
    };
}
void lcd_adc(int sampling_state){               //현재값 출력 사용자 정의 함수
          lcd_gotoxy(0,0);                          //좌표설정 
         sprintf(adc,"%3d %3d %3d %3d", read_adc(0), read_adc(1), read_adc(2), read_adc(3) );   //adc문자열 설정 (센서0~3 측정값)
          lcd_puts(adc);                            //lcd에 adc문자열 출력
                      
          lcd_gotoxy(0,1);                          //초기 좌표설정
          sprintf(adc,"%3d %3d %3d SW%d", read_adc(4), read_adc(5), read_adc(6), sampling_state%6);   //adc문자열 설정 (센서4~7 측정값)
          lcd_puts(adc);                            //lcd에 adc문자열 출력
}
void straight(){                                //2차주행을 위한 사용자 정의 함수
    if(drive==1){                               //1차주행시
       if(straight_time>1000){                     //4초 이상 직진하면      
           if(straight_time<1050){r=0;}              //몇번 반복하는지 알기 위해 r=0 설정           
           straight_ctcT=r;                        //그때 r값(ctc반복 횟수) 변수 straight_ctcT에 저장 
           if(straight_ctcT>750){                  //3초이상 값이 입력되면       (직진시 약 8초 1050+750=1800->7.2초)
               straight_ctc=straight_ctcT;          //+1000:직진동안 ctc반복횟수
               straight_time=0;                     //설정완료 후 straight_time(직진경과시간)초기화
               drive=2;                             //직진구간 계산 후 2차주행으로 간주      
           }          
       }
    }
    else if(drive==2){                         //2차주행시
       if(straight_time>500){                     //2초이상 직진하면
             straight_next=1; }                      //직진구간으로 인식
       else{ straight_next=0;}                    //2초이상 직진하지 않으면 직진구간으로 인식X
       }                                                                               
}