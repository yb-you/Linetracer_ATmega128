
#include <mega128.h>
#include <delay.h>
#include <lcd.h>		          //lcd ����ϱ�����
#include <stdio.h>		          //sprint ����ϱ�����

#asm
    .equ __lcd_port = 0x1B;               //LCD��Ʈ A ����
#endasm
#define ADC_VREF_TYPE 0x40		  //�ܺ��� AVCC���ڷ� �Էµ� ������ ���

unsigned char adc[16];                    //lcd����� ���� ���ڿ�
unsigned char led = 0xff;                 //led�׻� ����� ���� 0xff�ʱ�ȭ
int motor_state = 0;                      //���ͽ���ġ����
int sampling_state = 0;                   //���ø� ����ġ ����
int ad_max[8]={0,0,0,0,0,0,0,0}, ad_min[8]={1000,1000,1000,1000,1000,1000,1000,1000};      //�ִ�,�ּҰ� �迭 ���� �� �ʱ�ȭ
int i;                                    //for���� ���Ǵ� ���� (�ִ�, �ּ� ����)
int n;                                    //for���� ���Ǵ� ���� (temp�� ����)
const int weight[8]={-100,-60,-45,-20,20,45,60,100};      //����ġ
int temp[8]={0};                          //����ġ ���Ŀ� ���Ǵ� ����
int sum_position=0;                       //
int weight_position=0;                    //
int position=0;                           // �����ǰ�
unsigned char motor_R[4]={0x99,0xaa,0x66,0x55};   //R���� �迭
unsigned char motor_L[4]={0x99,0xaa,0x66,0x55};   //L���� �迭
unsigned char speed_R[]={255,200, 125};           //R���Ϳ� ���Ǵ� ctc �ֱ� �迭                                                                                         
unsigned char speed_LH[]={0xFF,0xFF, 0xFF};       //L���Ϳ� ���Ǵ� overflow high�ֱ� �迭 
unsigned char speed_LL[]={0x01,0x38, 0x83};       //L���Ϳ� ���Ǵ� overflow low�ֱ� �迭 
int index_speedR;                                 //speed_R�迭�� ���Ǵ� �ε��� ����
int index_speedLH;                                //speed_LH�迭�� ���Ǵ� �ε��� ����
int index_speedLL;                                //speed_LL�迭�� ���Ǵ� �ε��� ����
int r=0;                                      //motor_R�迭�� ���Ǵ� �ε��� ����
int l=0;                                      //motor_L�迭�� ���Ǵ� �ε��� ����
void lcd_adc(int sampling_state);                 //���簪 ǥ�� ����� ���� �Լ�
void straight();                                  //2�������� ���� ����� ���� �Լ�
int straight_time=0;                              //�����ϴ½ð� ����
int straight_ctcT=0;                              //4�� ���� ���� ctc�ݺ�Ƚ��
int straight_ctc=0;                               //�������� ctc�ݺ�Ƚ��
int straight_distance=0;                          //�ӵ� ���� �� ���� ����Ÿ�
int drive=1;                                      //����Ƚ��
int straight_next=0;                              //1�� ���� ���� �������� ����  
interrupt [EXT_INT7] void ext_int7_isr(void){          //7������ġ ���   (���� ���� ����)
    delay_ms(300);
    motor_state++;                                         //motor_state���� ����
}

interrupt [EXT_INT6] void ext_int6_isr(void){          //6������ġ ���   (���ø� ����ġ ����)
   delay_ms(300);
   sampling_state++;                                      //sampling_state���� ����
   lcd_init(16);                                          //lcd �ʱ�ȭ
   lcd_clear();
}
unsigned int read_adc(unsigned char adc_input){
    ADMUX = adc_input | (ADC_VREF_TYPE);               //7��Ʈ �Է��� ����
    
    delay_us(5);                                     //lcd���� �ӵ�
    
    ADCSRA|= 0x40;                                    //�Ƴ��α׸� �����з� ��ȯ(0x40 = ��ȯ����)
    while((ADCSRA & 0x10)==0);                        //��ȯ �Ϸ�(0x10 = ��ȯ �Ϸ�)
    ADCSRA|=0x10;                                     //��ȯ �Ϸ� ���ͷ�Ʈ ��û(�����ص� ���۰�������)
    return ADCW;                                      //�Ƴ��αװ� �����з� ��ȯ�� ��(=�ܱؼ���)  
}
interrupt [TIM1_COMPA] void timer_compa_isr()     //ctc (R_motor)
{ 
   TCNT1H=0x00;                                     //TCNT1�ʱⰪ 0x00����                                    
   TCNT1L=0x00; 
            
   if((motor_state%2)==1){                         //��������(�۵�)
      while(1){                                        //while���� ���� �ֱⰡ �ٲ� �Ŀ� break�� Ż��
           if(position==-1){break;}                       //position==-1�϶� ������ ����                                   
           if(position<25){                               //position<25�϶�(����,��ȸ��)
              index_speedR=0;                               //OCR1A=255(0.004��)
              if(straight_next==1){                           //��������� �����϶�    
                        index_speedR=2;                             //�ӵ� ���� 
                        straight_distance++;                        //��������Ÿ� ����                                           
                        if(straight_distance>(straight_ctc+1000)/2){ index_speedR=0; break;}  //��������Ÿ��� �����Ÿ��� ���̻� ������ ���� �ӵ���
                        break; }
              straight_time++;                              //�����ϴ� �ð�++
              break;}         
           else if(position<50){                          //25<position<50�϶�(����ȸ��)
             index_speedR=1;  break;}                        //OCR1A=200(0.003��)         
           else{                                          //50<=position�϶�(����ȸ��)
             index_speedR=2;                                  //OCR1A=125(0.002��)
             straight_time=0;                             //�����ϴ� �ð� �ʱ�ȭ
             break;}                                                   
           }
     OCR1A=speed_R[index_speedR];                     //OCR1A �ֱ� ����                                                                                                                                             
     PORTB=motor_R[(r++)%4];                          //R���� ȸ��
   }                                      
   else {}                                         //��������(����)
   
}   

interrupt [TIM3_OVF] void timer3_ovf_isr()     //�����÷ο�   (L_motor)   
{               
   if((motor_state%2)==1){                         //��������(�۵�)
      while(1){                                      //while���� ���� �ֱⰡ �ٲ� �Ŀ� break�� Ż��
           if(position==-1){ break;}                 //position==-1�϶� ������ ����
           if(-25<position){                         //position<25�϶�(����,��ȸ��)
               index_speedLH=0;                       //TCNT1H=0xFF (0.004��)
               index_speedLL=0;                       //TCNT1L=0x01
               if(straight_next==1){                    //�������� �� �����϶�      
                         index_speedLH=2;                    //�ӵ�����
                         index_speedLL=2; 
                         if(straight_distance>(straight_ctc+1000)/2){ index_speedLH=0; index_speedLL=0; break;}//�����Ÿ��� ���̻� ������ ���� �ӵ���
                         break;}                  
                     break;}        
           else if(-50<position){               //-50<position<-25�϶�(���ȸ��)            
              index_speedLH=1;                     //TCNT1H=0xFF (0.003��)
              index_speedLL=1; break;}             //TCNT1L=0x38 
           else{                                //position<=-50�϶�(����ȸ��)
              index_speedLH=2;                       //TCNT1H=0xFF (0.002��)
              index_speedLL=2;                       //TCNT1L=0x83
              straight_time=0; break;                //�����ϴ� �ð� �ʱ�ȭ
              }    
           }
     TCNT3H=speed_LH[index_speedLH];       //TCNT3H �ֱ� ����
     TCNT3L=speed_LL[index_speedLL];       //TCNT3L �ֱ� ����                                                                                                                                
     PORTC=motor_L[(l++)%4];               //L���� ȸ��
   }
   else {}                               //��������(����)
}
void main(){
    DDRA=0xff;                        //(lcd�� ������)A��Ʈ ������� ����
    PORTA=0xff;                       
    
    DDRB=0xff;                       //(R���͸� ������) B��Ʈ ������� ����
    DDRC=0xff;                       //(L���͸� ������) C��Ʈ ������� ����
    
    DDRD =0xff;                      //(led�� ������)D��Ʈ ������� ����
    PORTD =led;                      //D��Ʈ led�� ����(0xff): �׻���

    DDRE = 0x00;                     //(����ġ�� ������)E��Ʈ �Է����� ����
    PORTE = 0x00;
       
    EIMSK=0xc0;                      //7��,6�� ����ġ ���
    EICRB=0x00;                      // lowƮ���� ���
          
    DDRF = 0x00;                     //(������ ������)F��Ʈ �Է����� ����
    PORTF =0x00;                                                 
    
    TIMSK=0x10;                      //timer/counter intuerrup mask register
    TCCR1A=0x00;
    TCCR1B=0x0c;                     //ctc��� ���ֺ� 256(ctc��� 0x08+ 256���ֺ� 0x04) 
    TCNT1=0x00;
    
    TCNT3H=0x00;
    TCNT3L=0x00;   
    ETIMSK=0x04;                     // 3��Ÿ�̸�          
    TCCR3A=0x00;                   
    TCCR3B=0x04;                     // 256����

    ADMUX = ADC_VREF_TYPE;
    ADCSRA=0x86;                                     
    lcd_init(16);                    //lcd �ʱ�ȭ
    lcd_clear();                     //lcd ���� �����

    #asm("sei")
        
    while(1){
        switch((sampling_state%6)) {
         case 0:                                     //case 0:���� ������ ǥ��
          lcd_adc(sampling_state);                       //lcd_adc(int sampling_state): ���簪 ǥ�� ����� ���� �Լ�
          break;
          
         case 1:                                     //case 1:"sampling ǥ��"
          lcd_init(16);
          lcd_clear();
                     
          lcd_gotoxy(0,0);                                //(0,0)��ǥ����
          lcd_puts("sampling...");                        //"sampling..."�������
          sprintf(adc,"SW%d",sampling_state%6);          
          lcd_gotoxy(12,1);                               //(12,1)��ǥ����
          lcd_puts(adc);                                  //"SW1"�������
  
          break;
          
         case 2:                                     //case 2:���� ������ ǥ�� & �ִ� �ּҰ� ���
          lcd_adc(sampling_state%6);                      //lcd_adc(int sampling_state): ���簪 ǥ�� ����� ���� �Լ�
          
          for(i=0;i<8;i++){                               //������ �ִ�, �ּ� ���
             if(ad_max[i]<read_adc(i)) ad_max[i]=read_adc(i);
             if(ad_min[i]>read_adc(i)) ad_min[i]=read_adc(i);
          }                           
          break;
                                                     
         case 3:                                     //case3: �ִ� ���
          
          sprintf(adc,"%3d %3d %3d %3d", ad_max[0], ad_max[1], ad_max[2], ad_max[3] );   //adc���ڿ� ���� (����0~3 �ִ�)
          lcd_gotoxy(0,0);                               //(0,0)��ǥ����
          lcd_puts(adc);                                 //lcd�� adc���ڿ� ���
          
          sprintf(adc,"%3d %3d %3d SW%d", ad_max[4], ad_max[5], ad_max[6], sampling_state%6 );   //adc���ڿ� ���� (����4~7 �ִ�)
          lcd_gotoxy(0,1);                               //(0,1)��ǥ����
          lcd_puts(adc);                                 //lcd�� adc���ڿ� ���
          break;
          
         case 4:                                     //case 4:�ּڰ� ǥ��

          sprintf(adc,"%3d %3d %3d %3d", ad_min[0], ad_min[1], ad_min[2], ad_min[3] );   //adc���ڿ� ���� (����0~3 �ּڰ�)
          lcd_gotoxy(0,0);                               //(0,0)��ǥ����
          lcd_puts(adc);                                 //lcd�� adc���ڿ� ���

          sprintf(adc,"%3d %3d %3d SW%d", ad_min[4], ad_min[5], ad_min[6], sampling_state%6 );   //adc���ڿ� ���� (����4~6 �ּڰ�,SW4)
          lcd_gotoxy(0,1);                               //(0,1)��ǥ����
          lcd_puts(adc);                                 //lcd�� adc���ڿ� ���
          break;                  
         case 5:                                    //case 5:����ġ ��� & ǥ��
          lcd_init(16);
          lcd_clear();
          for(n=1;n<7;n++){                              //����ġ ���
            temp[n]=((read_adc(n)/(ad_max[n]-ad_min[n]))*100);}
          sum_position=temp[1]+temp[2]+temp[3]+temp[4]+temp[5]+temp[6];
            temp[1]*=weight[1]; temp[2]*=weight[2];
            temp[3]*=weight[3]; temp[4]*=weight[4];
            temp[5]*=weight[5]; temp[6]*=weight[6];
                        
          weight_position=temp[1]+temp[2]+temp[3]+temp[4]+temp[5]+temp[6];
                                             
         position=(weight_position/sum_position);

          lcd_gotoxy(0,0);                               //(0,0)��ǥ����
          sprintf(adc,"%3d",position);                                  
          lcd_puts(adc);                                 //lcd�� position�� ���  
          sprintf(adc,"SW%d",sampling_state%6);          
          lcd_gotoxy(12,1);                              //(12,1)��ǥ����
          lcd_puts(adc);                                 //lcd��"SW5"���  
          sprintf(adc,"Drive%d",drive);          
          lcd_gotoxy(9,0);
          lcd_puts(adc); 
          if((motor_state%2)==1){                        //���ͱ�����
          straight();}                                      //2�������� ���� straight()����������Լ� ����
         break;
        }  
    };
}
void lcd_adc(int sampling_state){               //���簪 ��� ����� ���� �Լ�
          lcd_gotoxy(0,0);                          //��ǥ���� 
         sprintf(adc,"%3d %3d %3d %3d", read_adc(0), read_adc(1), read_adc(2), read_adc(3) );   //adc���ڿ� ���� (����0~3 ������)
          lcd_puts(adc);                            //lcd�� adc���ڿ� ���
                      
          lcd_gotoxy(0,1);                          //�ʱ� ��ǥ����
          sprintf(adc,"%3d %3d %3d SW%d", read_adc(4), read_adc(5), read_adc(6), sampling_state%6);   //adc���ڿ� ���� (����4~7 ������)
          lcd_puts(adc);                            //lcd�� adc���ڿ� ���
}
void straight(){                                //2�������� ���� ����� ���� �Լ�
    if(drive==1){                               //1�������
       if(straight_time>1000){                     //4�� �̻� �����ϸ�      
           if(straight_time<1050){r=0;}              //��� �ݺ��ϴ��� �˱� ���� r=0 ����           
           straight_ctcT=r;                        //�׶� r��(ctc�ݺ� Ƚ��) ���� straight_ctcT�� ���� 
           if(straight_ctcT>750){                  //3���̻� ���� �ԷµǸ�       (������ �� 8�� 1050+750=1800->7.2��)
               straight_ctc=straight_ctcT;          //+1000:�������� ctc�ݺ�Ƚ��
               straight_time=0;                     //�����Ϸ� �� straight_time(��������ð�)�ʱ�ȭ
               drive=2;                             //�������� ��� �� 2���������� ����      
           }          
       }
    }
    else if(drive==2){                         //2�������
       if(straight_time>500){                     //2���̻� �����ϸ�
             straight_next=1; }                      //������������ �ν�
       else{ straight_next=0;}                    //2���̻� �������� ������ ������������ �ν�X
       }                                                                               
}